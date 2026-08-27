int main(int argc, char **argv) {
  (void)std::setlocale(LC_CTYPE, "");
  (void)std::signal(SIGPIPE, SIG_IGN);
  std::ios::sync_with_stdio(false);
  std::cin.tie(nullptr);

  vector<string> paths;
  bool show_banner = true;
  bool force_no_status = false;
  bool options = true;
  for (int i = 1; i < argc; ++i) {
    string arg = argv[i];
    if (options && arg == "--") {
      options = false;
      continue;
    }
    if (options && (arg == "--version" || arg == "-V")) {
      cout << "tedit " << TEDIT_VERSION << "\n";
      return 0;
    }
    if (options && (arg == "--help" || arg == "-h")) {
      cout << "usage: tedit [options] [--] [file ...]\n"
           << "       tedit --help\n"
           << "       tedit --version\n"
           << "\n"
           << "Options:\n"
           << "  --no-banner   skip the optional ~/.tedit_banner\n"
           << "  --no-status   hide the per-command status line for this run\n"
           << "  --            treat remaining arguments as file paths\n"
           << "\n"
           << "Open one or more files. Extra files start as buffers.\n";
      return 0;
    }
    if (options && arg == "--no-banner") {
      show_banner = false;
      continue;
    }
    if (options && arg == "--no-status") {
      force_no_status = true;
      continue;
    }
    if (options && arg.size() > 1 && arg[0] == '-') {
      cerr << "tedit: unknown option: " << escape_for_display(arg)
           << "\nTry 'tedit --help'.\n";
      return 2;
    }
    paths.push_back(std::move(arg));
  }

  Editor ed;

  ed.load_config();
  if (force_no_status)
    ed.show_status_line = false;

  if (!paths.empty()) {
    if (!ed.load(paths[0], false))
      return 1;
    for (size_t i = 1; i < paths.size(); ++i) {
      if (!ed.add_background_buffer(paths[i], false))
        return 1;
    }
    ed.save_config();
  } else {
    ed.buf.path.clear();
  }

  if (show_banner && !ed.ed_mode)
    ed.banner();
  if (!ed.ed_mode)
    cout << ed.P.title << "tedit " << TEDIT_VERSION << C_RESET << "\n"
         << ed.P.dim << "file: " << C_RESET
         << (ed.buf.path.empty() ? "(unnamed)"
                                 : escape_for_display(ed.buf.path))
         << "\n"
         << ed.P.dim << "lines: " << C_RESET << ed.buf.lines.size() << "  "
         << ed.P.dim << "buffers: " << C_RESET << ed.buffer_count() << "  "
         << ed.P.dim << "help: " << C_RESET << "help, help <command>" << "\n";
  if (!ed.ed_mode)
    ed.tip();

  for (;;) {
    if (ed.show_status_line && !ed.ed_mode)
      ed.status();
    string line = ed.lr.read(ed.prompt_str());
    if (!std::cin.good() && line.empty()) {
      size_t recovered = ed.snapshot_dirty_buffers();
      cout << "\n";
      if (recovered)
        cout << ed.P.warn << "input closed; wrote " << recovered
             << " recovery snapshot" << (recovered == 1 ? "" : "s") << C_RESET
             << "\n";
      break;
    }
    if (line.empty()) {
      if (ed.ed_mode) {
        size_t next = ed.buf.current_line + 1;
        if (next >= 1 && next <= ed.buf.lines.size())
          ed.print(next, next);
        else
          cout << "?\n";
      }
      continue;
    }
    ed.lr.remember(line);
    bool keep = ed.handle(line);
    if (!keep)
      break;
  }
  return 0;
}
