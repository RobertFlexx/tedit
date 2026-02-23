#include "fileio.hpp"
#include "utils.hpp"

void load_file(const string& path, Buffer& b){
    b.lines.clear();
    std::ifstream in(path);
    if(!in.good()){ b.dirty=false; return; }
    string line;
    while(std::getline(in,line)){ rstrip_newline(line); b.lines.push_back(line); }
    b.dirty=false;
}

static int fsync_dir_of(const string& path){
    string dir = path;
    auto pos = dir.find_last_of('/');
    if(pos==string::npos) dir = ".";
    else if(pos==0) dir = "/";
    else dir = dir.substr(0,pos);

    int dfd = open(dir.c_str(), O_RDONLY
    #ifdef O_DIRECTORY
    | O_DIRECTORY
    #endif
    );
    if(dfd<0) return -1;
    int rc = fsync(dfd);
    int e = errno;
    close(dfd);
    errno = e;
    return rc;
}

bool atomic_save_to_fd(FILE* tf, const Buffer& b, string& err){
    for(auto& L: b.lines){
        if(fputs(L.c_str(), tf)==EOF || fputc('\n', tf)==EOF){
            err=string("write: ")+strerror(errno); fclose(tf); return false;
        }
    }
    if(fflush(tf)!=0){
        err=string("flush: ")+strerror(errno); fclose(tf); return false;
    }
    if(fsync(fileno(tf))<0){
        err=string("fsync: ")+strerror(errno); fclose(tf); return false;
    }
    if(fclose(tf)!=0){
        err=string("close: ")+strerror(errno); return false;
    }
    return true;
}

static bool doas_move_into_place_secure(const string& tmp, const string& dest, string &err) {
    string inner = "mv " + sh_escape(tmp) + " " + sh_escape(dest) + " && sync";
    string cmd   = "doas sh -c " + sh_escape(inner);
    int rc = run_shell_cmd(cmd);
    if (rc != 0) {
        err = "doas move failed (exit " + std::to_string(rc) + ")";
        return false;
    }
    return true;
}

bool atomic_save(const string& path, const Buffer& b, bool backup, string& err){
    mode_t mode = 0644; struct stat st{};
    if(::stat(path.c_str(), &st)==0) mode = st.st_mode & 0777;

    if(backup && ::stat(path.c_str(), &st)==0){
        string berr;
        (void)safe_backup_copy(path, path+"~", berr);
    }

    string tmp = path+".tmp.XXXXXX";
    vector<char> tbuf(tmp.begin(), tmp.end()); tbuf.push_back('\0');
    int tfd = mkstemp(tbuf.data());
    if(tfd<0){ err=string("mkstemp: ")+strerror(errno); return false; }
    (void)fchmod(tfd, mode);
    FILE* tf = fdopen(tfd, "w");
    if(!tf){
        err=string("fdopen: ")+strerror(errno);
        close(tfd);
        unlink(tbuf.data());
        return false;
    }
    if(!atomic_save_to_fd(tf,b,err)){
        unlink(tbuf.data());
        return false;
    }

    if(::rename(tbuf.data(), path.c_str())<0){
        string err2;
        if(!doas_move_into_place_secure(tbuf.data(), path, err2)){
            err = "rename: " + string(strerror(errno)) + " ; " + err2;
            unlink(tbuf.data());
            return false;
        }
    }

    (void)fsync_dir_of(path);
    return true;
}