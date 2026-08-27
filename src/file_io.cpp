static bool load_file(const string &path, Buffer &b, string &err) {
  int flags = O_RDONLY;
#ifdef O_CLOEXEC
  flags |= O_CLOEXEC;
#endif
#ifdef O_NONBLOCK
  flags |= O_NONBLOCK;
#endif
  int fd = ::open(path.c_str(), flags);
  if (fd < 0) {
    if (errno == ENOENT) {
      b.lines.clear();
      b.final_newline = true;
      b.dirty = false;
      return true;
    }
    err = string("open: ") + std::strerror(errno);
    return false;
  }
  struct stat st{};
  if (::fstat(fd, &st) != 0) {
    err = string("fstat: ") + std::strerror(errno);
    ::close(fd);
    return false;
  }
  if (!S_ISREG(st.st_mode)) {
    err = "not a regular file";
    ::close(fd);
    return false;
  }
#ifdef O_NONBLOCK
  int current_flags = ::fcntl(fd, F_GETFL);
  if (current_flags >= 0)
    (void)::fcntl(fd, F_SETFL, current_flags & ~O_NONBLOCK);
#endif
  FILE *input = ::fdopen(fd, "rb");
  if (!input) {
    err = string("fdopen: ") + std::strerror(errno);
    ::close(fd);
    return false;
  }
  vector<string> loaded;
  char *data = nullptr;
  size_t capacity = 0;
  ssize_t length = 0;
  bool final_newline = false;
  while ((length = ::getline(&data, &capacity, input)) >= 0) {
    string line(data, (size_t)length);
    final_newline = !line.empty() && line.back() == '\n';
    if (final_newline) {
      line.pop_back();
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
    }
    loaded.push_back(std::move(line));
  }
  bool failed = ::ferror(input) != 0;
  std::free(data);
  if (::fclose(input) != 0)
    failed = true;
  if (failed) {
    err = "read failed";
    return false;
  }
  b.lines.swap(loaded);
  b.final_newline = b.lines.empty() ? false : final_newline;
  b.current_line = b.lines.size();
  b.dirty = false;
  b.undo.clear();
  b.redo.clear();
  return true;
}

static int fsync_dir_of(const string &path) {
  string dir = path;
  auto pos = dir.find_last_of('/');
  if (pos == string::npos)
    dir = ".";
  else if (pos == 0)
    dir = "/";
  else
    dir = dir.substr(0, pos);

  int dfd = open(dir.c_str(), O_RDONLY
#ifdef O_DIRECTORY
                                  | O_DIRECTORY
#endif
  );
  if (dfd < 0)
    return -1;
  int rc = fsync(dfd);
  int e = errno;
  close(dfd);
  errno = e;
  return rc;
}

static bool atomic_save_to_fd(FILE *tf, const Buffer &b, string &err) {
  for (size_t i = 0; i < b.lines.size(); ++i) {
    const auto &line = b.lines[i];
    bool write_newline = i + 1 < b.lines.size() || b.final_newline;
    if ((!line.empty() &&
         fwrite(line.data(), 1, line.size(), tf) != line.size()) ||
        (write_newline && fputc('\n', tf) == EOF)) {
      err = string("write: ") + strerror(errno);
      fclose(tf);
      return false;
    }
  }
  if (fflush(tf) != 0) {
    err = string("flush: ") + strerror(errno);
    fclose(tf);
    return false;
  }
  if (fsync(fileno(tf)) < 0) {
    err = string("fsync: ") + strerror(errno);
    fclose(tf);
    return false;
  }
  if (fclose(tf) != 0) {
    err = string("close: ") + strerror(errno);
    return false;
  }
  return true;
}

static bool doas_move_into_place_secure(const string &tmp, const string &dest,
                                        string &err) {
  int rc = run_exec_program("doas", {"mv", tmp, dest});
  if (rc != 0) {
    err = "doas move failed (exit " + std::to_string(rc) + ")";
    return false;
  }
  return true;
}

