struct LineReader {
  vector<string> history;
  size_t HIST_MAX = 800;
  vector<string> commands;
  string color_input = "";
  string color_reset = C_RESET;

  void set_theme_colors(const ThemePalette &P) {
    color_input = P.input;
    color_reset = C_RESET;
  }

  static vector<string> split_words(const string &s) {
    vector<string> v;
    std::istringstream in(s);
    string w;
    while (in >> w)
      v.push_back(w);
    return v;
  }

  static string expand_home_in_token(string in) {
    if (in.empty())
      return in;
    if (in == "~")
      return home_path();
    if (in.size() >= 2 && in[0] == '~' && in[1] == '/') {
      return home_path() + in.substr(1);
    }
    return in;
  }

  static string quote_completion(const string &value) {
    if (value.find_first_of(" \t\"\\") == string::npos)
      return value;
    string out = "\"";
    for (char c : value) {
      if (c == '\"' || c == '\\')
        out.push_back('\\');
      out.push_back(c);
    }
    out.push_back('\"');
    return out;
  }

  static vector<string> complete_dirs_only(const string &token) {
    string t = expand_home_in_token(token);
    vector<string> out;
    string base = t, dir = ".";
    auto pos = t.find_last_of('/');
    if (pos != string::npos) {
      dir = t.substr(0, pos);
      base = t.substr(pos + 1);
    }
    std::error_code ec;
    for (auto &e : fs::directory_iterator(
             dir, fs::directory_options::skip_permission_denied, ec)) {
      string name = e.path().filename().string();
      std::error_code type_ec;
      if (name.rfind(base, 0) == 0 && e.is_directory(type_ec) && !type_ec) {
        string cand = (dir == "." ? name : dir + "/" + name);
        out.push_back(cand + "/");
      }
    }
    std::sort(out.begin(), out.end());
    return out;
  }

  static vector<string> complete_fs(const string &token) {
    string expanded = expand_home_in_token(token);
    vector<string> out;
    string base = expanded, dir = "";
    auto pos = expanded.find_last_of('/');
    if (pos != string::npos) {
      dir = expanded.substr(0, pos);
      base = expanded.substr(pos + 1);
    } else
      dir = ".";
    std::error_code ec;
    for (auto &e : fs::directory_iterator(
             dir, fs::directory_options::skip_permission_denied, ec)) {
      string name = e.path().filename().string();
      if (name.rfind(base, 0) == 0) {
        string cand = (dir == "." ? name : dir + "/" + name);
        std::error_code type_ec;
        if (e.is_directory(type_ec) && !type_ec)
          cand += "/";
        out.push_back(cand);
      }
    }
    std::sort(out.begin(), out.end());
    return out;
  }

  vector<string> complete(const string &buf) {
    auto toks = split_words(buf);
    vector<string> out;
    bool at_start = toks.empty();
    bool fresh = (!buf.empty() && std::isspace((unsigned char)buf.back()));

    if (at_start) {
      out = commands;
      return out;
    }

    if (toks.size() == 1 && !fresh) {
      string pref = toks[0];
      for (const auto &c : commands)
        if (c.rfind(pref, 0) == 0)
          out.push_back(c);
      return out;
    }

    string first = toks[0];

    if (first == "cd") {
      if (toks.size() == 1 && fresh) {
        return complete_dirs_only("");
      }
      string last = fresh ? string("") : toks.back();
      return complete_dirs_only(last);
    }

    string last = fresh ? string("") : toks.back();
    return complete_fs(last);
  }

  void remember(const string &s) {
    if (s.empty())
      return;
    if (history.empty() || history.back() != s) {
      if (history.size() < HIST_MAX)
        history.push_back(s);
      else {
        history.erase(history.begin());
        history.push_back(s);
      }
    }
  }

