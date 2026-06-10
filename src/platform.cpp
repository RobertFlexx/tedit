static inline bool is_tty_stdout() {
    #if defined(__unix__) || defined(__APPLE__)
    return isatty(STDOUT_FILENO);
    #else
    return false;
    #endif
}


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


static int run_shell_cmd(const string &cmd) {
    return std::system(cmd.c_str());
}

static int run_exec_file(const string& path, const vector<string>& args) {
#if defined(__unix__) || defined(__APPLE__)
    pid_t pid = fork();
    if(pid < 0) return -1;
    if(pid == 0){
        vector<char*> argv;
        argv.push_back(const_cast<char*>(path.c_str()));
        for(const auto& a: args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(path.c_str(), argv.data());
        _exit(127);
    }
    int st = 0;
    while(waitpid(pid, &st, 0) < 0){
        if(errno != EINTR) return -1;
    }
    if(WIFEXITED(st)) return WEXITSTATUS(st);
    return -1;
#else
    string cmd = sh_escape(path);
    for(const auto& a: args) cmd += " " + sh_escape(a);
    return run_shell_cmd(cmd);
#endif
}


static bool safe_backup_copy(const string &src, const string &dst, string &err) {
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
    string themes = root + "/themes";
    fs::create_directories(themes, ec);
    return root;
}

static string tedit_plugins_dir(){
    return tedit_config_dir() + "/plugins";
}

static string tedit_themes_dir(){
    return tedit_config_dir() + "/themes";
}

static string tedit_recovery_dir(){
    string dir = tedit_config_dir() + "/recovery";
    std::error_code ec;
    fs::create_directories(dir, ec);
    chmod(dir.c_str(), 0700);
    return dir;
}
