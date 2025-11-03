#define _POSIX_C_SOURCE 200809L
#define TEDIT_VERSION "2.0.0"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <sys/ioctl.h>
#endif
#include <filesystem>
#include <chrono>
#include <map>
#include <random>
#include <ctime>
#include <vector>

extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace fs = std::filesystem;
using std::string; using std::vector; using std::cout; using std::cerr; using std::endl;

struct Editor;
static Editor* g_editor = nullptr;

static int l_tedit_echo(lua_State* L);
static int l_tedit_command(lua_State* L);
static int l_tedit_print(lua_State* L);

/* ------------------------------------------------------------------ */
/*               SECURE HELPERS (NEW / HARDENED)                      */
/* ------------------------------------------------------------------ */

static inline bool is_tty_stdout() {
    #if defined(__unix__) || defined(__APPLE__)
    return isatty(STDOUT_FILENO);
    #else
    return false;
    #endif
}

/* shell-escape for single-quoted sh -c */
static string sh_escape(const string &s) {
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

/* run shell command that is ALREADY SAFE (we only call with escaped stuff) */
static int run_shell_cmd(const string &cmd) {
    return std::system(cmd.c_str());
}

/* copy file -> backup using low-level I/O, try O_CREAT|O_EXCL first */
static bool safe_backup_copy(const string &src, const string &dst, string &err) {
    int sfd = ::open(src.c_str(), O_RDONLY);
    if (sfd < 0) {
        /* nothing to backup, not fatal */
        return true;
    }

    int dfd = ::open(dst.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (dfd < 0) {
        /* if it exists, just overwrite it – still safe enough for an editor */
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

static inline string home_path(){
    const char* h = getenv("HOME");
    if(!h) h = getenv("USERPROFILE");
    if(!h) return string(".");
    return string(h);
}

static inline bool file_exists(const string& p){
    struct stat st{};
    return ::stat(p.c_str(),&st)==0;
}

static string tedit_config_dir(){
    string base = home_path();
    string root = base + "/tedit-config";
    std::error_code ec;
    fs::create_directories(root, ec);
    string plugins = root + "/plugins";
    fs::create_directories(plugins, ec);
    return root;
}

static string tedit_plugins_dir(){
    return tedit_config_dir() + "/plugins";
}

/* ------------------------------------------------------------------ */
/*                         ANSI / THEMES                              */
/* ------------------------------------------------------------------ */
static bool use_color(){
    return is_tty_stdout();
}
static const string C_RESET = "\033[0m";
static const string C_DIM   = "\033[2m";
static const string C_BOLD  = "\033[1m";
static const string C_GREEN = "\033[32m";
static const string C_RED   = "\033[31m";
static const string C_CYAN  = "\033[36m";
static const string C_YEL   = "\033[33m";
static const string C_BRIGHT_BLACK = "\033[90m";
static const string C_BRIGHT_WHITE = "\033[97m";
static const string C_BRIGHT_CYAN  = "\033[96m";
static const string C_BRIGHT_GREEN = "\033[92m";
static const string C_BRIGHT_YEL   = "\033[93m";
static const string C_BRIGHT_RED   = "\033[91m";
static const string C_MAGENTA      = "\033[35m";
static const string C_BRIGHT_MAGENTA = "\033[95m";

enum class Theme { Default, Dark, Neon, Matrix, Paper };

struct ThemePalette {
    string accent, ok, warn, err, dim;
    string prompt, input, gutter, title;
    string help_cmd, help_arg, help_text;
};

static ThemePalette palette_for(Theme t){
    if(!use_color()) return {"","","","","", "", "", "", "", "", "", ""};

    switch(t){
        case Theme::Dark:
            return {
                C_CYAN,
                C_GREEN,
                C_YEL,
                C_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_CYAN,
                C_BRIGHT_WHITE,
                C_BRIGHT_BLACK,
                C_BOLD + C_CYAN,
                C_BRIGHT_CYAN,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        case Theme::Neon:
            return {
                C_BRIGHT_MAGENTA,
                C_BRIGHT_GREEN,
                C_BRIGHT_YEL,
                C_BRIGHT_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_MAGENTA,
                C_BRIGHT_CYAN,
                C_BRIGHT_BLACK,
                C_BOLD + C_BRIGHT_MAGENTA,
                C_BRIGHT_MAGENTA,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        case Theme::Matrix:
            return {
                C_GREEN,
                C_BRIGHT_GREEN,
                C_YEL,
                C_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_GREEN,
                C_BRIGHT_GREEN,
                C_BRIGHT_BLACK,
                C_BOLD + C_GREEN,
                C_BRIGHT_GREEN,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        case Theme::Paper:
            return {
                C_BRIGHT_BLACK,
                C_GREEN,
                C_YEL,
                C_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BOLD + C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        default:
            return {
                C_CYAN,
                C_GREEN,
                C_YEL,
                C_RED,
                C_DIM,
                C_BRIGHT_CYAN,
                C_BRIGHT_WHITE,
                C_BRIGHT_BLACK,
                C_BOLD + C_CYAN,
                C_CYAN,
                C_DIM,
                C_DIM
            };
    }
}

/* ------------------------------------------------------------------ */
/*                              Helpers                               */
/* ------------------------------------------------------------------ */
static inline string trim_copy(const string& s){
    size_t i=0,j=s.size();
    while(i<j && std::isspace((unsigned char)s[i])) i++;
    while(j>i && std::isspace((unsigned char)s[j-1])) j--;
    return s.substr(i,j-i);
}
static inline void rstrip_newline(string& s){
    while(!s.empty() && (s.back()=='\n'||s.back()=='\r')) s.pop_back();
}
static inline string lower(string s){ for(char& c: s) c=(char)std::tolower((unsigned char)c); return s; }

/* SAFE parse_long (with errno check) */
static inline bool parse_long(const string& s, long& out){
    if(s.empty()) return false;
    errno = 0;
    char *e=nullptr; long v=strtol(s.c_str(), &e, 10);
    if(errno == ERANGE) return false;
    if(e==s.c_str()||*e) { return false; }
    out = v;
    return true;
}

static inline int digits_for(size_t n){ int w=1; while(n>=10){ n/=10; w++; } return w; }

/* ------------------------------------------------------------------ */
/*                        Line storage / Buffer                       */
/* ------------------------------------------------------------------ */

struct Buffer{
    string path;
    vector<string> lines;
    bool dirty=false;
    bool number=true;
    bool backup=true;
    bool highlight=false;
};

static size_t char_count(const Buffer& b){ size_t t=0; for(auto& L: b.lines) t += L.size()+1; return t; }

/* ------------------------------- Undo ------------------------------ */
struct Snap{ vector<string> lines; };
static const size_t UNDO_MAX=200;
struct Stack{
    vector<Snap> st;
    void clear(){ for(auto& s: st) s.lines.clear(); st.clear(); }
    void push(const Buffer& b){ if(st.size()==UNDO_MAX) st.erase(st.begin()); st.push_back(Snap{b.lines}); }
    bool pop(Snap& s){ if(st.empty()) return false; s=st.back(); st.pop_back(); return true; }
};

/* ------------------------------------------------------------------ */
/*                      File I/O (Hardened)                           */
/* ------------------------------------------------------------------ */
static void load_file(const string& path, Buffer& b){
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

/* hardened atomic_save_to_fd: write -> flush -> fsync -> close */
static bool atomic_save_to_fd(FILE* tf, const Buffer& b, string& err){
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

/* secure doas move (with escaping) */
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

/* hardened atomic_save (with backup and secure fallback) */
static bool atomic_save(const string& path, const Buffer& b, bool backup, string& err){
    mode_t mode = 0644; struct stat st{};
    if(::stat(path.c_str(), &st)==0) mode = st.st_mode & 0777;

    if(backup && ::stat(path.c_str(), &st)==0){
        string berr;
        (void)safe_backup_copy(path, path+"~", berr);
        /* we don't die hard if backup fails */
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
        /* try doas fallback */
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

/* ------------------------------------------------------------------ */
/*                   Auto-recover (kept from old)                     */
/* ------------------------------------------------------------------ */
static string recover_path_for(const Buffer& b){
    string p = b.path.empty()? ".unnamed" : b.path;
    std::hash<string> H; size_t h = H(p);
    std::ostringstream ss; ss<<home_path()<<"/.tedit-recover-"<<std::hex<<h;
    return ss.str();
}
static void autosave_if_needed(const Buffer& b, std::chrono::steady_clock::time_point& last, int interval_sec){
    if(interval_sec<=0) return;
    auto now = std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::seconds>(now - last).count() < interval_sec) return;
    if(!b.dirty) { last = now; return; }
    string rp = recover_path_for(b);
    std::ofstream out(rp, std::ios::binary|std::ios::trunc);
    if(out.good()){
        for(auto& L: b.lines) out<<L<<"\n";
        out.close();
    }
    last = now;
}
static bool maybe_recover(Buffer& b){
    string rp = recover_path_for(b);
    if(!file_exists(rp)) return false;
    cout<<C_YEL<<"recovery: found snapshot "<<rp<<C_RESET<<"\n";
    std::ifstream in(rp); if(!in.good()) return false;
    b.lines.clear(); string L; while(std::getline(in,L)){ rstrip_newline(L); b.lines.push_back(L); }
    b.dirty=true;
    return true;
}

/* ------------------------------------------------------------------ */
/*                     Range parsing (hardened)                       */
/* ------------------------------------------------------------------ */
static bool parse_range(const string& arg, size_t nlines, size_t& out_lo, size_t& out_hi){
    auto norm_token = [&](string t)->string{
        t = trim_copy(t);
        if(t=="$") return std::to_string(nlines? nlines: 0);
        return t;
    };
    string s; for(char c: arg){ if(!std::isspace((unsigned char)c)) s.push_back(c); }
    if(s.empty()){ out_lo=1; out_hi=nlines? nlines:0; return true; }
    auto dash = s.find('-'); long lo=-1, hi=-1;
    if(dash==string::npos){
        string t=norm_token(s);
        if(!parse_long(t, lo) || lo<=0) return false;
        hi = lo;
    }else{
        string L=norm_token(s.substr(0,dash));
        string R=norm_token(s.substr(dash+1));
        if(L.empty()) lo=1; else { if(!parse_long(L, lo) || lo<=0) return false; }
        if(R.empty()) hi=(long)nlines; else { if(!parse_long(R, hi) || hi<=0) return false; }
    }
    if(nlines==0){ out_lo=1; out_hi=0; return true; }
    if((size_t)lo<1) lo=1;
    if((size_t)hi>nlines) hi=(long)nlines;
    if(lo>hi) return false;
    out_lo=(size_t)lo;
    out_hi=(size_t)hi;
    return true;
}

/* ------------------------------------------------------------------ */
/*                   Search / Replace (same)                          */
/* ------------------------------------------------------------------ */
static size_t search_plain_allhits(const Buffer& b, const string& q, bool icase, vector<size_t>& out_lines){
    out_lines.clear();
    if(q.empty()) return 0;
    string qq = icase? lower(q): q;
    for(size_t i=0;i<b.lines.size();++i){
        string L = icase? lower(b.lines[i]) : b.lines[i];
        if(L.find(qq)!=string::npos) out_lines.push_back(i+1);
    }
    return out_lines.size();
}
static size_t search_plain(const Buffer& b, const string& q, bool icase){
    vector<size_t> hits;
    size_t n = search_plain_allhits(b,q,icase,hits);
    if(!n){ cout<<"no matches\n"; return 0; }
    for(auto ln: hits) cout<<"match at "<<ln<<": "<<b.lines[ln-1]<<"\n";
    return n;
}
static size_t search_regex(const Buffer& b, const string& pat){
    size_t hits=0; try{
        std::regex rx(pat);
        for(size_t i=0;i<b.lines.size();++i){
            if(std::regex_search(b.lines[i], rx)){
                cout<<"match at "<<i+1<<": "<<b.lines[i]<<"\n"; hits++;
            }
        }
    } catch(const std::exception& e){ cout<<"regex: "<<e.what()<<"\n"; return 0; }
    if(!hits) cout<<"no matches\n";
    return hits;
}
static int replace_first_line(const string& s,const string& needle,const string& repl,string& out){
    auto pos=s.find(needle);
    if(pos==string::npos){ out=s; return 0; }
    out = s.substr(0,pos) + repl + s.substr(pos+needle.size());
    return 1;
}
static int replace_all_line(const string& s,const string& needle,const string& repl,string& out){
    if(needle.empty()){ out=s; return 0; }
    int cnt=0; size_t pos=0; out.clear();
    while(true){
        auto p=s.find(needle,pos);
        if(p==string::npos){ out += s.substr(pos); break; }
        out += s.substr(pos, p-pos);
        out += repl;
        pos = p + needle.size();
        cnt++;
    }
    return cnt;
}

/* ------------------------------------------------------------------ */
/*                 SECURE FILTER (this was the big one)               */
/* ------------------------------------------------------------------ */
static bool run_filter_replace(vector<string>& lines, size_t lo, size_t hi, const string& shcmd, string &err){
    if (lo < 1 || hi < lo || hi > lines.size()) {
        err = "invalid range";
        return false;
    }

    char in_tpl[]  = "/tmp/tedit_in_XXXXXX";
    char out_tpl[] = "/tmp/tedit_out_XXXXXX";

    int in_fd = ::mkstemp(in_tpl);
    if (in_fd < 0) {
        err = "mkstemp(in): " + string(std::strerror(errno));
        return false;
    }

    {
        FILE *f = ::fdopen(in_fd, "w");
        if (!f) {
            err = "fdopen(in): " + string(std::strerror(errno));
            ::close(in_fd);
            ::unlink(in_tpl);
            return false;
        }
        for (size_t i = lo; i <= hi; ++i) {
            if (std::fputs(lines[i - 1].c_str(), f) == EOF || std::fputc('\n', f) == EOF) {
                err = "write temp: " + string(std::strerror(errno));
                ::fclose(f);
                ::unlink(in_tpl);
                return false;
            }
        }
        std::fflush(f);
        ::fclose(f);
    }

    int out_fd = ::mkstemp(out_tpl);
    if (out_fd < 0) {
        err = "mkstemp(out): " + string(std::strerror(errno));
        ::unlink(in_tpl);
        return false;
    }
    ::close(out_fd);

    /* build: sh -c '<user_cmd> < in > out' but escape OUR paths */
    string shell_line = "sh -c " +
    sh_escape(shcmd + " < " + sh_escape(in_tpl) + " > " + sh_escape(out_tpl));

    int rc = run_shell_cmd(shell_line);
    ::unlink(in_tpl);
    if (rc != 0) {
        err = "filter failed (exit " + std::to_string(rc) + ")";
        ::unlink(out_tpl);
        return false;
    }

    vector<string> out_lines;
    {
        std::ifstream ifs(out_tpl);
        if (!ifs) {
            err = "cannot read filter output";
            ::unlink(out_tpl);
            return false;
        }
        string L;
        while (std::getline(ifs, L)) {
            rstrip_newline(L);
            out_lines.push_back(L);
        }
    }
    ::unlink(out_tpl);

    size_t count = hi - lo + 1;
    lines.erase(lines.begin() + (long)lo - 1,
                lines.begin() + (long)lo - 1 + (long)count);
    lines.insert(lines.begin() + (long)lo - 1, out_lines.begin(), out_lines.end());
    return true;
}

/* ------------------------------------------------------------------ */
/*           Directory listing — mildly hardened                      */
/* ------------------------------------------------------------------ */
static string perm_string(mode_t m){
    const char* t = S_ISDIR(m) ? "d" : "-";
    string p = t;
    const int bits[9]={S_IRUSR,S_IWUSR,S_IXUSR,S_IRGRP,S_IWGRP,S_IXGRP,S_IROTH,S_IWOTH,S_IXOTH};
    const char ch[9]={'r','w','x','r','w','x','r','w','x'};
    for(int i=0;i<9;i++) p.push_back((m & bits[i])? ch[i] : '-');
    return p;
}
static void ls_list(const string& path, bool all, bool longfmt){
    /* micro “don’t list /etc/shadow if you’re not root” thing */
    if (path == "/etc/shadow" && ::geteuid() != 0) {
        cout<<"ls: permission denied\n";
        return;
    }

    std::error_code ec;
    fs::file_status st = fs::status(path, ec);
    if(ec){ cout<<"ls: "<<path<<": "<<ec.message()<<"\n"; return; }
    auto print_one = [&](const fs::directory_entry& e){
        string name = e.path().filename().string();
        if(!all && !name.empty() && name[0]=='.') return;
        string shown = name;
        bool isdir = e.is_directory();
        if(isdir) shown += "/";
        if(longfmt){
            struct stat sb{};
            string full = (path=="."? name : (fs::path(path)/name).string());
            if(::stat(full.c_str(), &sb)==0){
                cout<<perm_string(sb.st_mode)<<" "
                <<std::setw(8)<<(long long)sb.st_size<<"  "
                <<shown<<"\n";
            }else{
                cout<<"?????????? "<<std::setw(8)<<"?"<<"  "<<shown<<"\n";
            }
        }else{
            cout<<shown<<"\n";
        }
    };

    if(fs::is_directory(st)){
        vector<fs::directory_entry> entries;
        std::error_code ec2;
        fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec2), end;
        for(; !ec2 && it!=end; it.increment(ec2)){
            entries.push_back(*it);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const fs::directory_entry& a, const fs::directory_entry& b){
                      return a.path().filename().string() < b.path().filename().string();
                  });
        for(auto& e: entries) print_one(e);
    }else{
        if(longfmt){
            struct stat sb{};
            if(::stat(path.c_str(), &sb)==0){
                cout<<perm_string(sb.st_mode)<<" "
                <<std::setw(8)<<(long long)sb.st_size<<"  "<<fs::path(path).filename().string()<<"\n";
            }else{
                cout<<"?????????? "<<std::setw(8)<<"?"<<"  "<<path<<"\n";
            }
        }else{
            cout<<fs::path(path).filename().string()<<"\n";
        }
    }
}

/* ------------------------------------------------------------------ */
/*             Highlight (keep but slightly guarded)                  */
/* ------------------------------------------------------------------ */
enum class Lang { Plain, Cpp, Python, Shell, Ruby, JS, HTML, CSS, JSON };

static Lang detect_lang(const string& path){
    string ext = lower(fs::path(path).extension().string());
    if(ext==".c"||ext==".cc"||ext==".cpp"||ext==".cxx"||ext==".h"||ext==".hh"||ext==".hpp") return Lang::Cpp;
    if(ext==".py") return Lang::Python;
    if(ext==".sh"||ext==".bash"||ext==".zsh") return Lang::Shell;
    if(ext==".rb") return Lang::Ruby;
    if(ext==".js"||ext==".mjs"||ext==".ts") return Lang::JS;
    if(ext==".html"||ext==".htm") return Lang::HTML;
    if(ext==".css") return Lang::CSS;
    if(ext==".json") return Lang::JSON;
    return Lang::Plain;
}

static string colorize(const string& L, const Buffer& b, const ThemePalette& P){
    if(!use_color() || !b.highlight) return L;
    string s=L;
    try{
        s = std::regex_replace(s, std::regex(R"("([^"\\]|\\.)*")"), P.accent+"$&"+C_RESET);
        s = std::regex_replace(s, std::regex(R"(//.*$)"), P.dim+"$&"+C_RESET);
        s = std::regex_replace(s, std::regex(R"(\b(auto|break|case|class|const|continue|default|delete|do|else|enum|for|friend|if|inline|namespace|new|noexcept|operator|private|protected|public|return|sizeof|static|struct|switch|template|this|throw|try|typedef|typename|union|using|virtual|void|volatile|while)\b)"), P.ok+"$&"+C_RESET);
    }catch(...){}
    return s;
}

static string colorize_lang(const string& L, const Buffer& b, const ThemePalette& P, Lang lang){
    if(!use_color() || !b.highlight) return L;

    if(lang==Lang::Cpp || lang==Lang::Plain) return colorize(L, b, P);

    string s=L;
    try{
        auto qd = std::regex(R"("([^"\\]|\\.)*")");
        auto qs = std::regex(R"('([^'\\]|\\.)*')");
        switch(lang){
            case Lang::Python:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, qs, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(#.*$)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(False|True|None|def|class|return|import|from|if|else|elif|for|while|try|except|finally|with|as|lambda|pass|yield|raise|global|nonlocal|assert|async|await|in|is|and|or|not)\b)"), P.ok+"$&"+C_RESET);
                break;
            case Lang::Shell:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, qs, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(#.*$)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(if|then|else|elif|fi|for|in|do|done|case|esac|function|select|until|time|echo|exit|return)\b)"), P.ok+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\$[A-Za-z_][A-Za-z0-9_]*|\$\{[^}]+\})"), P.accent+"$&"+C_RESET);
                break;
            case Lang::Ruby:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, qs, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(#.*$)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(def|class|module|if|else|elsif|end|do|while|until|return|yield|begin|rescue|ensure|case|when|then|super|self|nil|true|false)\b)"), P.ok+"$&"+C_RESET);
                break;
            case Lang::JS:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, qs, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(//.*$)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(function|return|let|const|var|if|else|for|while|class|extends|import|export|new|try|catch|finally|throw|switch|case|default|break|continue|yield|await|async)\b)"), P.ok+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(true|false|null|undefined|NaN|Infinity)\b)"), P.ok+"$&"+C_RESET);
                break;
            case Lang::HTML:
                s = std::regex_replace(s, std::regex(R"(<!--.*-->)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(<[^>]+>)"), P.accent+"$&"+C_RESET);
                break;
            case Lang::CSS:
                s = std::regex_replace(s, std::regex(R"(\/\*.*\*\/)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b([A-Za-z_-]+)(?=\s*:))"), P.ok+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"([{};:,])"), P.accent+"$&"+C_RESET);
                break;
            case Lang::JSON:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(true|false|null)\b)"), P.ok+"$&"+C_RESET);
                break;
            default: break;
    }
} catch(...) {}
    return s;
}

/* ------------------------------------------------------------------ */
/*               Terminal width & wrapped printing                    */
/* ------------------------------------------------------------------ */
static int term_width(){
#if defined(__unix__) || defined(__APPLE__)
    struct winsize ws{};
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==0 && ws.ws_col>0) return ws.ws_col;
#endif
    return 80;
}

static void print_wrapped_with_gutter(const string& ansi,
                                      const string& first_prefix,
                                      const string& cont_prefix,
                                      int avail_cols)
{
    int col = 0;
    bool esc = false;

    auto print_prefix = [&](const string& p){
        cout<<p;
    };

    print_prefix(first_prefix);

    for(size_t k=0;k<ansi.size();++k){
        char ch = ansi[k];
        if(!esc){
            if(ch=='\033'){ esc=true; cout<<ch; continue; }
            if(ch=='\n'){
                cout<<"\n";
                col = 0;
                print_prefix(cont_prefix);
                continue;
            }
            if(avail_cols>0 && col>=avail_cols){
                cout<<"\n";
                col = 0;
                print_prefix(cont_prefix);
            }
            cout<<ch;
            col++;
        }else{
            cout<<ch;
            if(ch=='m') esc=false;
        }
    }
    cout<<C_RESET<<"\n";
}

/* ------------------------------------------------------------------ */
/*                     Interactive line input                         */
/* ------------------------------------------------------------------ */
struct LineReader{
    vector<string> history; size_t HIST_MAX=800;
    vector<string> commands;
    string color_input = "";
    string color_reset = C_RESET;

    void set_theme_colors(const ThemePalette& P){
        color_input = P.input;
        color_reset = C_RESET;
    }

    static vector<string> split_words(const string& s){
        vector<string> v;
        std::istringstream in(s);
        string w;
        while(in>>w) v.push_back(w);
        return v;
    }

    static string expand_home_in_token(string in){
        if(in.empty()) return in;
        if(in=="~") return home_path();
        if(in.size()>=2 && in[0]=='~' && in[1]=='/'){
            return home_path()+in.substr(1);
        }
        return in;
    }

    static vector<string> complete_dirs_only(const string& token){
        string t = expand_home_in_token(token);
        vector<string> out; string base=t, dir=".";
        auto pos = t.find_last_of('/');
        if(pos!=string::npos){ dir = t.substr(0,pos); base = t.substr(pos+1); }
        std::error_code ec;
        for(auto& e: fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)){
            string name = e.path().filename().string();
            if(name.rfind(base,0)==0 && e.is_directory()){
                string cand = (dir=="."? name: dir+"/"+name);
                out.push_back(cand + "/");
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    static vector<string> complete_fs(const string& token){
        vector<string> out; string base=token, dir="";
        auto pos = token.find_last_of('/');
        if(pos!=string::npos){ dir = token.substr(0,pos); base = token.substr(pos+1); } else dir = ".";
        std::error_code ec;
        for(auto& e: fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)){
            string name = e.path().filename().string();
            if(name.rfind(base,0)==0){
                string cand = (dir=="."? name: dir+"/"+name);
                if(e.is_directory()) cand += "/";
                out.push_back(cand);
            }
        }
        std::sort(out.begin(), out.end()); return out;
    }

    vector<string> complete(const string& buf){
        auto toks = split_words(buf); vector<string> out;
        bool at_start = toks.empty();
        bool fresh = (!buf.empty() && std::isspace((unsigned char)buf.back()));

        if(at_start){ out = commands; return out; }

        if(toks.size()==1 && !fresh){
            string pref=toks[0];
            for(const auto& c: commands) if(c.rfind(pref,0)==0) out.push_back(c);
            return out;
        }

        string first = toks[0];

        if(first=="cd"){
            if(toks.size()==1 && fresh){
                return complete_dirs_only("");
            }
            string last = fresh ? string("") : toks.back();
            return complete_dirs_only(last);
        }

        string last = fresh ? string("") : toks.back();
        return complete_fs(last);
    }

    void remember(const string& s){
        if(s.empty()) return;
        if(history.empty() || history.back()!=s){
            if(history.size()<HIST_MAX) history.push_back(s);
            else { history.erase(history.begin()); history.push_back(s); }
        }
    }

    string read(const string& prompt){
        bool tty=false;
        #if defined(__unix__) || defined(__APPLE__)
        tty = isatty(STDIN_FILENO);
        #endif
        if(!tty){
            cout<<prompt<<std::flush;
            string s; if(!std::getline(std::cin, s)) return string(); return s;
        }

        #if defined(__unix__) || defined(__APPLE__)
        cout<<prompt<<std::flush;

        struct termios orig{};
        bool have_orig = false;
        if(isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO,&orig)!=-1){
            struct termios t=orig;
            t.c_lflag &= ~(ECHO|ICANON);
            t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
            if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&t)==0) {
                have_orig = true;
            }
        }

        struct TermiosGuard {
            bool active;
            struct termios o;
            TermiosGuard(bool a, const struct termios& oo):active(a),o(oo){}
            ~TermiosGuard(){
                if(active) tcsetattr(STDIN_FILENO,TCSAFLUSH,&o);
            }
        } guard(have_orig, orig);

        string buf; size_t cursor=0; int hist_idx=(int)history.size();

        auto refresh=[&](){
            cout<<"\r\033[2K"<<prompt<<color_input<<buf<<color_reset;
            size_t tail = buf.size() - cursor;
            if(tail>0) cout<<"\033["<<tail<<"D";
            cout.flush();
        };

        refresh();
        while(true){
            char c=0; ssize_t n=::read(STDIN_FILENO,&c,1);
            if(n<=0) return string();
            if(c=='\r'||c=='\n'){ cout<<"\r\n"; break; }
            else if((unsigned char)c==127||c=='\b'){
                if(cursor>0){ buf.erase(buf.begin()+cursor-1); cursor--; refresh(); }
            }
            else if(c=='\t'){
                auto opts=complete(buf);
                if(opts.empty()){
                } else if(opts.size()==1){
                    auto toks=split_words(buf);
                    size_t lastsp=buf.find_last_of(' ');
                    string prefix=(lastsp==string::npos? string(): buf.substr(0,lastsp+1));
                    buf = prefix + opts[0];
                    cursor=buf.size(); refresh();
                } else {
                    cout<<"\r\n";
                    size_t shown=0;
                    for(auto& o:opts){ cout<<o<<"  "; if(++shown%6==0) cout<<"\r\n"; }
                    if((shown%6)!=0) cout<<"\r\n";
                    refresh();
                }
            }
            else if(c==27){
                char seq[2];
                if(::read(STDIN_FILENO,seq,1)<=0) continue;
                if(seq[0]=='['){
                    char k;
                    if(::read(STDIN_FILENO,&k,1)<=0) continue;
                    if(k=='A'){
                        if(hist_idx>0){ hist_idx--; buf=history[hist_idx]; cursor=buf.size(); refresh(); }
                    } else if(k=='B'){
                        if(hist_idx<(int)history.size()-1){ hist_idx++; buf=history[hist_idx]; cursor=buf.size(); refresh(); }
                        else { hist_idx=(int)history.size(); buf.clear(); cursor=0; refresh(); }
                    } else if(k=='C'){ if(cursor<buf.size()){ cursor++; refresh(); } }
                    else if(k=='D'){ if(cursor>0){ cursor--; refresh(); } }
                }
            }
            else if(c==1){ cursor=0; refresh(); }
            else if(c==5){ cursor=buf.size(); refresh(); }
            else{
                buf.insert(buf.begin()+cursor,c);
                cursor++; refresh();
            }
        }
        return buf;
        #else
        string s; cout<<prompt; if(!std::getline(std::cin,s)) return string(); return s;
        #endif
    }
};

/* ------------------------------------------------------------------ */
/*                               Editor                               */
/* ------------------------------------------------------------------ */
struct Editor{
    Buffer buf; Stack undo, redo; LineReader lr;

    Theme theme = Theme::Default;
    ThemePalette P = palette_for(theme);

    vector<Buffer> others;
    string last_search; bool last_icase=false; size_t last_index=0;
    int autosave_sec = 120;
    std::chrono::steady_clock::time_point last_autosave = std::chrono::steady_clock::now();
    std::map<string,string> aliases;

    bool wrap_long = true;
    bool truncate_long = false;

    Lang lang = Lang::Plain;

    lua_State* L = nullptr;
    vector<string> plugin_names;

    Editor(){
        g_editor = this;
        lr.commands = {
            "help","open","info","write","w","wq","saveas","quit","q","print","p","r",
            "append","a","insert","i","delete","d","move","m","join","find","findi","findre",
            "repl","replg","read","undo","u","redo","set","filter","ls","pwd","number",
            "goto","n","N","new","bnext","bprev","lsb","theme","highlight","alias","diff",
            "cd","clear","version","lua","luafile","plugins","reload-plugins"
        };
        lr.set_theme_colors(P);
        init_lua();
    }

    ~Editor(){
        close_lua();
    }

    string cfg_path() const { return tedit_config_dir() + "/.teditrc"; }

    static string theme_name(Theme t){
        switch(t){ case Theme::Dark: return "dark"; case Theme::Neon: return "neon"; case Theme::Matrix: return "matrix"; case Theme::Paper: return "paper"; default: return "default"; }
    }
    static Theme theme_from_name(const string& s){
        string n=lower(s);
        if(n=="dark") return Theme::Dark;
        if(n=="neon") return Theme::Neon;
        if(n=="matrix") return Theme::Matrix;
        if(n=="paper") return Theme::Paper;
        return Theme::Default;
    }
    static bool parse_bool_string(const string& v, bool& out){
        string s=lower(trim_copy(v));
        if(s=="1"||s=="on"||s=="true"||s=="yes"){ out=true; return true; }
        if(s=="0"||s=="off"||s=="false"||s=="no"){ out=false; return true; }
        return false;
    }
    static string esc(const string& in){ string r; r.reserve(in.size()); for(char ch: in){ if(ch=='\\' || ch=='\t') r.push_back('\\'); r.push_back(ch); } return r; }
    static string unesc(const string& in){ string r; r.reserve(in.size()); bool e=false; for(char ch: in){ if(e){ r.push_back(ch); e=false; } else if(ch=='\\') e=true; else r.push_back(ch); } return r; }

    void init_lua(){
        if(L) return;
        L = luaL_newstate();
        if(!L) return;
        luaL_openlibs(L);
        lua_register(L, "tedit_command", l_tedit_command);
        lua_register(L, "tedit_echo", l_tedit_echo);
        lua_register(L, "tedit_print", l_tedit_print);
        load_lua_plugins();
    }

    void close_lua(){
        if(L){
            lua_close(L);
            L = nullptr;
        }
    }

    void load_lua_plugins(){
        plugin_names.clear();
        if(!L) return;
        string dir = tedit_plugins_dir();
        std::error_code ec;
        if(!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;
        fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
        for(; !ec && it!=end; it.increment(ec)){
            const auto& entry = *it;
            if(!entry.is_regular_file()) continue;
            fs::path p = entry.path();
            if(p.extension() != ".lua") continue;
            string fpath = p.string();
            int rc = luaL_loadfile(L, fpath.c_str());
            if(rc != LUA_OK){
                const char* msg = lua_tostring(L, -1);
                cout<<P.err<<"lua plugin error ("<<fpath<<"): "<<(msg?msg:"")<<C_RESET<<"\n";
                lua_pop(L, 1);
                continue;
            }
            rc = lua_pcall(L, 0, 0, 0);
            if(rc != LUA_OK){
                const char* msg = lua_tostring(L, -1);
                cout<<P.err<<"lua plugin error ("<<fpath<<"): "<<(msg?msg:"")<<C_RESET<<"\n";
                lua_pop(L, 1);
                continue;
            }
            string name = p.stem().string();
            plugin_names.push_back(name);
            cout<<P.ok<<"loaded "<<name<<C_RESET<<"\n";
        }
    }

    void save_config(){
        std::ofstream out(cfg_path(), std::ios::binary|std::ios::trunc);
        if(!out.good()) return;
        out<<"theme="<<theme_name(theme)<<"\n";
        out<<"highlight="<<(buf.highlight?"on":"off")<<"\n";
        out<<"number="<<(buf.number?"on":"off")<<"\n";
        out<<"backup="<<(buf.backup?"on":"off")<<"\n";
        out<<"autosave="<<(autosave_sec)<<"\n";
        out<<"wrap="<<(wrap_long?"on":"off")<<"\n";
        out<<"truncate="<<(truncate_long?"on":"off")<<"\n";
        for(auto& kv: aliases) out<<"alias\t"<<esc(kv.first)<<"\t"<<esc(kv.second)<<"\n";
    }
    void load_config(){
        std::ifstream in(cfg_path());
        if(!in.good()) return;
        string line;
        while(std::getline(in,line)){
            if(line.empty()) continue;
            if(line.rfind("alias\t",0)==0){
                size_t p1=line.find('\t',6); if(p1==string::npos) continue;
                size_t p2=line.find('\t',p1+1);
                if(p2==string::npos){
                    string from=unesc(line.substr(p1+1));
                    aliases[from]="";
                }else{
                    string from=unesc(line.substr(p1+1, p2-(p1+1)));
                    string to  =unesc(line.substr(p2+1));
                    aliases[from]=to;
                }
                continue;
            }
            auto eq=line.find('=');
            if(eq==string::npos) continue;
            string key=lower(trim_copy(line.substr(0,eq)));
            string val=trim_copy(line.substr(eq+1));
            if(key=="theme"){ theme=theme_from_name(val); P=palette_for(theme); lr.set_theme_colors(P); }
            else if(key=="highlight"){ bool b; if(parse_bool_string(val,b)) buf.highlight=b; }
            else if(key=="number"){ bool b; if(parse_bool_string(val,b)) buf.number=b; }
            else if(key=="backup"){ bool b; if(parse_bool_string(val,b)) buf.backup=b; }
            else if(key=="autosave"){ long s; if(parse_long(val,s)) autosave_sec=(int)std::max<long>(0,s); }
            else if(key=="wrap"){ bool b; if(parse_bool_string(val,b)) wrap_long=b; }
            else if(key=="truncate"){ bool b; if(parse_bool_string(val,b)) truncate_long=b; }
        }
    }

    void confetti(){
        if(!use_color()) return;
        const char* art[] = {" *  .  *   . *",".  *  *  .    *","   *  .   *  . "};
        cout<<P.accent<<art[0]<<C_RESET<<"\n"<<P.ok<<art[1]<<C_RESET<<"\n"<<P.warn<<art[2]<<C_RESET<<"\n";
    }
    void tip(){
        static const char* tips[] = {
            "Tip: use 'goto <n>' to jump to a line.",
            "Tip: 'n' and 'N' hop through last search results.",
            "Tip: ':filter 1,10 !sed -n \"p\"' pipes lines through a shell.",
            "Tip: 'theme neon' and 'highlight on' for vibes.",
            "Tip: 'alias dd \"delete 1-$\"' to delete all quickly.",
            "Tip: 'diff' shows changes vs on-disk.",
            "Tip: 'Tab': first word = commands only; after 'cd ' => directories only."
        };
        std::mt19937_64 rng((unsigned)time(nullptr));
        cout<<P.dim<<tips[rng()% (sizeof(tips)/sizeof(*tips))]<<C_RESET<<"\n";
    }
    void banner(){
        string bpath = home_path()+"/.tedit_banner";
        if(!file_exists(bpath)) return;
        std::ifstream in(bpath);
        if(!in.good()) return;
        cout<<P.accent;
        string L; while(std::getline(in,L)) cout<<L<<"\n";
        cout<<C_RESET;
    }

    string prompt_str() const {
        string dirty = buf.dirty? "*" : "";
        return (use_color()? P.prompt : string("")) + dirty + "tedit> " + (use_color()? C_RESET: string(""));
    }

    void status(){
        using std::chrono::system_clock;
        auto t = system_clock::to_time_t(system_clock::now());
        char tb[32]; strftime(tb,sizeof(tb),"%H:%M:%S", localtime(&t));
        string tname = (theme==Theme::Dark?"dark": theme==Theme::Neon?"neon": theme==Theme::Matrix?"matrix": theme==Theme::Paper?"paper":"default");
        cout<<P.dim<<"["<< (buf.path.empty()? "(unnamed)": buf.path) << "] "
        <<"lines="<<buf.lines.size()<<" chars="<<char_count(buf)
        <<(buf.dirty?" *":"")
        <<" | "<<tb<<" | theme:"<<tname
        <<" | wrap:"<<(wrap_long?"on":"off")
        <<C_RESET<<"\n";
    }

    void help(){
        auto CMD = [&](const string& cmd, const string& args, const string& desc){
            std::ostringstream left;
            left<<std::left<<std::setw(22)<<(cmd);
            cout<<P.help_cmd<<left.str()<<C_RESET
            <<P.help_arg<<std::left<<std::setw(18)<<args<<C_RESET
            <<" — "<<P.help_text<<desc<<C_RESET<<"\n";
        };

        cout<<P.title<<"Commands (':' optional)"<<C_RESET<<"\n";
        CMD("open <path>",            "", "open file");
        CMD("info",                   "", "buffer + file info");
        CMD("w|write [path]",         "", "save (atomic), optional new path");
        CMD("wq",                     "", "save & quit");
        CMD("saveas <path>",          "", "save to path");
        CMD("q|quit",                 "", "quit (prompts if unsaved)");
        CMD("p|print [range]",        "", "print lines");
        CMD("r <n>",                  "", "show one line");
        CMD("a|append",               "", "append lines ('.' ends; use \".\" for a literal)");
        CMD("i|insert <n>",           "", "insert before line n");
        CMD("d|delete [range]",       "", "delete lines");
        CMD("m|move <from> <to>",     "", "move line");
        CMD("join <range>",           "", "join lines with space");
        CMD("/text | find | findi | findre", "", "search (regex via findre)");
        CMD("n | N",                  "", "next/prev match from last search");
        CMD("goto <n>",               "", "jump to line");
        CMD("repl old new | replg old new", "", "replace first/global per line");
        CMD("read <path> [n]",        "", "insert file after n (default=end)");
        CMD("write [range] <path>",   "", "write range to path");
        CMD("filter <range> !shell",  "", "pipe range through shell and replace (safe temp names)");
        CMD("undo | u [k]",           "", "undo (optionally k steps)");
        CMD("redo",                   "", "redo");
        CMD("set number on|off",      "", "toggle line numbers");
        CMD("set backup on|off",      "", "toggle on-save ~ backup");
        CMD("set autosave <sec>",     "", "autosave interval");
        CMD("set wrap on|off",        "", "soft-wrap long lines under the gutter");
        CMD("set truncate on|off",    "", "truncate line display when wrap=off");
        CMD("set lang <name>",        "", "override syntax (auto by extension)");
        CMD("highlight on|off",       "", "simple syntax highlighting");
        CMD("theme <name>",           "", "default|dark|neon|matrix|paper");
        CMD("alias <from> <to...>",   "", "define command alias");
        CMD("new [path]",             "", "open new buffer (push current)");
        CMD("bnext | bprev | lsb",    "", "cycle/list buffers");
        CMD("diff",                   "", "show diff vs on-disk (safe)");
        CMD("ls [-l] [-a] [path] | pwd","", "filesystem helpers");
        CMD("cd <dir>",               "", "change directory (./ ../ ~/)");
        CMD("clear",                  "", "clear screen and scrollback");
        CMD("version",                "", "show tedit version");
        CMD("lua <code>",             "", "run Lua code");
        CMD("luafile <path>",         "", "run Lua script file");
        CMD("plugins",                "", "list loaded Lua plugins");
        CMD("reload-plugins",         "", "reload Lua plugins from tedit-config/plugins");
        cout<<P.dim<<"Tab: first word => commands only; after 'cd ' => directories only."<<C_RESET<<"\n";
    }

    void load(const string& p){
        buf.path=p; load_file(p, buf);
        lang = detect_lang(p);
        cout<<P.ok<<"opened "<<p<<C_RESET<<"\n";
        (void)maybe_recover(buf);
    }

    bool run_hook(const char* name){
        string h = home_path()+"/.tedit/hooks/";
        h += name;
        if(!file_exists(h)) return true;
        string cmd = "sh -c " + sh_escape(h + (buf.path.empty() ? "" : (" " + sh_escape(buf.path))));
        int rc = run_shell_cmd(cmd);
        return rc==0;
    }

    bool save(const string& maybe){
        string target = maybe.empty()? buf.path : maybe;
        if(target.empty()){
            cout<<P.warn<<"save: no filename (use: write <path>)"<<C_RESET<<"\n"; return false;
        }
        string err;
        bool ok = atomic_save(target, buf, buf.backup, err);
        if(!ok){
            cout<<P.err<<"save: "<<err<<C_RESET<<"\n";
            return false;
        }
        if(target!=buf.path) buf.path=target;
        buf.dirty=false;
        cout<<P.ok<<"saved to "<<target<<C_RESET<<"\n";
        confetti();
        string rec = recover_path_for(buf);
        unlink(rec.c_str());
        (void)run_hook("on_save");
        return true;
    }

    void push_undo(){ undo.push(buf); redo.clear(); }

    void append_mode(){
        cout<<"enter text; '.' alone ends (use \".\" for a literal '.')\n";
        string s; size_t added=0;
        while(true){
            cout<<"> "<<std::flush;
            if(!std::getline(std::cin,s)){ cout<<"\n"; break; }
            if(s=="\".\"") s=".";
            else if(s==".") break;
            buf.lines.push_back(s); added++;
        }
        if(added){ buf.dirty=true; cout<<"appended "<<added<<" line(s)\n"; }
    }

    void insert_mode(size_t before){
        cout<<"enter text; '.' alone ends (use \".\" for a literal '.')\n";
        string s; size_t added=0;
        while(true){
            cout<<"> "<<std::flush;
            if(!std::getline(std::cin,s)){ cout<<"\n"; break; }
            if(s=="\".\"") s=".";
            else if(s==".") break;
            if(before + added > buf.lines.size()) {
                buf.lines.push_back(s);
            } else {
                buf.lines.insert(buf.lines.begin()+ (long)before + (long)added, s);
            }
            added++;
        }
        if(added){ buf.dirty=true; cout<<"inserted "<<added<<" line(s)\n"; }
    }

    int gutter_width() const {
        if(!buf.number) return 0;
        int w = digits_for(buf.lines.size()==0?1:buf.lines.size());
        return w + 3;
    }

    void print_line(size_t i){
        const int termw = term_width();
        const int gw = gutter_width();
        const int avail = std::max(10, termw - gw);

        std::ostringstream first, cont;
        if(buf.number){
            first<<P.gutter<<std::setw(gw-3)<<i<<" | "<<C_RESET;
            cont <<P.gutter<<std::string(gw-3, ' ')<<" | "<<C_RESET;
        }

        string colored = colorize_lang(buf.lines[i-1], buf, P, lang);

        if(wrap_long){
            print_wrapped_with_gutter(colored, first.str(), cont.str(), avail);
        } else {
            if(truncate_long){
                int col=0; bool esc=false;
                cout<<first.str();
                for(size_t k=0;k<colored.size();++k){
                    char ch=colored[k];
                    if(!esc){
                        if(ch=='\033'){ esc=true; cout<<ch; continue; }
                        if(col>=avail-1){ cout<<"…"; break; }
                        cout<<ch; col++;
                    }else{
                        cout<<ch; if(ch=='m') esc=false;
                    }
                }
                cout<<C_RESET<<"\n";
            }else{
                cout<<first.str()<<colored<<C_RESET<<"\n";
            }
        }
    }

    void print(size_t lo, size_t hi){
        if(hi==0){ cout<<"(empty)\n"; return; }
        for(size_t i=lo;i<=hi;i++) print_line(i);
    }

    void repl(bool global, const string& old, const string& nw){
        if(old.empty()){ cout<<P.warn<<"usage: repl[g] <old> <new>"<<C_RESET<<"\n"; return; }
        push_undo(); int total=0;
        for(string& L: buf.lines){
            string out; int c = global? replace_all_line(L,old,nw,out): replace_first_line(L,old,nw,out);
            if(c){ L.swap(out); total+=c; }
        }
        if(total){ buf.dirty=true; cout<<"replaced "<<total<<" occurrence"<<(total==1?"":"s")<<(global?" (global)":" (first per line)")<<"\n"; }
        else { cout<<"no occurrences\n"; }
    }

    void info(){
        struct stat st{}; bool have = (!buf.path.empty() && ::stat(buf.path.c_str(), &st)==0);
        cout<<"file: "<<(buf.path.empty()? "(unnamed)": buf.path)<<(buf.dirty?" *":"")<<"\n";
        cout<<"  lines: "<<buf.lines.size()<<", chars: "<<char_count(buf)<<"\n";
        if(have){ cout<<"  size: "<<(long long)st.st_size<<" bytes, mode: "<<std::oct<< (st.st_mode & 0777) << std::dec <<"\n"; }
        else cout<<"  on-disk: (none)\n";
    }

    void next_match(bool reverse){
        if(last_search.empty()){ cout<<"(no previous search)\n"; return; }
        vector<size_t> hits;
        search_plain_allhits(buf,last_search,last_icase,hits);
        if(hits.empty()){ cout<<"no matches\n"; return; }
        if(!reverse){
            auto it = std::upper_bound(hits.begin(), hits.end(), last_index);
            if(it==hits.end()) it=hits.begin();
            last_index = *it;
        }else{
            auto it = std::lower_bound(hits.begin(), hits.end(), last_index);
            if(it==hits.begin()) it=hits.end();
            --it; last_index = *it;
        }
        print(last_index,last_index);
    }

    void cycle_theme(const string& name){
        theme = theme_from_name(name);
        P = palette_for(theme);
        lr.set_theme_colors(P);
        cout<<P.ok<<"theme set"<<C_RESET<<"\n";
        save_config();
    }

    void open_new_buffer(const string& path){
        others.push_back(buf);
        Buffer nb;
        if(!path.empty()){ nb.path=path; load_file(path, nb); maybe_recover(nb); }
        buf = std::move(nb);
        lang = detect_lang(buf.path);
        cout<<P.ok<<"(new buffer) "<<(path.empty()? "(unnamed)":path)<<C_RESET<<"\n";
    }
    void list_buffers(){
        cout<<C_BOLD<<"* 0 "<<(buf.path.empty()?"(unnamed)":buf.path)<<(buf.dirty?" *":"")<<C_RESET<<"\n";
        for(size_t i=0;i<others.size();++i){
            const auto& b=others[i];
            cout<<"  "<<i+1<<" "<<(b.path.empty()?"(unnamed)":b.path)<<(b.dirty?" *":"")<<"\n";
        }
    }
    void bnext(){
        if(others.empty()){ cout<<"(only one buffer)\n"; return; }
        others.insert(others.begin(), buf);
        buf = others.back();
        others.pop_back();
        lang = detect_lang(buf.path);
        cout<<"[bnext] "<<(buf.path.empty()? "(unnamed)":buf.path)<<"\n";
    }
    void bprev(){
        if(others.empty()){ cout<<"(only one buffer)\n"; return; }
        auto back = others.back(); others.pop_back();
        others.insert(others.begin(), buf);
        buf = back;
        lang = detect_lang(buf.path);
        cout<<"[bprev] "<<(buf.path.empty()? "(unnamed)":buf.path)<<"\n";
    }
    void show_diff(){
        if(buf.path.empty() || !file_exists(buf.path)){ cout<<"diff: no on-disk version\n"; return; }
        char tpat[]="/tmp/tedit_diff_XXXXXX";
        int tfd = mkstemp(tpat); if(tfd<0){ cout<<"diff: mkstemp failed\n"; return; }
        FILE* tf = fdopen(tfd,"w"); if(!tf){ close(tfd); unlink(tpat); cout<<"diff: fdopen failed\n"; return; }
        string err;
        if(!atomic_save_to_fd(tf,buf,err)){ unlink(tpat); cout<<"diff: "<<err<<"\n"; return; }

        string inner = "diff -u -- " + sh_escape(buf.path) + " " + sh_escape(tpat) + " || true";
        string cmd   = "sh -c " + sh_escape(inner);
        run_shell_cmd(cmd);
        unlink(tpat);
    }

    void clear_screen(){
        cout<<"\033[3J\033[H\033[2J"<<std::flush;
    }

    static string expand_path(const string& in){
        if(in.empty()) return in;
        if(in=="~") return home_path();
        if(in.size()>=2 && in[0]=='~' && in[1]=='/') return home_path()+in.substr(1);
        return in;
    }

    bool handle(const string& raw){
        autosave_if_needed(buf, last_autosave, autosave_sec);

        string in = trim_copy(raw);
        if(in.empty()) return true;
        if(in[0]==':'){ in = trim_copy(in.substr(1)); if(in.empty()) return true; }

        {
            std::istringstream ss(in); string tok; ss>>tok;
            auto it = aliases.find(tok);
            if(it!=aliases.end()){
                string rest; std::getline(ss,rest);
                in = it->second + rest;
            }
        }

        if(in[0]=='/'){
            string q=in.substr(1); last_search=q; last_icase=false; last_index=0;
            search_plain(buf,q,false); return true;
        }

        std::istringstream ss(in); string cmd; ss>>cmd; string rest; std::getline(ss,rest); rest=trim_copy(rest);
        string lc = lower(cmd);

        if(lc=="help"||lc=="h"||lc=="?") { help(); return true; }
        if(lc=="open"){
            if(rest.empty()){ cout<<P.warn<<"usage: open <path>"<<C_RESET<<"\n"; return true; }
            if(!buf.path.empty() && buf.dirty){ cout<<P.warn<<"Unsaved changes. Use wq or quit."<<C_RESET<<"\n"; return true; }
            load(rest); return true;
        }
        if(lc=="info"){ info(); return true; }
        if(lc=="wq"){ if(save("")){ cout<<P.dim<<"bye!"<<C_RESET<<"\n"; (void)run_hook("on_quit"); return false; } return true; }
        if(lc=="write"||lc=="w"){ save(rest); return true; }
        if(lc=="saveas"){ if(rest.empty()){ cout<<P.warn<<"usage: saveas <path>"<<C_RESET<<"\n"; return true; } save(rest); return true; }

        if(lc=="quit"||lc=="q"){
            if(buf.dirty){
                cout<<P.warn<<"Unsaved changes. Save before quit? [y/N] "<<C_RESET<<std::flush;
                char c=0; std::cin.get(c); string dump; std::getline(std::cin,dump);
                if(c=='y'||c=='Y'){ if(!save("")) return true; }
            }
            cout<<P.dim<<"bye!"<<C_RESET<<"\n"; (void)run_hook("on_quit"); return false;
        }

        if(lc=="print"||lc=="p"){
            size_t lo=1,hi=buf.lines.size();
            if(!parse_range(rest,buf.lines.size(),lo,hi)){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
            print(lo,hi); return true;
        }
        if(lc=="r"){
            long n=0; if(!parse_long(rest,n)){ cout<<P.warn<<"usage: r <n>"<<C_RESET<<"\n"; return true; }
            if(n<1 || (size_t)n>buf.lines.size()){ cout<<P.warn<<"no such line"<<C_RESET<<"\n"; return true; } print((size_t)n,(size_t)n); return true;
        }
        if(lc=="goto"){
            long n=0; if(!parse_long(rest,n)){ cout<<P.warn<<"usage: goto <n>"<<C_RESET<<"\n"; return true; }
            if(n<1 || (size_t)n>buf.lines.size()){ cout<<P.warn<<"no such line"<<C_RESET<<"\n"; return true; }
            print((size_t)n,(size_t)n); return true;
        }

        if(lc=="append"||lc=="a"){ push_undo(); append_mode(); return true; }
        if(lc=="insert"||lc=="i"){
            long n=0; if(!parse_long(rest,n)){ cout<<P.warn<<"usage: insert <n>"<<C_RESET<<"\n"; return true; }
            if(n<1 || (size_t)n>buf.lines.size()+1){ cout<<P.warn<<"invalid target line"<<C_RESET<<"\n"; return true; }
            push_undo(); insert_mode((size_t)n-1); buf.dirty=true; return true;
        }

        if(lc=="delete"||lc=="d"){
            if(buf.lines.empty()){ cout<<"(empty)\n"; return true; }
            size_t lo=1,hi=buf.lines.size();
            if(!parse_range(rest,buf.lines.size(),lo,hi)){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
            push_undo();
            size_t count=hi-lo+1;
            buf.lines.erase(buf.lines.begin()+ (long)lo-1,
                            buf.lines.begin()+ (long)lo-1 + (long)count);
            buf.dirty=true;
            cout<<"deleted "<<count<<" line(s)\n";
            return true;
        }

        if(lc=="move"||lc=="m"){
            std::istringstream ts(rest); long from=0,to=0; ts>>from>>to;
            if(!from && !to){ cout<<P.warn<<"usage: move <from> <to>"<<C_RESET<<"\n"; return true; }
            if(from<1 || (size_t)from>buf.lines.size() || to<0 || (size_t)to>buf.lines.size()){
                cout<<P.warn<<"bad indexes"<<C_RESET<<"\n"; return true;
            }
            push_undo();
            string s=buf.lines[(size_t)from-1];
            buf.lines.erase(buf.lines.begin()+ (long)from-1);
            if(to>from) to--;
            if(to>(long)buf.lines.size()) to=(long)buf.lines.size();
            buf.lines.insert(buf.lines.begin()+ (long)to, s);
            buf.dirty=true;
            cout<<"moved line "<<from<<" to "<<to<<"\n";
            return true;
        }

        if(lc=="join"){
            size_t lo=1,hi=buf.lines.size();
            if(!parse_range(rest,buf.lines.size(),lo,hi)||hi<=lo){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
            push_undo();
            std::ostringstream out;
            for(size_t i=lo;i<=hi;i++){ if(i>lo) out<<" "; out<<buf.lines[i-1]; }
            buf.lines.erase(buf.lines.begin()+ (long)lo-1,
                            buf.lines.begin()+ (long)hi);
            buf.lines.insert(buf.lines.begin()+ (long)lo-1, out.str());
            buf.dirty=true;
            cout<<"joined\n";
            return true;
        }

        if(lc=="find"){ if(rest.empty()){ cout<<P.warn<<"usage: find <text>"<<C_RESET<<"\n"; return true; } last_search=rest; last_icase=false; last_index=0; search_plain(buf,rest,false); return true; }
        if(lc=="findi"){ if(rest.empty()){ cout<<P.warn<<"usage: findi <text>"<<C_RESET<<"\n"; return true; } last_search=rest; last_icase=true;  last_index=0; search_plain(buf,rest,true);  return true; }
        if(lc=="findre"){ if(rest.empty()){ cout<<P.warn<<"usage: findre <regex>"<<C_RESET<<"\n"; return true; } search_regex(buf,rest); return true; }
        if(lc=="n"){ next_match(false); return true; }
        if(lc=="N"){ next_match(true);  return true; }

        if(lc=="repl"||lc=="replg"){ bool g=(lc=="replg"); std::istringstream ts(rest); string old,nw; ts>>old>>nw; repl(g,old,nw); return true; }

        if(lc=="read"){
            std::istringstream ts(rest); string p; long n=-1; ts>>p;
            if(p.empty()){ cout<<P.warn<<"usage: read <path> [n]"<<C_RESET<<"\n"; return true; }
            if(!(ts>>n)) n=-1;
            std::ifstream in(p);
            if(!in.good()){ cout<<P.err<<"read: cannot open"<<C_RESET<<"\n"; return true; }
            push_undo();
            vector<string> R; string L;
            while(std::getline(in,L)){ rstrip_newline(L); R.push_back(L); }
            size_t at = (n<0)? buf.lines.size(): (size_t)n;
            if(at>buf.lines.size()) at=buf.lines.size();
            buf.lines.insert(buf.lines.begin()+ (long)at, R.begin(), R.end());
            buf.dirty=true;
            cout<<"read "<<R.size()<<" line(s) from "<<p<<"\n";
            return true;
        }

        if(lc=="write"){
            std::istringstream ts(rest); string tok1; ts>>tok1;
            if(tok1.empty()){ cout<<P.warn<<"usage: write [range] <path>"<<C_RESET<<"\n"; return true; }
            bool isrange=false;
            for(char ch: tok1){ if(ch=='-'||std::isdigit((unsigned char)ch)||ch=='$'){ isrange=true; break; } }
            size_t lo=1,hi=buf.lines.size(); string outp;
            if(isrange){
                if(!parse_range(tok1,buf.lines.size(),lo,hi)){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
                ts>>outp; if(outp.empty()){ cout<<P.warn<<"usage: write [range] <path>"<<C_RESET<<"\n"; return true; }
            } else outp=tok1;
            Buffer tmp;
            if(hi>=lo) tmp.lines.assign(buf.lines.begin()+ (long)lo-1, buf.lines.begin()+ (long)hi);
            string err;
            if(atomic_save(outp, tmp, buf.backup, err)){ cout<<"wrote "<<(hi>=lo?hi-lo+1:0)<<" line(s) to "<<outp<<"\n"; }
            else cout<<P.err<<"write: "<<err<<C_RESET<<"\n";
            return true;
        }

        if(lc=="filter"){
            std::istringstream ts(rest); string rng; ts>>rng; string ex; std::getline(ts,ex); ex=trim_copy(ex);
            if(ex.empty()||ex[0]!='!'){ cout<<P.warn<<"usage: filter <range> !shell"<<C_RESET<<"\n"; return true; }
            size_t lo=1,hi=buf.lines.size();
            if(!parse_range(rng,buf.lines.size(),lo,hi)){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
            push_undo();
            string ferr;
            if(run_filter_replace(buf.lines,lo,hi, ex.substr(1), ferr)){ buf.dirty=true; cout<<"filtered\n"; }
            else { cout<<P.err<<"filter failed: "<<ferr<<C_RESET<<"\n"; }
            return true;
        }

        if(lc=="undo"||lc=="u"){
            long k=1; if(!rest.empty()) parse_long(rest,k);
            bool any=false;
            while(k-- > 0){
                Snap s; if(!undo.pop(s)){ if(!any) cout<<"nothing to undo\n"; break; }
                redo.push(buf); buf.lines = std::move(s.lines); buf.dirty=true; any=true;
            }
            if(any) cout<<"undo\n";
            return true;
        }
        if(lc=="redo"){
            Snap s; if(!redo.pop(s)){ cout<<"nothing to redo\n"; return true; }
            undo.push(buf); buf.lines = std::move(s.lines); buf.dirty=true; cout<<"redo\n"; return true;
        }

        if(lc=="set"){
            std::istringstream ts(rest); string what,val; ts>>what>>val; what=lower(what); val=lower(val);
            if(what=="number"){
                if(val=="on"||val=="1"||val=="true"){ buf.number=true; cout<<"number: on\n"; save_config(); }
                else if(val=="off"||val=="0"||val=="false"){ buf.number=false; cout<<"number: off\n"; save_config(); }
                else cout<<P.warn<<"usage: set number on|off"<<C_RESET<<"\n";
            } else if(what=="backup"){
                if(val=="on"||val=="1"||val=="true"){ buf.backup=true; cout<<"backup: on\n"; save_config(); }
                else if(val=="off"||val=="0"||val=="false"){ buf.backup=false; cout<<"backup: off\n"; save_config(); }
                else cout<<P.warn<<"usage: set backup on|off"<<C_RESET<<"\n";
            } else if(what=="autosave"){
                long s=0; if(!parse_long(val,s)){ cout<<P.warn<<"usage: set autosave <seconds>"<<C_RESET<<"\n"; return true; }
                autosave_sec = (int)std::max<long>(0,s);
                cout<<"autosave: "<<autosave_sec<<"s\n"; save_config();
            } else if(what=="wrap"){
                bool b=false; if(!parse_bool_string(val,b)){ cout<<P.warn<<"usage: set wrap on|off"<<C_RESET<<"\n"; return true; }
                wrap_long=b; cout<<"wrap: "<<(wrap_long?"on":"off")<<"\n"; save_config();
            } else if(what=="truncate"){
                bool b=false; if(!parse_bool_string(val,b)){ cout<<P.warn<<"usage: set truncate on|off"<<C_RESET<<"\n"; return true; }
                truncate_long=b; cout<<"truncate: "<<(truncate_long?"on":"off")<<"\n"; save_config();
            } else if(what=="lang"){
                if(val=="cpp"||val=="c"||val=="c++"||val=="hpp"||val=="h") lang=Lang::Cpp;
                else if(val=="py"||val=="python") lang=Lang::Python;
                else if(val=="sh"||val=="bash"||val=="zsh"||val=="shell") lang=Lang::Shell;
                else if(val=="rb"||val=="ruby") lang=Lang::Ruby;
                else if(val=="js"||val=="javascript"||val=="ts"||val=="typescript") lang=Lang::JS;
                else if(val=="html"||val=="htm") lang=Lang::HTML;
                else if(val=="css") lang=Lang::CSS;
                else if(val=="json") lang=Lang::JSON;
                else lang=Lang::Plain;
                cout<<"lang: set\n";
            } else cout<<P.warn<<"unknown setting"<<C_RESET<<"\n";
            return true;
        }

        if(lc=="number"){ buf.number = !buf.number; cout<<"number: "<<(buf.number?"on":"off")<<"\n"; save_config(); return true; }

        if(lc=="theme"){ if(rest.empty()){ cout<<P.warn<<"usage: theme <name>"<<C_RESET<<"\n"; return true; } cycle_theme(rest); return true; }

        if(lc=="highlight"){
            string v=lower(rest);
            if(v=="on"||v=="1"||v=="true"){ buf.highlight=true; cout<<"highlight: on\n"; save_config(); }
            else if(v=="off"||v=="0"||v=="false"){ buf.highlight=false; cout<<"highlight: off\n"; save_config(); }
            else cout<<P.warn<<"usage: highlight on|off"<<C_RESET<<"\n";
            return true;
        }

        if(lc=="alias"){
            std::istringstream ts(rest); string from; ts>>from; string to; std::getline(ts,to); to=trim_copy(to);
            if(from.empty()||to.empty()){ cout<<P.warn<<"usage: alias <from> <to...>"<<C_RESET<<"\n"; return true; }
            aliases[from]=to; cout<<"alias: "<<from<<" -> "<<to<<"\n"; save_config(); return true; }
            if(lc=="new"){ string p=rest; open_new_buffer(p); return true; }
            if(lc=="bnext"){ bnext(); return true; }
            if(lc=="bprev"){ bprev(); return true; }
            if(lc=="lsb"){ list_buffers(); return true; }

            if(lc=="diff"){ show_diff(); return true; }

            if(lc=="pwd"){ std::error_code ec; auto p = fs::current_path(ec); if(ec) cout<<P.err<<"pwd: "<<ec.message()<<C_RESET<<"\n"; else cout<<p.string()<<"\n"; return true; }
            if(lc=="ls"){
                bool all=false,longfmt=false; string target=".";
                std::istringstream ts2(rest); string t; vector<string> args;
                while(ts2>>t) args.push_back(t);
                for(size_t i=0;i<args.size();++i){
                    if(args[i]=="-a") all=true;
                    else if(args[i]=="-l") longfmt=true;
                    else target=args[i];
                }
                if(target.empty()) target=".";
                ls_list(target, all, longfmt);
                return true;
            }

            if(lc=="cd"){
                if(rest.empty()){
                    cout<<P.warn<<"cd: requires a directory path (try ., .., ~, or a folder name)"<<C_RESET<<"\n";
                    return true;
                }
                string target = expand_path(rest);
                std::error_code ec;
                fs::file_status st = fs::status(target, ec);
                if(ec || !fs::exists(st)){
                    cout<<P.err<<"cd: no such directory: "<<target<<C_RESET<<"\n";
                    return true;
                }
                if(!fs::is_directory(st)){
                    cout<<P.err<<"cd: not a directory: "<<target<<C_RESET<<"\n";
                    return true;
                }
                fs::current_path(target, ec);
                if(ec) cout<<P.err<<"cd: "<<ec.message()<<C_RESET<<"\n";
                else cout<<P.ok<<"cd: "<<fs::current_path().string()<<C_RESET<<"\n";
                return true;
            }
// whats up
            if(lc=="clear"){ clear_screen(); return true; }

            if(lc=="lua"){
                if(!L){
                    cout<<P.err<<"lua: not available"<<C_RESET<<"\n";
                    return true;
                }
                if(rest.empty()){
                    cout<<P.warn<<"usage: lua <code>"<<C_RESET<<"\n";
                    return true;
                }
                int rc = luaL_loadstring(L, rest.c_str());
                if(rc != LUA_OK){
                    const char* msg = lua_tostring(L, -1);
                    cout<<P.err<<"lua: "<<(msg?msg:"")<<C_RESET<<"\n";
                    lua_pop(L, 1);
                    return true;
                }
                rc = lua_pcall(L, 0, LUA_MULTRET, 0);
                if(rc != LUA_OK){
                    const char* msg = lua_tostring(L, -1);
                    cout<<P.err<<"lua: "<<(msg?msg:"")<<C_RESET<<"\n";
                    lua_pop(L, 1);
                }
                return true;
            }

            if(lc=="luafile"){
                if(!L){
                    cout<<P.err<<"luafile: lua not available"<<C_RESET<<"\n";
                    return true;
                }
                if(rest.empty()){
                    cout<<P.warn<<"usage: luafile <path>"<<C_RESET<<"\n";
                    return true;
                }
                string p = expand_path(rest);
                int rc = luaL_loadfile(L, p.c_str());
                if(rc != LUA_OK){
                    const char* msg = lua_tostring(L, -1);
                    cout<<P.err<<"luafile: "<<(msg?msg:"")<<C_RESET<<"\n";
                    lua_pop(L, 1);
                    return true;
                }
                rc = lua_pcall(L, 0, 0, 0);
                if(rc != LUA_OK){
                    const char* msg = lua_tostring(L, -1);
                    cout<<P.err<<"luafile: "<<(msg?msg:"")<<C_RESET<<"\n";
                    lua_pop(L, 1);
                }
                return true;
            }

            if(lc=="plugins"){
                if(plugin_names.empty()){
                    cout<<"no plugins loaded\n";
                } else {
                    for(const auto& n : plugin_names) cout<<"- "<<n<<"\n";
                }
                return true;
            }

            if(lc=="reload-plugins"){
                if(!L){
                    cout<<P.err<<"reload-plugins: lua not available"<<C_RESET<<"\n";
                    return true;
                }
                load_lua_plugins();
                cout<<"plugins reloaded\n";
                return true;
            }

            if(lc=="version" || lc=="ver"){
                cout<<P.title<<"tedit "<<TEDIT_VERSION<<C_RESET<<"\n";
                return true;
            }

            cout<<P.warn<<"unknown command — type 'help'"<<C_RESET<<"\n"; return true;
    }
};

/* ------------------------------------------------------------------ */
/*                      Lua bridge functions                          */
/* ------------------------------------------------------------------ */

static int l_tedit_echo(lua_State* L){
    const char* s = luaL_checkstring(L, 1);
    if(g_editor){
        cout<<g_editor->P.accent;
        if(s) cout<<s;
        cout<<C_RESET<<"\n";
    }
    return 0;
}

static int l_tedit_command(lua_State* L){
    const char* s = luaL_checkstring(L, 1);
    if(g_editor && s){
        string cmd = s;
        if(!cmd.empty()) g_editor->handle(cmd);
    }
    return 0;
}

static int l_tedit_print(lua_State* L){
    lua_Integer ln = luaL_checkinteger(L, 1);
    if(g_editor){
        if(ln >= 1 && (size_t)ln <= g_editor->buf.lines.size()){
            g_editor->print((size_t)ln, (size_t)ln);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*                               main                                 */
/* ------------------------------------------------------------------ */
int main(int argc, char** argv){
    std::ios::sync_with_stdio(false); std::cin.tie(nullptr);
    Editor ed;

    ed.load_config();

    if(argc>=2){ ed.load(argv[1]); } else { ed.buf.path.clear(); }

    ed.banner();
    cout<<palette_for(ed.theme).accent<<"tedit — editing "
    <<( ed.buf.path.empty()? "(unnamed)": ed.buf.path )<<" ("
    <<ed.buf.lines.size()<<" lines). Type 'help'."<<C_RESET<<"\n";
    ed.tip();

    for(;;){
        ed.status();
        string line = ed.lr.read(ed.prompt_str());
        if(!std::cin.good() && line.empty()){ cout<<"\n"; break; }
        if(line.empty()) continue;
        ed.lr.remember(line);
        bool keep = ed.handle(line);
        if(!keep) break;
    }
    return 0;
}