static bool atomic_save(const string &path, const Buffer &b, bool backup,
                        string &err) {
  mode_t mode = 0644;
  struct stat st{};
  bool target_exists = ::stat(path.c_str(), &st) == 0;
  if (target_exists) {
    if (!S_ISREG(st.st_mode)) {
      err = "target is not a regular file";
      return false;
    }
    mode = st.st_mode & 0777;
  }

  if (backup && target_exists) {
    string berr;
    if (!safe_backup_copy(path, path + "~", berr)) {
      err = berr;
      return false;
    }
  }

  string tmp = path + ".tmp.XXXXXX";
  vector<char> tbuf(tmp.begin(), tmp.end());
  tbuf.push_back('\0');
  int tfd = mkstemp(tbuf.data());
  if (tfd < 0) {
    err = string("mkstemp: ") + strerror(errno);
    return false;
  }
  (void)fchmod(tfd, mode);
  FILE *tf = fdopen(tfd, "wb");
  if (!tf) {
    err = string("fdopen: ") + strerror(errno);
    close(tfd);
    unlink(tbuf.data());
    return false;
  }
  if (!atomic_save_to_fd(tf, b, err)) {
    unlink(tbuf.data());
    return false;
  }

  if (::rename(tbuf.data(), path.c_str()) < 0) {
    int rename_errno = errno;
    string err2;
    if ((rename_errno != EACCES && rename_errno != EPERM) ||
        !doas_move_into_place_secure(tbuf.data(), path, err2)) {
      err = "rename: " + string(strerror(rename_errno));
      if (!err2.empty())
        err += " ; " + err2;
      unlink(tbuf.data());
      return false;
    }
  }

  (void)fsync_dir_of(path);
  return true;
}

static string recover_path_for(const Buffer &b) {
  string p = b.path.empty() ? ".unnamed" : b.path;
  std::hash<string> H;
  size_t h = H(p);
  std::ostringstream ss;
  ss << tedit_recovery_dir() << "/" << std::hex << h << ".recover";
  return ss.str();
}
static string legacy_recover_path_for(const Buffer &b) {
  string p = b.path.empty() ? ".unnamed" : b.path;
  std::hash<string> H;
  size_t h = H(p);
  std::ostringstream ss;
  ss << home_path() << "/.tedit-recover-" << std::hex << h;
  return ss.str();
}

static bool write_recovery_snapshot(const Buffer &b, string &err) {
  string rp = recover_path_for(b);
  string tmp = rp + ".tmp.XXXXXX";
  vector<char> tbuf(tmp.begin(), tmp.end());
  tbuf.push_back('\0');
  int fd = ::mkstemp(tbuf.data());
  if (fd < 0) {
    err = string("mkstemp: ") + strerror(errno);
    return false;
  }
  (void)::fchmod(fd, 0600);
  FILE *f = ::fdopen(fd, "wb");
  if (!f) {
    err = string("fdopen: ") + strerror(errno);
    ::close(fd);
    ::unlink(tbuf.data());
    return false;
  }
  if (!atomic_save_to_fd(f, b, err)) {
    ::unlink(tbuf.data());
    return false;
  }
  if (::rename(tbuf.data(), rp.c_str()) != 0) {
    err = string("rename: ") + strerror(errno);
    ::unlink(tbuf.data());
    return false;
  }
  (void)fsync_dir_of(rp);
  return true;
}

static void autosave_if_needed(const Buffer &b,
                               std::chrono::steady_clock::time_point &last,
                               int interval_sec) {
  if (interval_sec <= 0)
    return;
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::seconds>(now - last).count() <
      interval_sec)
    return;
  if (!b.dirty) {
    last = now;
    return;
  }
  string ignored;
  (void)write_recovery_snapshot(b, ignored);
  last = now;
}
static bool load_recovery_from(const string &rp, Buffer &b) {
  struct stat rst{};
  if (::lstat(rp.c_str(), &rst) != 0 || !S_ISREG(rst.st_mode) ||
      rst.st_uid != ::geteuid())
    return false;
  if (!b.path.empty()) {
    std::error_code rec_ec, file_ec;
    auto rec_time = fs::last_write_time(rp, rec_ec);
    auto file_time = fs::last_write_time(b.path, file_ec);
    if (!rec_ec && !file_ec && rec_time <= file_time)
      return false;
  }
  cout << C_YEL << "recovery: found snapshot " << rp << C_RESET << "\n";
  Buffer recovered;
  string load_error;
  if (!load_file(rp, recovered, load_error))
    return false;
  b.lines = std::move(recovered.lines);
  b.final_newline = recovered.final_newline;
  b.current_line = b.lines.size();
  b.dirty = true;
  b.undo.clear();
  b.redo.clear();
  return true;
}
static bool maybe_recover(Buffer &b) {
  if (load_recovery_from(recover_path_for(b), b))
    return true;
  return load_recovery_from(legacy_recover_path_for(b), b);
}
