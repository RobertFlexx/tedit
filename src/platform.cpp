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
  int status = std::system(cmd.c_str());
  if (status < 0)
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  if (WIFEXITED(status))
    return WEXITSTATUS(status);
  if (WIFSIGNALED(status))
    return 128 + WTERMSIG(status);
#endif
  return status;
}

static int run_exec_file(const string &path, const vector<string> &args) {
#if defined(__unix__) || defined(__APPLE__)
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid == 0) {
    vector<char *> argv;
    argv.push_back(const_cast<char *>(path.c_str()));
    for (const auto &a : args)
      argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);
    execv(path.c_str(), argv.data());
    _exit(127);
  }
  int st = 0;
  while (waitpid(pid, &st, 0) < 0) {
    if (errno != EINTR)
      return -1;
  }
  if (WIFEXITED(st))
    return WEXITSTATUS(st);
  return -1;
#else
  string cmd = sh_escape(path);
  for (const auto &a : args)
    cmd += " " + sh_escape(a);
  return run_shell_cmd(cmd);
#endif
}

static int run_exec_program(const string &program, const vector<string> &args) {
#if defined(__unix__) || defined(__APPLE__)
  pid_t pid = fork();
  if (pid < 0)
    return -1;
  if (pid == 0) {
    vector<char *> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char *>(program.c_str()));
    for (const auto &arg : args)
      argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);
    execvp(program.c_str(), argv.data());
    _exit(127);
  }
  int st = 0;
  while (waitpid(pid, &st, 0) < 0) {
    if (errno != EINTR)
      return -1;
  }
  if (WIFEXITED(st))
    return WEXITSTATUS(st);
  return -1;
#else
  string cmd = sh_escape(program);
  for (const auto &arg : args)
    cmd += " " + sh_escape(arg);
  return run_shell_cmd(cmd);
#endif
}

static bool write_all(int fd, const char *data, size_t size, string &err,
                      const char *operation) {
  size_t offset = 0;
  while (offset < size) {
    ssize_t written = ::write(fd, data + offset, size - offset);
    if (written < 0) {
      if (errno == EINTR)
        continue;
      err = string(operation) + ": " + std::strerror(errno);
      return false;
    }
    if (written == 0) {
      err = string(operation) + ": short write";
      return false;
    }
    offset += (size_t)written;
  }
  return true;
}

static bool safe_backup_copy(const string &src, const string &dst,
                             string &err) {
  int sfd = ::open(src.c_str(), O_RDONLY);
  if (sfd < 0) {

    return true;
  }

  string tmp = dst + ".tmp.XXXXXX";
  vector<char> tbuf(tmp.begin(), tmp.end());
  tbuf.push_back('\0');
  int dfd = ::mkstemp(tbuf.data());
  if (dfd < 0) {
    err = "backup open: " + string(std::strerror(errno));
    ::close(sfd);
    return false;
  }
  (void)::fchmod(dfd, 0600);

  char buf[4096];
  ssize_t r;
  while ((r = ::read(sfd, buf, sizeof(buf))) != 0) {
    if (r < 0) {
      if (errno == EINTR)
        continue;
      err = "backup read: " + string(std::strerror(errno));
      ::close(sfd);
      ::close(dfd);
      ::unlink(tbuf.data());
      return false;
    }
    if (!write_all(dfd, buf, (size_t)r, err, "backup write")) {
      ::close(sfd);
      ::close(dfd);
      ::unlink(tbuf.data());
      return false;
    }
  }

  ::close(sfd);
  if (::fsync(dfd) != 0) {
    err = "backup fsync: " + string(std::strerror(errno));
    ::close(dfd);
    ::unlink(tbuf.data());
    return false;
  }
  if (::close(dfd) != 0) {
    err = "backup close: " + string(std::strerror(errno));
    ::unlink(tbuf.data());
    return false;
  }
  if (::rename(tbuf.data(), dst.c_str()) != 0) {
    err = "backup rename: " + string(std::strerror(errno));
    ::unlink(tbuf.data());
    return false;
  }
  return true;
}

static inline string home_path() {
  const char *h = getenv("HOME");
  if (!h || !*h)
    h = getenv("USERPROFILE");
  if (!h || !*h) {
    struct passwd *pw = ::getpwuid(::geteuid());
    if (pw && pw->pw_dir && *pw->pw_dir)
      h = pw->pw_dir;
  }
  if (!h || !*h)
    return string(".");
  return string(h);
}

static inline bool file_exists(const string &p) {
  struct stat st{};
  return ::stat(p.c_str(), &st) == 0;
}

static string tedit_config_dir() {
  static string cached;
  if (!cached.empty())
    return cached;
  string base = home_path();
  string root = base + "/tedit-config";
  std::error_code ec;
  fs::create_directories(root, ec);
  string plugins = root + "/plugins";
  fs::create_directories(plugins, ec);
  string themes = root + "/themes";
  fs::create_directories(themes, ec);
  cached = root;
  return cached;
}

static string tedit_plugins_dir() { return tedit_config_dir() + "/plugins"; }

static string tedit_themes_dir() { return tedit_config_dir() + "/themes"; }

static string tedit_recovery_dir() {
  string dir = tedit_config_dir() + "/recovery";
  std::error_code ec;
  fs::create_directories(dir, ec);
  chmod(dir.c_str(), 0700);
  return dir;
}
