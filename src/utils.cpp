#include "utils.hpp"

bool is_tty_stdout() {
    #if defined(__unix__) || defined(__APPLE__)
    return isatty(STDOUT_FILENO);
    #else
    return false;
    #endif
}

string sh_escape(const string &s) {
    string r;
    r.reserve(s.size() + 2);
    r.push_back('\'');
    for (char c : s) {
        if (c == '\'')
            r += "'\\''";
        else
            r.push_back(c);
    }
    r.push_back('\'');
    return r;
}

int run_shell_cmd(const string &cmd) {
    return std::system(cmd.c_str());
}

bool safe_backup_copy(const string &src, const string &dst, string &err) {
    int sfd = ::open(src.c_str(), O_RDONLY);
    if (sfd < 0) {
        return true;
    }

    int dfd = ::open(dst.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (dfd < 0) {
        dfd = ::open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (dfd < 0) {
            err = "backup open: " + string(std::strerror(errno));
            ::close(sfd);
            return false;
        }
    }

    char buf[4096];
    ssize_t r;
    while ((r = ::read(sfd, buf, sizeof(buf))) > 0) {
        ssize_t w = ::write(dfd, buf, (size_t)r);
        if (w != r) {
            err = "backup write: " + string(std::strerror(errno));
            ::close(sfd);
            ::close(dfd);
            return false;
        }
    }

    ::close(sfd);
    ::close(dfd);
    return true;
}

string home_path(){
    const char* h = getenv("HOME");
    if(!h) h = getenv("USERPROFILE");
    if(!h) return string(".");
    return string(h);
}

bool file_exists(const string& p){
    struct stat st{};
    return ::stat(p.c_str(),&st)==0;
}

string tedit_config_dir(){
    string base = home_path();
    string root = base + "/tedit-config";
    std::error_code ec;
    fs::create_directories(root, ec);
    string plugins = root + "/plugins";
    fs::create_directories(plugins, ec);
    string themes = root + "/themes";
    fs::create_directories(themes, ec);
    return root;
}

string tedit_plugins_dir(){
    return tedit_config_dir() + "/plugins";
}

string tedit_themes_dir(){
    return tedit_config_dir() + "/themes";
}

string trim_copy(const string& s){
    size_t i=0,j=s.size();
    while(i<j && std::isspace((unsigned char)s[i])) i++;
    while(j>i && std::isspace((unsigned char)s[j-1])) j--;
    return s.substr(i,j-i);
}

void rstrip_newline(string& s){
    while(!s.empty() && (s.back()=='\n'||s.back()=='\r')) s.pop_back();
}

string lower(string s){ 
    for(char& c: s) c=(char)std::tolower((unsigned char)c); 
    return s; 
}

bool parse_long(const string& s, long& out){
    if(s.empty()) return false;
    errno = 0;
    char *e=nullptr; long v=strtol(s.c_str(), &e, 10);
    if(errno == ERANGE) return false;
    if(e==s.c_str()||*e) { return false; }
    out = v;
    return true;
}

int digits_for(size_t n){ 
    int w=1; 
    while(n>=10){ n/=10; w++; } 
    return w; 
}