  string read(const string &prompt) {
    bool tty = false;
#if defined(__unix__) || defined(__APPLE__)
    tty = isatty(STDIN_FILENO);
#endif
    if (!tty) {
      cout << prompt << std::flush;
      string s;
      if (!std::getline(std::cin, s))
        return string();
      return s;
    }

#if defined(__unix__) || defined(__APPLE__)
    cout << prompt << std::flush;

    struct termios orig{};
    bool have_orig = false;
    if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &orig) != -1) {
      struct termios t = orig;
      // Read control keys ourselves so Ctrl-C/Ctrl-D cannot leave the
      // terminal stuck in non-canonical mode.
      t.c_lflag &= ~(ECHO | ICANON | ISIG);
      t.c_iflag &= ~(IXON | ICRNL);
      t.c_cc[VMIN] = 1;
      t.c_cc[VTIME] = 0;
      if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &t) == 0) {
        have_orig = true;
      }
    }

    struct TermiosGuard {
      bool active;
      struct termios o;
      TermiosGuard(bool a, const struct termios &oo) : active(a), o(oo) {}
      ~TermiosGuard() {
        if (active)
          tcsetattr(STDIN_FILENO, TCSAFLUSH, &o);
      }
    } guard(have_orig, orig);

    string buf;
    size_t cursor = 0;
    int hist_idx = (int)history.size();

    auto refresh = [&]() {
      string shown = escape_for_display(buf);
      cout << "\r\033[2K" << prompt << color_input << shown << color_reset;
      size_t tail = display_columns(escape_for_display(buf.substr(cursor)));
      if (tail > 0)
        cout << "\033[" << tail << "D";
      cout.flush();
    };

    auto history_up = [&]() {
      if (hist_idx > 0) {
        hist_idx--;
        buf = history[(size_t)hist_idx];
        cursor = buf.size();
        refresh();
      }
    };
    auto history_down = [&]() {
      if (hist_idx < (int)history.size() - 1) {
        hist_idx++;
        buf = history[(size_t)hist_idx];
        cursor = buf.size();
      } else {
        hist_idx = (int)history.size();
        buf.clear();
        cursor = 0;
      }
      refresh();
    };
    auto move_word_left = [&]() {
      while (
          cursor > 0 &&
          std::isspace((unsigned char)buf[previous_utf8_boundary(buf, cursor)]))
        cursor = previous_utf8_boundary(buf, cursor);
      while (cursor > 0 &&
             !std::isspace(
                 (unsigned char)buf[previous_utf8_boundary(buf, cursor)]))
        cursor = previous_utf8_boundary(buf, cursor);
      refresh();
    };
    auto move_word_right = [&]() {
      while (cursor < buf.size() && std::isspace((unsigned char)buf[cursor]))
        cursor = next_utf8_boundary(buf, cursor);
      while (cursor < buf.size() && !std::isspace((unsigned char)buf[cursor]))
        cursor = next_utf8_boundary(buf, cursor);
      refresh();
    };

    refresh();
    while (true) {
      char c = 0;
      ssize_t n = ::read(STDIN_FILENO, &c, 1);
      if (n <= 0)
        return string();
      if (c == '\r' || c == '\n') {
        cout << "\r\n";
        break;
      } else if ((unsigned char)c == 127 || c == '\b') {
        if (cursor > 0) {
          size_t previous = previous_utf8_boundary(buf, cursor);
          buf.erase(previous, cursor - previous);
          cursor = previous;
          refresh();
        }
      } else if (c == '\t') {
        auto opts = complete(buf);
        if (opts.empty()) {
        } else if (opts.size() == 1) {
          auto toks = split_words(buf);
          size_t lastsp = buf.find_last_of(' ');
          string prefix =
              (lastsp == string::npos ? string() : buf.substr(0, lastsp + 1));
          buf = prefix + quote_completion(opts[0]);
          cursor = buf.size();
          refresh();
        } else {
          cout << "\r\n";
          size_t shown = 0;
          for (auto &o : opts) {
            cout << escape_for_display(o) << "  ";
            if (++shown % 6 == 0)
              cout << "\r\n";
          }
          if ((shown % 6) != 0)
            cout << "\r\n";
          refresh();
        }
      } else if (c == 27) {
        char first = 0;
        if (::read(STDIN_FILENO, &first, 1) <= 0)
          continue;
        if (first == 'b') {
          move_word_left();
          continue;
        }
        if (first == 'f') {
          move_word_right();
          continue;
        }
        if (first == '[') {
          string sequence;
          for (size_t count = 0; count < 12; ++count) {
            char key = 0;
            if (::read(STDIN_FILENO, &key, 1) <= 0)
              break;
            sequence.push_back(key);
            if (std::isalpha((unsigned char)key) || key == '~')
              break;
          }
          if (sequence == "A")
            history_up();
          else if (sequence == "B")
            history_down();
          else if (sequence == "C" && cursor < buf.size()) {
            cursor = next_utf8_boundary(buf, cursor);
            refresh();
          } else if (sequence == "D" && cursor > 0) {
            cursor = previous_utf8_boundary(buf, cursor);
            refresh();
          } else if (sequence == "1;5D")
            move_word_left();
          else if (sequence == "1;5C")
            move_word_right();
          else if (sequence == "H" || sequence == "1~" || sequence == "7~") {
            cursor = 0;
            refresh();
          } else if (sequence == "F" || sequence == "4~" || sequence == "8~") {
            cursor = buf.size();
            refresh();
          } else if (sequence == "3~" && cursor < buf.size()) {
            size_t next = next_utf8_boundary(buf, cursor);
            buf.erase(cursor, next - cursor);
            refresh();
          }
        }
      } else if (c == 1) {
        cursor = 0;
        refresh();
      } else if (c == 5) {
        cursor = buf.size();
        refresh();
      } else if (c == 3) {
        buf.clear();
        cursor = 0;
        cout << "^C\r\n";
        break;
      } else if (c == 4) {
        if (buf.empty()) {
          std::cin.setstate(std::ios::eofbit);
          cout << "\r\n";
          return string();
        }
        if (cursor < buf.size()) {
          size_t next = next_utf8_boundary(buf, cursor);
          buf.erase(cursor, next - cursor);
          refresh();
        }
      } else if (c == 11) {
        buf.erase(cursor);
        refresh();
      } else if (c == 12) {
        cout << "\033[H\033[2J";
        refresh();
      } else if (c == 21) {
        buf.erase(0, cursor);
        cursor = 0;
        refresh();
      } else if (c == 23) {
        size_t start = cursor;
        while (start > 0 &&
               std::isspace(
                   (unsigned char)buf[previous_utf8_boundary(buf, start)]))
          start = previous_utf8_boundary(buf, start);
        while (start > 0 &&
               !std::isspace(
                   (unsigned char)buf[previous_utf8_boundary(buf, start)]))
          start = previous_utf8_boundary(buf, start);
        buf.erase(start, cursor - start);
        cursor = start;
        refresh();
      } else if (c == 17 || c == 19 || c == 26 || c == 28) {
        // Ignore suspend/quit controls while raw editing is active.
      } else if ((unsigned char)c >= 0x20) {
        string input(1, c);
        unsigned char lead = (unsigned char)c;
        int continuation = lead >= 0xf0   ? 3
                           : lead >= 0xe0 ? 2
                           : lead >= 0xc0 ? 1
                                          : 0;
        while (continuation-- > 0) {
          char next = 0;
          if (::read(STDIN_FILENO, &next, 1) <= 0)
            break;
          input.push_back(next);
        }
        buf.insert(cursor, input);
        cursor += input.size();
        refresh();
      }
    }
    return buf;
#else
    string s;
    cout << prompt;
    if (!std::getline(std::cin, s))
      return string();
    return s;
#endif
  }
};
