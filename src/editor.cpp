struct Editor {
  Buffer buf;
  LineReader lr;

  Theme theme = Theme::Default;
  ThemePalette P = palette_for(theme);

  string active_lua_theme;

  vector<Buffer> others;
  string last_search;
  bool last_icase = false;
  size_t last_index = 0;
  int autosave_sec = 120;
  std::chrono::steady_clock::time_point last_autosave =
      std::chrono::steady_clock::now();
  std::map<string, string> aliases;
  vector<string> recent_files;
  vector<string> messages;
  vector<string> trusted_plugins;

  bool wrap_long = true;
  bool truncate_long = false;
  bool show_status_line = true;
  int indent_width = 4;
  bool ed_mode = false;
  bool ed_quit_warned = false;

  Lang lang = Lang::Plain;

  lua_State *L = nullptr;
  LspClient lsp;
  vector<string> plugin_names;
  std::map<string, string> plugin_files;
  string current_plugin;

  Editor() {
    g_editor = this;
    lr.commands = {
        "help",       "open",    "info",       "write",     "w",
        "wq",         "saveas",  "quit",       "q",         "print",
        "p",          "r",       "append",     "a",         "insert",
        "i",          "edit",    "delete",     "d",         "move",
        "m",          "join",    "find",       "findi",     "findre",
        "findrei",    "repl",    "replg",      "read",      "undo",
        "u",          "redo",    "set",        "indent",    "filter",
        "ls",         "pwd",     "number",     "goto",      "n",
        "N",          "new",     "bnext",      "bprev",     "lsb",
        "buffer",     "close",   "theme",      "highlight", "alias",
        "diff",       "cd",      "clear",      "version",   "lsp",
        "lua",        "luafile", "run-plugin", "plugins",   "reload-plugins",
        "lua-themes", "config",  "recent",     "messages",  "history",
        "aliases",    "syntax",  "plugin",     "edmode",    "w!",
        "q!",         "quit!",   "write!"};
    lr.set_theme_colors(P);
    init_lua();
  }

  ~Editor() {
    close_lua();
    if (g_editor == this)
      g_editor = nullptr;
  }

  string cfg_path() const { return home_path() + "/.teditrc"; }
  string legacy_cfg_path() const { return tedit_config_dir() + "/.teditrc"; }

  static string theme_name(Theme t) {
    switch (t) {
    case Theme::Dark:
      return "dark";
    case Theme::Neon:
      return "neon";
    case Theme::Matrix:
      return "matrix";
    case Theme::Paper:
      return "paper";
    case Theme::Yellow:
      return "yellow";
    case Theme::Iceberg:
      return "iceberg";
    case Theme::Ocean:
      return "ocean";
    case Theme::Solarized:
      return "solarized";
    case Theme::Gruvbox:
      return "gruvbox";
    case Theme::Rose:
      return "rose";
    default:
      return "default";
    }
  }
  static bool theme_from_name(const string &s, Theme &out) {
    string n = lower(s);
    if (n == "default") {
      out = Theme::Default;
      return true;
    }
    if (n == "dark") {
      out = Theme::Dark;
      return true;
    }
    if (n == "neon") {
      out = Theme::Neon;
      return true;
    }
    if (n == "matrix") {
      out = Theme::Matrix;
      return true;
    }
    if (n == "paper") {
      out = Theme::Paper;
      return true;
    }
    if (n == "yellow") {
      out = Theme::Yellow;
      return true;
    }
    if (n == "iceberg") {
      out = Theme::Iceberg;
      return true;
    }
    if (n == "ocean") {
      out = Theme::Ocean;
      return true;
    }
    if (n == "solarized") {
      out = Theme::Solarized;
      return true;
    }
    if (n == "gruvbox") {
      out = Theme::Gruvbox;
      return true;
    }
    if (n == "rose") {
      out = Theme::Rose;
      return true;
    }
    return false;
  }
  static bool parse_bool_string(const string &v, bool &out) {
    string s = lower(trim_copy(v));
    if (s == "1" || s == "on" || s == "true" || s == "yes") {
      out = true;
      return true;
    }
    if (s == "0" || s == "off" || s == "false" || s == "no") {
      out = false;
      return true;
    }
    return false;
  }
  static string esc(const string &in) {
    string r;
    r.reserve(in.size());
    for (char ch : in) {
      if (ch == '\\')
        r += "\\\\";
      else if (ch == '\t')
        r += "\\t";
      else if (ch == '\n')
        r += "\\n";
      else if (ch == '\r')
        r += "\\r";
      else
        r.push_back(ch);
    }
    return r;
  }
  static string unesc(const string &in) {
    string r;
    r.reserve(in.size());
    bool escaped = false;
    for (char ch : in) {
      if (!escaped) {
        if (ch == '\\')
          escaped = true;
        else
          r.push_back(ch);
      } else {
        if (ch == 't')
          r.push_back('\t');
        else if (ch == 'n')
          r.push_back('\n');
        else if (ch == 'r')
          r.push_back('\r');
        else
          r.push_back(ch);
        escaped = false;
      }
    }
    if (escaped)
      r.push_back('\\');
    return r;
  }

  static bool looks_like_range_token(const string &s) {
    if (s.empty())
      return false;
    bool has_value = false;
    for (char ch : s) {
      if (std::isdigit((unsigned char)ch) || ch == '$') {
        has_value = true;
        continue;
      }
      if (ch == '-')
        continue;
      return false;
    }
    return has_value;
  }

  string onoff(bool v) const { return v ? "on" : "off"; }
  size_t buffer_count() const { return others.size() + 1; }
  size_t current_buffer_index() const { return 0; }
  size_t dirty_background_count() const {
    return (size_t)std::count_if(others.begin(), others.end(),
                                 [](const Buffer &b) { return b.dirty; });
  }

  size_t snapshot_dirty_buffers() {
    size_t saved = 0;
    string err;
    if (buf.dirty && write_recovery_snapshot(buf, err))
      ++saved;
    for (const auto &background : others) {
      err.clear();
      if (background.dirty && write_recovery_snapshot(background, err))
        ++saved;
    }
    return saved;
  }

  static void discard_recovery(const Buffer &buffer) {
    ::unlink(recover_path_for(buffer).c_str());
    ::unlink(legacy_recover_path_for(buffer).c_str());
  }

  void discard_all_recovery() {
    discard_recovery(buf);
    for (const auto &background : others)
      discard_recovery(background);
  }

  bool guard_dirty_backgrounds(const char *command) {
    size_t count = dirty_background_count();
    if (count == 0)
      return false;
    cout << P.warn << command << ": " << count << " background buffer"
         << (count == 1 ? " has" : "s have")
         << " unsaved changes; switch to each buffer and save or use q! to "
            "discard all"
         << C_RESET << "\n";
    return true;
  }

  void note(const string &s) {
    messages.push_back(s);
    if (messages.size() > 80)
      messages.erase(messages.begin());
  }

  void add_recent(const string &p, bool persist = true) {
    if (p.empty())
      return;
    string e = expand_path(p);
    recent_files.erase(std::remove(recent_files.begin(), recent_files.end(), e),
                       recent_files.end());
    recent_files.insert(recent_files.begin(), e);
    if (recent_files.size() > 20)
      recent_files.resize(20);
    if (persist)
      save_config();
  }

  bool is_trusted_plugin(const string &key) const {
    return std::find(trusted_plugins.begin(), trusted_plugins.end(), key) !=
           trusted_plugins.end();
  }

  void trust_plugin_key(const string &key) {
    if (key.empty())
      return;
    if (!is_trusted_plugin(key))
      trusted_plugins.push_back(key);
    save_config();
  }

  bool untrust_plugin_key(const string &key) {
    auto it = std::find(trusted_plugins.begin(), trusted_plugins.end(), key);
    if (it == trusted_plugins.end())
      return false;
    trusted_plugins.erase(it);
    save_config();
    return true;
  }

  string plugin_key_for(const string &raw) const {
    auto it = plugin_files.find(raw);
    if (it != plugin_files.end())
      return it->second;
    return expand_path(raw);
  }

  void show_settings() {
    cout << "settings:\n";
    cout << "  theme="
         << (!active_lua_theme.empty() ? active_lua_theme : theme_name(theme))
         << "\n";
    cout << "  highlight=" << onoff(buf.highlight) << "\n";
    cout << "  number=" << onoff(buf.number) << "\n";
    cout << "  backup=" << onoff(buf.backup) << "\n";
    cout << "  autosave=" << autosave_sec << "\n";
    cout << "  wrap=" << onoff(wrap_long) << "\n";
    cout << "  truncate=" << onoff(truncate_long) << "\n";
    cout << "  status=" << onoff(show_status_line) << "\n";
    cout << "  indentwidth=" << indent_width << "\n";
    cout << "  edmode=" << onoff(ed_mode) << "\n";
    cout << "  lang=" << lang_name() << "\n";
  }

  string lang_name() const {
    switch (lang) {
    case Lang::Cpp:
      return "cpp";
    case Lang::Python:
      return "python";
    case Lang::Shell:
      return "shell";
    case Lang::Ruby:
      return "ruby";
    case Lang::JS:
      return "js";
    case Lang::HTML:
      return "html";
    case Lang::CSS:
      return "css";
    case Lang::JSON:
      return "json";
    case Lang::Go:
      return "go";
    case Lang::Rust:
      return "rust";
    case Lang::Java:
      return "java";
    case Lang::CSharp:
      return "csharp";
    case Lang::PHP:
      return "php";
    case Lang::Markdown:
      return "markdown";
    case Lang::YAML:
      return "yaml";
    case Lang::TOML:
      return "toml";
    case Lang::SQL:
      return "sql";
    case Lang::XML:
      return "xml";
    case Lang::Swift:
      return "swift";
    case Lang::Kotlin:
      return "kotlin";
    case Lang::Lua:
      return "lua";
    case Lang::Perl:
      return "perl";
    case Lang::R:
      return "r";
    case Lang::Make:
      return "make";
    case Lang::Docker:
      return "dockerfile";
    case Lang::Ini:
      return "ini";
    case Lang::Diff:
      return "diff";
    case Lang::Modula2:
      return "modula2";
    case Lang::Modula3:
      return "modula3";
    case Lang::ObjectPascal:
      return "pascal";
    default:
      return "plain";
    }
  }

  void show_config_paths() {
    cout << "config: " << escape_for_display(tedit_config_dir()) << "\n";
    cout << "plugins: " << escape_for_display(tedit_plugins_dir()) << "\n";
    cout << "themes: " << escape_for_display(tedit_themes_dir()) << "\n";
    cout << "recovery: " << escape_for_display(tedit_recovery_dir()) << "\n";
    cout << "rc: " << escape_for_display(cfg_path()) << "\n";
  }

  void show_recent() {
    if (recent_files.empty()) {
      cout << "no recent files\n";
      return;
    }
    for (size_t i = 0; i < recent_files.size(); ++i)
      cout << i + 1 << " " << escape_for_display(recent_files[i]) << "\n";
  }

  void show_messages() {
    if (messages.empty()) {
      cout << "no messages\n";
      return;
    }
    for (const auto &m : messages)
      cout << escape_for_display(m) << "\n";
  }

  void show_history() {
    if (lr.history.empty()) {
      cout << "no command history\n";
      return;
    }
    for (size_t i = 0; i < lr.history.size(); ++i)
      cout << i + 1 << "  " << escape_for_display(lr.history[i]) << "\n";
  }

  void show_aliases() {
    if (aliases.empty()) {
      cout << "no aliases\n";
      return;
    }
    for (const auto &entry : aliases)
      cout << entry.first << " -> " << escape_for_display(entry.second) << "\n";
  }

  void help_topic(const string &topic) {
    string raw = trim_copy(topic);
    string t = lower(raw);
    if (t.empty()) {
      help();
      return;
    }
    if (raw == "N") {
      cout << "N\nRepeats the previous plain find/findi search and jumps to "
              "the previous match.\n";
      return;
    }
    struct HelpEntry {
      const char *names;
      const char *usage;
      const char *text;
    };
    static const HelpEntry entries[] = {
        {"help h ?", "help [command]",
         "Shows the full command list, or detailed help for one command. "
         "Command names and common aliases both work."},
        {"open", "open <path>",
         "Loads a file into the current buffer. Paths support ~ expansion. If "
         "the current buffer has unsaved changes, save or quit first."},
        {"info", "info",
         "Shows current file path, dirty state, line count, character count, "
         "on-disk size, and file mode when available."},
        {"w write", "write [path] | write <range> <path>",
         "Saves the current buffer. With a path, saves there and adopts that "
         "path. With a range and path, writes only selected lines without "
         "changing the current buffer path."},
        {"w! write!", "write! [path]",
         "Force-saves the current buffer without creating a backup file for "
         "that save. Useful when backup files are unwanted for one write."},
        {"wq", "wq",
         "Saves the current buffer to its current path, then exits if the save "
         "succeeds."},
        {"q quit", "quit",
         "Exits the editor. If the current buffer is dirty, prompts to save, "
         "discard, or cancel."},
        {"q! quit!", "quit!",
         "Exits immediately without saving unsaved changes. Hooks still run on "
         "quit."},
        {"saveas", "saveas <path>",
         "Saves the current buffer to a new path and makes that path the "
         "active buffer path."},
        {"p print", "print [range]",
         "Prints the whole buffer or a range. Ranges use forms like 3, 3-8, "
         "3-, -8, or $."},
        {"r", "r <n>", "Prints exactly one line by number."},
        {"a append", "append",
         "Starts append mode. Enter lines to append to the end of the buffer; "
         "a single dot ends input."},
        {"i insert", "insert <n>",
         "Starts insert mode before line n. Enter lines to insert; a single "
         "dot ends input."},
        {"edit", "edit <n> [text...]",
         "Replaces line n. If text is omitted, prompts interactively for the "
         "replacement line."},
        {"d delete", "delete [range]",
         "Deletes a range of lines and records the change for undo."},
        {"m move", "move <from> <to>",
         "Moves one line to a new zero-based insertion position and records "
         "the change for undo."},
        {"join", "join <range>",
         "Joins all lines in a range into one line separated by spaces."},
        {"find", "find <text>",
         "Searches for literal text, case-sensitive, and prints every matching "
         "line."},
        {"findi", "findi <text>",
         "Searches for literal text, case-insensitive, and prints every "
         "matching line."},
        {"findre", "findre [-i] <regex>",
         "Searches with a regular expression. Use -i for case-insensitive "
         "regex matching."},
        {"findrei", "findrei <regex>",
         "Shortcut for case-insensitive regular expression search."},
        {"n", "n",
         "Repeats the previous plain find/findi search and jumps to the next "
         "match."},
        {"N", "N",
         "Repeats the previous plain find/findi search and jumps to the "
         "previous match."},
        {"goto", "goto <n>",
         "Prints line n so you can quickly jump to a location in the file."},
        {"repl", "repl <old> <new>",
         "Replaces the first occurrence of old with new on each line."},
        {"replg", "replg <old> <new>",
         "Replaces every occurrence of old with new on each line."},
        {"read", "read <path> [n]",
         "Reads another file and inserts it after line n. If n is omitted, "
         "inserts at the end. Paths support ~ expansion."},
        {"indent", "indent [range]",
         "Reindents a range using the active language and configured indent "
         "width."},
        {"filter", "filter <range> !shell",
         "Runs a shell command with the selected range on stdin and replaces "
         "that range with command output."},
        {"undo u", "undo [count]",
         "Reverts the most recent edit, or count edits. Undo stores line "
         "snapshots."},
        {"redo", "redo", "Reapplies one change that was undone."},
        {"set", "set [name value]",
         "Without arguments, lists settings. Supports number, backup, "
         "autosave, wrap, truncate, and lang."},
        {"number", "number", "Toggles line numbers and saves the setting."},
        {"highlight", "highlight on|off",
         "Turns syntax highlighting on or off for the active buffer and saves "
         "the setting."},
        {"syntax", "syntax <name>",
         "Alias for set lang <name>. Names include cpp, python, js, rust, lua, "
         "sql, make, dockerfile, and plain."},
        {"theme", "theme <name> | theme preview",
         "Applies a built-in or Lua theme. theme preview prints samples for "
         "built-in themes."},
        {"alias", "alias <from> <to...>",
         "Creates a command alias stored in config. The alias replaces the "
         "first command word before dispatch."},
        {"new", "new [path]",
         "Pushes the current buffer into the buffer list and opens a new empty "
         "or file-backed buffer."},
        {"bnext", "bnext", "Cycles to the next buffer."},
        {"bprev", "bprev", "Cycles to the previous buffer."},
        {"lsb", "lsb",
         "Lists the active buffer as 0 and background buffers as 1..n. Dirty "
         "buffers are marked with an asterisk."},
        {"buffer buffers", "buffer <n>",
         "Switches directly to buffer n from lsb output. Buffer 0 is the "
         "current buffer."},
        {"close", "close",
         "Closes the current buffer if it is clean. If other buffers exist, "
         "switches to one of them; otherwise starts a new unnamed buffer."},
        {"config", "config",
         "Prints the config, plugin, theme, recovery, and rc file paths used "
         "by tedit."},
        {"recent", "recent",
         "Lists recent files opened or saved in this session and persisted in "
         "config."},
        {"messages", "messages",
         "Shows recent editor events such as opens and saves."},
        {"history", "history",
         "Shows command history for the current editor session."},
        {"aliases", "aliases", "Lists configured command aliases."},
        {"edmode", "edmode on|off",
         "Enables or disables classic ed-compatible command and display "
         "behavior."},
        {"lsp", "lsp start <server...> | lsp format | lsp status | lsp stop",
         "Runs a language server over stdio and applies document formatting "
         "edits with undo support."},
        {"diff", "diff",
         "Shows a unified diff between the current buffer and the on-disk "
         "file."},
        {"ls", "ls [-l] [-a] [path]",
         "Lists files from inside the editor. Use -a for hidden files and -l "
         "for size and permissions."},
        {"pwd", "pwd", "Prints the editor process current working directory."},
        {"cd", "cd <dir>",
         "Changes the editor process working directory. Supports ., .., and ~ "
         "paths."},
        {"clear", "clear",
         "Clears the terminal screen and scrollback using ANSI escape "
         "sequences."},
        {"version ver", "version",
         "Prints the tedit version compiled into this binary."},
        {"lua", "lua <code>",
         "Runs inline Lua code in tedit's Lua state. Lua code is trusted code "
         "and runs with your user permissions."},
        {"luafile", "luafile <path>",
         "Runs a Lua script file. Paths support ~ expansion. Script code is "
         "trusted code."},
        {"run-plugin", ":run-plugin <name|path>",
         "Runs a Lua plugin by configured plugin name or file path. The "
         "leading colon is required for safety."},
        {"plugin", "plugin trust|untrust|trusted [name|path]",
         "Manages trusted plugin warning sources. Trusting suppresses "
         "heuristic warnings for that plugin path or name."},
        {"plugins", "plugins",
         "Lists Lua plugin files found under ~/tedit-config/plugins and marks "
         "the current plugin when applicable."},
        {"reload-plugins", "reload-plugins",
         "Rescans ~/tedit-config/plugins for Lua plugin files without "
         "restarting tedit."},
        {"lua-themes", "lua-themes",
         "Lists Lua theme files found under ~/tedit-config/themes and marks "
         "the active Lua theme."}};
    for (const auto &e : entries) {
      std::istringstream names(e.names);
      string name;
      while (names >> name) {
        if (t == lower(name)) {
          cout << e.usage << "\n" << e.text << "\n";
          return;
        }
      }
    }
    cout << "no focused help for " << topic << "; type help for all commands\n";
  }

  void init_lua() {
    if (L)
      return;
    L = luaL_newstate();
    if (!L) {
      cout << C_RED << "lua: failed to initialize state" << C_RESET << "\n";
      return;
    }
    luaL_openlibs(L);
    register_tedit_lua_api(L);
    load_lua_plugins();
  }

  void close_lua() {
    if (L) {
      lua_close(L);
      L = nullptr;
    }
  }

  void load_lua_plugins() {
    plugin_names.clear();
    plugin_files.clear();
    if (!L)
      return;
    string dir = tedit_plugins_dir();
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec))
      return;
    fs::directory_iterator it(
        dir, fs::directory_options::skip_permission_denied, ec),
        end;
    for (; !ec && it != end; it.increment(ec)) {
      const auto &entry = *it;
      std::error_code type_ec;
      if (!entry.is_regular_file(type_ec) || type_ec)
        continue;
      fs::path p = entry.path();
      if (p.extension() != ".lua")
        continue;
      string fpath = p.string();
      string name = p.stem().string();
      plugin_names.push_back(name);
      plugin_files[name] = fpath;
    }
  }

  static bool scan_plugin_text(const string &content, vector<string> &hits) {
    static const char *patterns[] = {"os.execute", "io.popen",        "dofile",
                                     "loadfile",   "package.loadlib", "debug.",
                                     "os.remove",  "os.rename"};
    hits.clear();
    for (const char *pat : patterns) {
      if (content.find(pat) != string::npos) {
        hits.emplace_back(pat);
      }
    }
    return !hits.empty();
  }

  bool run_plugin_file(const string &display_name, const string &path) {
    if (!L) {
      cout << P.err << "run-plugin: lua not available" << C_RESET << "\n";
      return false;
    }

    std::ifstream in(path);
    if (!in.good()) {
      cout << P.err << "run-plugin: cannot open " << path << C_RESET << "\n";
      return false;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    string content = buf.str();

    vector<string> hits;
    bool trusted = is_trusted_plugin(display_name) || is_trusted_plugin(path);
    if (scan_plugin_text(content, hits) && !trusted) {
      cout << P.warn << "This plugin looks potentially malicious (uses: ";
      for (size_t i = 0; i < hits.size(); ++i) {
        if (i)
          cout << ", ";
        cout << hits[i];
      }
      cout << ")." << C_RESET << "\n";
      cout << P.warn
           << "This plugin is potentially malicious. Do you really want to run "
              "this? [y/N] "
           << C_RESET << std::flush;
      char c = 0;
      std::cin.get(c);
      string dump;
      std::getline(std::cin, dump);
      if (c != 'y' && c != 'Y') {
        cout << P.dim << "run-plugin: aborted by user" << C_RESET << "\n";
        return false;
      }
    }

    int rc = luaL_loadfile(L, path.c_str());
    if (rc != LUA_OK) {
      const char *msg = lua_tostring(L, -1);
      cout << P.err << "run-plugin: load error: " << (msg ? msg : "") << C_RESET
           << "\n";
      lua_pop(L, 1);
      return false;
    }
    rc = lua_pcall(L, 0, 0, 0);
    if (rc != LUA_OK) {
      const char *msg = lua_tostring(L, -1);
      cout << P.err << "run-plugin: runtime error: " << (msg ? msg : "")
           << C_RESET << "\n";
      lua_pop(L, 1);
      return false;
    }

    current_plugin = display_name;
    cout << P.ok << "run-plugin: loaded " << display_name << C_RESET << "\n";
    return true;
  }

  void list_lua_themes() {
    string dir = tedit_themes_dir();
    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
      cout << "no lua themes directory (" << dir << ")\n";
      return;
    }
    vector<string> names;
    fs::directory_iterator it(
        dir, fs::directory_options::skip_permission_denied, ec),
        end;
    for (; !ec && it != end; it.increment(ec)) {
      const auto &e = *it;
      std::error_code type_ec;
      if (!e.is_regular_file(type_ec) || type_ec)
        continue;
      fs::path p = e.path();
      if (p.extension() == ".lua")
        names.push_back(p.stem().string());
    }
    std::sort(names.begin(), names.end());
    if (names.empty()) {
      cout << "no lua themes found\n";
      return;
    }
    cout << "lua themes:\n";
    for (const auto &n : names) {
      bool is_current =
          !active_lua_theme.empty() && lower(n) == lower(active_lua_theme);
      cout << "- " << n << (is_current ? " *" : "") << "\n";
    }
  }

  void save_config() {
    std::ostringstream out;
    if (!active_lua_theme.empty()) {
      out << "theme=" << active_lua_theme << "\n";
    } else {
      out << "theme=" << theme_name(theme) << "\n";
    }
    out << "highlight=" << (buf.highlight ? "on" : "off") << "\n";
    out << "number=" << (buf.number ? "on" : "off") << "\n";
    out << "backup=" << (buf.backup ? "on" : "off") << "\n";
    out << "autosave=" << (autosave_sec) << "\n";
    out << "wrap=" << (wrap_long ? "on" : "off") << "\n";
    out << "truncate=" << (truncate_long ? "on" : "off") << "\n";
    out << "status=" << (show_status_line ? "on" : "off") << "\n";
    out << "indentwidth=" << indent_width << "\n";
    out << "edmode=" << (ed_mode ? "on" : "off") << "\n";
    for (auto &kv : aliases)
      out << "alias\t" << esc(kv.first) << "\t" << esc(kv.second) << "\n";
    for (auto &p : recent_files)
      out << "recent\t" << esc(p) << "\n";
    for (auto &p : trusted_plugins)
      out << "trust\t" << esc(p) << "\n";

    string path = cfg_path();
    string tmp = path + ".tmp.XXXXXX";
    vector<char> tbuf(tmp.begin(), tmp.end());
    tbuf.push_back('\0');
    int fd = ::mkstemp(tbuf.data());
    if (fd < 0)
      return;
    (void)::fchmod(fd, 0600);
    string content = out.str();
    string err;
    bool ok =
        write_all(fd, content.data(), content.size(), err, "config write");
    if (ok && ::fsync(fd) != 0)
      ok = false;
    if (::close(fd) != 0)
      ok = false;
    if (!ok || ::rename(tbuf.data(), path.c_str()) != 0) {
      ::unlink(tbuf.data());
      return;
    }
    (void)fsync_dir_of(path);
  }
  void load_config() {
    std::ifstream in(cfg_path());
    if (!in.good()) {
      in.clear();
      in.open(legacy_cfg_path());
    }
    if (!in.good())
      return;
    string line;
    while (std::getline(in, line)) {
      if (line.empty())
        continue;
      if (line.rfind("alias\t", 0) == 0) {
        size_t separator = line.find('\t', 6);
        if (separator == string::npos)
          continue;
        string from = unesc(line.substr(6, separator - 6));
        string to = unesc(line.substr(separator + 1));
        if (!from.empty() && !to.empty())
          aliases[from] = to;
        continue;
      }
      if (line.rfind("alias ", 0) == 0) {
        vector<string> args;
        string parse_error;
        if (split_quoted_words(line.substr(6), args, parse_error) &&
            args.size() >= 2) {
          aliases[args[0]] = join_words(args, 1);
        }
        continue;
      }
      if (line.rfind("recent\t", 0) == 0) {
        string p = unesc(line.substr(7));
        if (!p.empty() && std::find(recent_files.begin(), recent_files.end(),
                                    p) == recent_files.end())
          recent_files.push_back(p);
        continue;
      }
      if (line.rfind("trust\t", 0) == 0) {
        string key = unesc(line.substr(6));
        if (!is_trusted_plugin(key))
          trusted_plugins.push_back(key);
        continue;
      }
      auto eq = line.find('=');
      if (eq == string::npos)
        continue;
      string key = lower(trim_copy(line.substr(0, eq)));
      string val = trim_copy(line.substr(eq + 1));
      if (key == "theme") {
        Theme t;
        if (theme_from_name(val, t)) {
          theme = t;
          active_lua_theme.clear();
          P = palette_for(theme);
          lr.set_theme_colors(P);
        } else {

          ThemePalette luaP;
          if (load_theme_from_lua_file(val, luaP)) {
            P = luaP;
            active_lua_theme = val;
            lr.set_theme_colors(P);
          }
        }
      } else if (key == "highlight") {
        bool b;
        if (parse_bool_string(val, b))
          buf.highlight = b;
      } else if (key == "number") {
        bool b;
        if (parse_bool_string(val, b))
          buf.number = b;
      } else if (key == "backup") {
        bool b;
        if (parse_bool_string(val, b))
          buf.backup = b;
      } else if (key == "autosave") {
        long s;
        if (parse_long(val, s))
          autosave_sec = (int)std::min<long>(INT_MAX, std::max<long>(0, s));
      } else if (key == "wrap") {
        bool b;
        if (parse_bool_string(val, b))
          wrap_long = b;
      } else if (key == "truncate") {
        bool b;
        if (parse_bool_string(val, b))
          truncate_long = b;
      } else if (key == "status") {
        bool b;
        if (parse_bool_string(val, b))
          show_status_line = b;
      } else if (key == "indentwidth") {
        long n;
        if (parse_long(val, n) && n >= 1 && n <= 16)
          indent_width = (int)n;
      } else if (key == "edmode") {
        bool b;
        if (parse_bool_string(val, b))
          ed_mode = b;
      }
    }
  }

  void confetti() {
    if (!use_color())
      return;
    const char *art[] = {" *  .  *   . *", ".  *  *  .    *",
                         "   *  .   *  . "};
    cout << P.accent << art[0] << C_RESET << "\n"
         << P.ok << art[1] << C_RESET << "\n"
         << P.warn << art[2] << C_RESET << "\n";
  }
  void tip() {
    static const char *tips[] = {
        "Tip: use 'goto <n>' to jump to a line.",
        "Tip: 'n' and 'N' hop through last search results.",
        "Tip: ':filter 1,10 !sed -n \"p\"' pipes lines through a shell.",
        "Tip: use 'syntax <name>' when a file extension is ambiguous.",
        "Tip: 'alias dd \"delete 1-$\"' to delete all quickly.",
        "Tip: 'diff' shows changes vs on-disk.",
        ("Tip: 'Tab': first word = commands only; after 'cd ' => directories "
         "only.")};
    std::mt19937_64 rng((unsigned)time(nullptr));
    cout << P.dim << tips[rng() % (sizeof(tips) / sizeof(*tips))] << C_RESET
         << "\n";
  }
  void banner() {
    string bpath = home_path() + "/.tedit_banner";
    if (!file_exists(bpath))
      return;
    std::ifstream in(bpath);
    if (!in.good())
      return;
    cout << P.accent;
    string L;
    while (std::getline(in, L))
      cout << L << "\n";
    cout << C_RESET;
  }

  string prompt_str() const {
    if (ed_mode)
      return string();
    string dirty = buf.dirty ? "*" : "";
    return (use_color() ? P.prompt : string("")) + dirty + "tedit> " +
           (use_color() ? C_RESET : string(""));
  }

  void status() {
    using std::chrono::system_clock;
    auto t = system_clock::to_time_t(system_clock::now());
    char tb[32];
    strftime(tb, sizeof(tb), "%H:%M:%S", localtime(&t));
    string tname =
        active_lua_theme.empty() ? theme_name(theme) : active_lua_theme;
    cout << P.dim << "[" << current_buffer_index() << "/"
         << (buffer_count() - 1) << " "
         << (buf.path.empty() ? "(unnamed)" : escape_for_display(buf.path))
         << "] "
         << "lines=" << buf.lines.size() << " chars=" << char_count(buf)
         << (buf.dirty ? " *" : "") << " | " << tb << " | theme:" << tname
         << " | hl:" << (buf.highlight ? "on" : "off")
         << " | wrap:" << (wrap_long ? "on" : "off")
         << " | plugin:" << (current_plugin.empty() ? "none" : current_plugin)
         << C_RESET << "\n";
  }

  void help() {
    auto CMD = [&](const string &cmd, const string &args, const string &desc) {
      std::ostringstream left;
      string label = cmd;
      if (!args.empty())
        label += " " + args;
      left << std::left << std::setw(40) << label;
      string reset = use_color() ? C_RESET : string();
      cout << P.help_cmd << left.str() << reset << " - " << P.help_text << desc
           << reset << "\n";
    };

    cout << P.title << "Commands (':' optional, except where noted)" << C_RESET
         << "\n";
    CMD("open <path>", "", "open file");
    CMD("info", "", "buffer + file info");
    CMD("w|write [path]", "", "save (atomic), optional new path");
    CMD("write [range] <path>", "", "write selected lines to path");
    CMD("w!|write! <path>", "", "force save without backup");
    CMD("wq", "", "save & quit");
    CMD("q!|quit!", "", "quit without saving");
    CMD("saveas <path>", "", "save to path");
    CMD("q|quit", "", "quit (prompts if unsaved)");
    CMD("p|print [range]", "", "print lines");
    CMD("r <n>", "", "show one line");
    CMD("a|append", "", "append lines ('.' ends; use \".\" for a literal)");
    CMD("i|insert <n>", "", "insert before line n");
    CMD("edit <n> [text...]", "", "replace line n (prompts if no text)");
    CMD("d|delete [range]", "", "delete lines");
    CMD("m|move <from> <to>", "", "move line");
    CMD("join <range>", "", "join lines with space");
    CMD("/text | find | findi | findre", "", "search (regex via findre)");
    CMD("n | N", "", "next/prev match from last search");
    CMD("goto <n>", "", "jump to line");
    CMD("repl old new | replg old new", "", "replace first/global per line");
    CMD("read <path> [n]", "", "insert file after n (default=end)");
    CMD("indent [range]", "", "reindent using the active language");
    CMD("filter <range> !shell", "",
        "pipe range through shell and replace (safe temp names)");
    CMD("undo | u [k]", "", "undo (optionally k steps)");
    CMD("redo", "", "redo");
    CMD("set", "", "show current settings");
    CMD("set number on|off", "", "toggle line numbers");
    CMD("set backup on|off", "", "toggle on-save ~ backup");
    CMD("set autosave <sec>", "", "autosave interval");
    CMD("set wrap on|off", "", "soft-wrap long lines under the gutter");
    CMD("set truncate on|off", "", "truncate line display when wrap=off");
    CMD("set status on|off", "", "show or hide the per-command status line");
    CMD("set indentwidth <1-16>", "", "set spaces per indentation level");
    CMD("set edmode on|off", "", "toggle classic ed-compatible behavior");
    CMD("set lang <name>", "", "override syntax (auto by extension)");
    CMD("highlight on|off", "", "semantic syntax highlighting");
    CMD("syntax <name>", "", "alias for set lang <name>");
    CMD("theme <name>", "", "built-in theme name or Lua theme");
    CMD("theme preview", "", "show built-in theme samples");
    CMD("alias <from> <to...>", "", "define command alias");
    CMD("new [path]", "", "open new buffer (push current)");
    CMD("bnext | bprev | lsb", "", "cycle/list buffers");
    CMD("buffer <n> | close", "", "switch to or close a buffer");
    CMD("config | recent | messages", "",
        "show paths, recent files, or message log");
    CMD("history | aliases", "", "show command history or configured aliases");
    CMD("diff", "", "show diff vs on-disk (safe)");
    CMD("ls [-l] [-a] [path] | pwd", "", "filesystem helpers");
    CMD("cd <dir>", "", "change directory (./ ../ ~/)");
    CMD("clear", "", "clear screen and scrollback");
    CMD("version", "", "show tedit version");
    CMD("lsp start|format|status|stop", "",
        "language-server formatting over stdio");
    CMD("lua <code>", "", "run Lua code");
    CMD("luafile <path>", "", "run Lua script file");
    CMD("run-plugin <name>", "", "run Lua plugin (requires :run-plugin)");
    CMD("plugin trust <name|path>", "", "trust a plugin warning source");
    CMD("plugins", "", "list available Lua plugins");
    CMD("reload-plugins", "", "rescan Lua plugins from tedit-config/plugins");

    CMD("lua-themes", "", "list available Lua themes");
    cout << P.dim
         << "Tab: first word => commands only; after 'cd ' => directories only."
         << C_RESET << "\n";
  }

  bool load(const string &p, bool persist_recent = true) {
    string path = expand_path(p);
    Buffer next;
    next.path = path;
    next.number = buf.number;
    next.backup = buf.backup;
    next.highlight = buf.highlight;
    string err;
    if (!load_file(path, next, err)) {
      cout << P.err << "open: " << escape_for_display(path) << ": " << err
           << C_RESET << "\n";
      return false;
    }
    buf = std::move(next);
    lang = detect_lang(path);
    add_recent(path, persist_recent);
    note("opened " + path);
    cout << P.ok << "opened " << escape_for_display(path) << C_RESET << "\n";
    (void)maybe_recover(buf);
    return true;
  }

  bool run_hook(const char *name) {
    string h = home_path() + "/.tedit/hooks/";
    h += name;
    if (!file_exists(h))
      return true;
    vector<string> args;
    if (!buf.path.empty())
      args.push_back(buf.path);
    int rc = run_exec_file(h, args);
    return rc == 0;
  }

  bool save(const string &maybe) {
    string target = maybe.empty() ? buf.path : expand_path(maybe);
    if (target.empty()) {
      cout << P.warn << "save: no filename (use: write <path>)" << C_RESET
           << "\n";
      return false;
    }
    string old_recovery = recover_path_for(buf);
    string old_legacy_recovery = legacy_recover_path_for(buf);
    string err;
    bool ok = atomic_save(target, buf, buf.backup, err);
    if (!ok) {
      cout << P.err << "save: " << err << C_RESET << "\n";
      return false;
    }
    if (target != buf.path)
      buf.path = target;
    buf.dirty = false;
    buf.undo.mark_all_dirty();
    buf.redo.mark_all_dirty();
    add_recent(target);
    note("saved " + target);
    cout << P.ok << "saved to " << escape_for_display(target) << C_RESET
         << "\n";
    confetti();
    unlink(old_recovery.c_str());
    unlink(old_legacy_recovery.c_str());
    string rec = recover_path_for(buf);
    unlink(rec.c_str());
    string oldrec = legacy_recover_path_for(buf);
    unlink(oldrec.c_str());
    (void)run_hook("on_save");
    return true;
  }

  void push_undo() {
    buf.undo.push(buf.lines, buf.dirty, buf.final_newline, buf.current_line);
    buf.redo.clear();
    ed_quit_warned = false;
  }

  void append_mode() {
    cout << "enter text; '.' alone ends (use \".\" for a literal '.')\n";
    vector<string> added;
    string s;
    while (true) {
      cout << "> " << std::flush;
      if (!std::getline(std::cin, s)) {
        cout << "\n";
        break;
      }
      if (s == "\".\"")
        s = ".";
      else if (s == ".")
        break;
      added.push_back(std::move(s));
    }
    if (!added.empty()) {
      push_undo();
      size_t count = added.size();
      buf.lines.insert(buf.lines.end(), std::make_move_iterator(added.begin()),
                       std::make_move_iterator(added.end()));
      buf.final_newline = true;
      buf.current_line = buf.lines.size();
      buf.dirty = true;
      cout << "appended " << count << " line(s)\n";
    }
  }

  void insert_mode(size_t before) {
    cout << "enter text; '.' alone ends (use \".\" for a literal '.')\n";
    vector<string> added;
    string s;
    while (true) {
      cout << "> " << std::flush;
      if (!std::getline(std::cin, s)) {
        cout << "\n";
        break;
      }
      if (s == "\".\"")
        s = ".";
      else if (s == ".")
        break;
      added.push_back(std::move(s));
    }
    if (!added.empty()) {
      push_undo();
      size_t count = added.size();
      size_t position = std::min(before, buf.lines.size());
      bool appended = position == buf.lines.size();
      buf.lines.insert(buf.lines.begin() + (long)position,
                       std::make_move_iterator(added.begin()),
                       std::make_move_iterator(added.end()));
      if (appended)
        buf.final_newline = true;
      buf.current_line = position + count;
      buf.dirty = true;
      cout << "inserted " << count << " line(s)\n";
    }
  }

  int gutter_width() const {
    if (!buf.number)
      return 0;
    int w = digits_for(buf.lines.size() == 0 ? 1 : buf.lines.size());
    return w + 3;
  }

  void print_line(size_t i, HighlightState &highlight_state) {
    buf.current_line = i;
    const int termw = term_width();
    const int gw = gutter_width();
    const int avail = std::max(10, termw - gw);

    std::ostringstream first, cont;
    if (buf.number) {
      first << P.gutter << std::setw(gw - 3) << i << " | " << C_RESET;
      cont << P.gutter << std::string(gw - 3, ' ') << " | " << C_RESET;
    }

    string colored = colorize_lang(escape_for_display(buf.lines[i - 1]), buf, P,
                                   lang, highlight_state);

    if (wrap_long) {
      print_wrapped_with_gutter(colored, first.str(), cont.str(), avail);
    } else {
      if (truncate_long) {
        int col = 0;
        bool esc = false;
        cout << first.str();
        for (size_t k = 0; k < colored.size(); ++k) {
          char ch = colored[k];
          if (!esc) {
            if (ch == '\033') {
              esc = true;
              cout << ch;
              continue;
            }
            if (col >= avail - 3) {
              cout << "...";
              break;
            }
            cout << ch;
            col++;
          } else {
            cout << ch;
            if (ch == 'm')
              esc = false;
          }
        }
        cout << C_RESET << "\n";
      } else {
        cout << first.str() << colored << C_RESET << "\n";
      }
    }
  }

  void print(size_t lo, size_t hi) {
    if (hi == 0) {
      cout << "(empty)\n";
      return;
    }
    HighlightState highlight_state;
    if (buf.highlight && use_color()) {
      for (size_t i = 1; i < lo; i++)
        advance_highlight_state(escape_for_display(buf.lines[i - 1]), P, lang,
                                highlight_state);
    }
    for (size_t i = lo; i <= hi; i++)
      print_line(i, highlight_state);
  }

  void repl(bool global, const string &old, const string &nw) {
    if (old.empty()) {
      cout << P.warn << "usage: repl[g] <old> <new>" << C_RESET << "\n";
      return;
    }
    vector<string> replaced = buf.lines;
    size_t total = 0;
    for (string &line : replaced) {
      string out;
      size_t c = global ? replace_all_line(line, old, nw, out)
                        : replace_first_line(line, old, nw, out);
      if (c) {
        line.swap(out);
        total += c;
      }
    }
    if (total) {
      push_undo();
      buf.lines.swap(replaced);
      buf.dirty = true;
      cout << "replaced " << total << " occurrence" << (total == 1 ? "" : "s")
           << (global ? " (global)" : " (first per line)") << "\n";
    } else {
      cout << "no occurrences\n";
    }
  }

  void info() {
    struct stat st{};
    bool have = (!buf.path.empty() && ::stat(buf.path.c_str(), &st) == 0);
    cout << "file: "
         << (buf.path.empty() ? "(unnamed)" : escape_for_display(buf.path))
         << (buf.dirty ? " *" : "") << "\n";
    cout << "  lines: " << buf.lines.size() << ", chars: " << char_count(buf)
         << "\n";
    if (have) {
      cout << "  size: " << (long long)st.st_size
           << " bytes, mode: " << std::oct << (st.st_mode & 0777) << std::dec
           << "\n";
    } else
      cout << "  on-disk: (none)\n";
  }

  void next_match(bool reverse) {
    if (last_search.empty()) {
      cout << "(no previous search)\n";
      return;
    }
    vector<size_t> hits;
    search_plain_allhits(buf, last_search, last_icase, hits);
    if (hits.empty()) {
      cout << "no matches\n";
      return;
    }
    if (!reverse) {
      auto it = std::upper_bound(hits.begin(), hits.end(), last_index);
      if (it == hits.end())
        it = hits.begin();
      last_index = *it;
    } else {
      auto it = std::lower_bound(hits.begin(), hits.end(), last_index);
      if (it == hits.begin())
        it = hits.end();
      --it;
      last_index = *it;
    }
    print(last_index, last_index);
  }

  void cycle_theme(const string &name) {
    Theme t;
    if (theme_from_name(name, t)) {
      theme = t;
      active_lua_theme.clear();
      P = palette_for(theme);
      lr.set_theme_colors(P);
      cout << P.ok << "theme set" << C_RESET << "\n";
      save_config();
      return;
    }

    ThemePalette luaP;
    if (load_theme_from_lua_file(name, luaP)) {
      P = luaP;
      active_lua_theme = name;
      lr.set_theme_colors(P);
      cout << P.ok << "theme set (lua: " << name << ")" << C_RESET << "\n";
      save_config();
      return;
    }

    cout << P.err << "Theme not found" << C_RESET << "\n";
  }

  bool open_new_buffer(const string &path) {
    Buffer nb;
    nb.number = buf.number;
    nb.backup = buf.backup;
    nb.highlight = buf.highlight;
    if (!path.empty()) {
      nb.path = path;
      string err;
      if (!load_file(path, nb, err)) {
        cout << P.err << "new: " << escape_for_display(path) << ": " << err
             << C_RESET << "\n";
        return false;
      }
      maybe_recover(nb);
    }
    others.push_back(std::move(buf));
    buf = std::move(nb);
    lang = detect_lang(buf.path);
    add_recent(buf.path);
    cout << P.ok << "(new buffer) "
         << (path.empty() ? "(unnamed)" : escape_for_display(path)) << C_RESET
         << "\n";
    return true;
  }
  bool add_background_buffer(const string &path, bool persist_recent = true) {
    string p = expand_path(path);
    Buffer nb;
    nb.number = buf.number;
    nb.backup = buf.backup;
    nb.highlight = buf.highlight;
    nb.path = p;
    string err;
    if (!load_file(p, nb, err)) {
      cout << P.err << "open: " << escape_for_display(p) << ": " << err
           << C_RESET << "\n";
      return false;
    }
    maybe_recover(nb);
    others.push_back(std::move(nb));
    add_recent(p, persist_recent);
    return true;
  }
  void list_buffers() {
    cout << C_BOLD << "* 0 "
         << (buf.path.empty() ? "(unnamed)" : escape_for_display(buf.path))
         << (buf.dirty ? " *" : "") << C_RESET << "\n";
    for (size_t i = 0; i < others.size(); ++i) {
      const auto &b = others[i];
      cout << "  " << i + 1 << " "
           << (b.path.empty() ? "(unnamed)" : escape_for_display(b.path))
           << (b.dirty ? " *" : "") << "\n";
    }
  }
  void bnext() {
    if (others.empty()) {
      cout << "(only one buffer)\n";
      return;
    }
    Buffer current = std::move(buf);
    buf = std::move(others.back());
    others.pop_back();
    others.insert(others.begin(), std::move(current));
    lang = detect_lang(buf.path);
    cout << "[bnext] "
         << (buf.path.empty() ? "(unnamed)" : escape_for_display(buf.path))
         << "\n";
  }
  void bprev() {
    if (others.empty()) {
      cout << "(only one buffer)\n";
      return;
    }
    Buffer prev = std::move(others.front());
    others.erase(others.begin());
    others.push_back(std::move(buf));
    buf = std::move(prev);
    lang = detect_lang(buf.path);
    cout << "[bprev] "
         << (buf.path.empty() ? "(unnamed)" : escape_for_display(buf.path))
         << "\n";
  }
  void switch_buffer(size_t idx) {
    if (idx == 0) {
      cout << "[buffer] "
           << (buf.path.empty() ? "(unnamed)" : escape_for_display(buf.path))
           << "\n";
      return;
    }
    if (idx > others.size()) {
      cout << P.warn << "buffer: no such buffer" << C_RESET << "\n";
      return;
    }
    std::swap(buf, others[idx - 1]);
    lang = detect_lang(buf.path);
    cout << "[buffer] "
         << (buf.path.empty() ? "(unnamed)" : escape_for_display(buf.path))
         << "\n";
  }
  bool close_buffer() {
    if (buf.dirty) {
      cout << P.warn
           << "close: unsaved changes (use q! to discard or save first)"
           << C_RESET << "\n";
      return true;
    }
    if (others.empty()) {
      buf = Buffer{};
      lang = Lang::Plain;
      cout << "[close] new unnamed buffer\n";
      return true;
    }
    buf = others.back();
    others.pop_back();
    lang = detect_lang(buf.path);
    cout << "[close] "
         << (buf.path.empty() ? "(unnamed)" : escape_for_display(buf.path))
         << "\n";
    return true;
  }
  void theme_preview() {
    Theme themes[] = {Theme::Default, Theme::Dark,  Theme::Neon,
                      Theme::Matrix,  Theme::Paper, Theme::Yellow,
                      Theme::Iceberg, Theme::Ocean, Theme::Solarized,
                      Theme::Gruvbox, Theme::Rose};
    for (Theme t : themes) {
      ThemePalette q = palette_for(t);
      cout << q.title << theme_name(t) << C_RESET << " " << q.accent << "accent"
           << C_RESET << " " << q.ok << "ok" << C_RESET << " " << q.warn
           << "warn" << C_RESET << " " << q.err << "err" << C_RESET << "\n";
    }
  }
  void show_diff() {
    if (buf.path.empty() || !file_exists(buf.path)) {
      cout << "diff: no on-disk version\n";
      return;
    }
    char tpat[] = "/tmp/tedit_diff_XXXXXX";
    int tfd = mkstemp(tpat);
    if (tfd < 0) {
      cout << "diff: mkstemp failed\n";
      return;
    }
    FILE *tf = fdopen(tfd, "w");
    if (!tf) {
      close(tfd);
      unlink(tpat);
      cout << "diff: fdopen failed\n";
      return;
    }
    string err;
    if (!atomic_save_to_fd(tf, buf, err)) {
      unlink(tpat);
      cout << "diff: " << err << "\n";
      return;
    }

    int rc = run_exec_program("diff", {"-u", "--", buf.path, tpat});
    if (rc != 0 && rc != 1)
      cout << "diff: command failed (exit " << rc << ")\n";
    unlink(tpat);
  }

  void clear_screen() { cout << "\033[3J\033[H\033[2J" << std::flush; }

  static string expand_path(const string &in) {
    if (in.empty())
      return in;
    if (in == "~")
      return home_path();
    if (in.size() >= 2 && in[0] == '~' && in[1] == '/')
      return home_path() + in.substr(1);
    return in;
  }

  bool parse_ed_address(const string &token, size_t default_value,
                        size_t &value) const {
    if (token.empty() || token == ".") {
      value = default_value;
      return true;
    }
    if (token == "$") {
      value = buf.lines.size();
      return true;
    }
    long number = 0;
    if (!parse_long(token, number) || number < 0)
      return false;
    value = (size_t)number;
    return true;
  }

  bool handle_ed_syntax(const string &input, bool &keep_running) {
    keep_running = true;
    if (!ed_mode || input.empty())
      return false;
    if (input == "=") {
      cout << buf.current_line << "\n";
      return true;
    }
    if (input == "." || input == "$" ||
        std::all_of(input.begin(), input.end(),
                    [](unsigned char c) { return std::isdigit(c); })) {
      size_t line = 0;
      if (!parse_ed_address(input, buf.current_line, line) || line < 1 ||
          line > buf.lines.size())
        cout << "?\n";
      else
        print(line, line);
      return true;
    }
    if (input == "a" || input == "i") {
      size_t before = input == "a"
                          ? buf.current_line
                          : (buf.current_line ? buf.current_line - 1 : 0);
      size_t old_size = buf.lines.size();
      insert_mode(before);
      size_t added = buf.lines.size() - old_size;
      if (added)
        buf.current_line = before + added;
      return true;
    }
    size_t substitution = input.find("s/");
    if (substitution != string::npos) {
      if (substitution > 0) {
        size_t addressed = 0;
        if (!parse_ed_address(input.substr(0, substitution), buf.current_line,
                              addressed) ||
            addressed < 1 || addressed > buf.lines.size()) {
          cout << "?\n";
          return true;
        }
        buf.current_line = addressed;
      }
      size_t second = input.find('/', substitution + 2);
      if (second == string::npos) {
        cout << "?\n";
        return true;
      }
      size_t third = input.find('/', second + 1);
      if (third == string::npos) {
        cout << "?\n";
        return true;
      }
      string old = input.substr(substitution + 2, second - (substitution + 2));
      string replacement = input.substr(second + 1, third - second - 1);
      bool global = input.substr(third + 1) == "g";
      if (buf.current_line < 1 || buf.current_line > buf.lines.size() ||
          old.empty()) {
        cout << "?\n";
        return true;
      }
      string changed;
      size_t count = global
                         ? replace_all_line(buf.lines[buf.current_line - 1],
                                            old, replacement, changed)
                         : replace_first_line(buf.lines[buf.current_line - 1],
                                              old, replacement, changed);
      if (!count) {
        cout << "?\n";
        return true;
      }
      push_undo();
      buf.lines[buf.current_line - 1] = std::move(changed);
      buf.dirty = true;
      return true;
    }
    char operation = input.back();
    if (operation != 'p' && operation != 'n' && operation != 'd')
      return false;
    string address = input.substr(0, input.size() - 1);
    size_t comma = address.find(',');
    size_t lo = buf.current_line, hi = buf.current_line;
    if (address == "%") {
      lo = 1;
      hi = buf.lines.size();
    } else if (comma == string::npos) {
      if (!parse_ed_address(address, buf.current_line, lo)) {
        cout << "?\n";
        return true;
      }
      hi = lo;
    } else {
      if (!parse_ed_address(address.substr(0, comma), 1, lo) ||
          !parse_ed_address(address.substr(comma + 1), buf.lines.size(), hi)) {
        cout << "?\n";
        return true;
      }
    }
    if (lo < 1 || hi < lo || hi > buf.lines.size()) {
      cout << "?\n";
      return true;
    }
    if (operation == 'd') {
      push_undo();
      buf.lines.erase(buf.lines.begin() + (long)lo - 1,
                      buf.lines.begin() + (long)hi);
      buf.current_line = buf.lines.empty() ? 0 : std::min(lo, buf.lines.size());
      buf.dirty = true;
      return true;
    }
    bool old_number = buf.number;
    if (operation == 'p')
      buf.number = false;
    else
      buf.number = true;
    print(lo, hi);
    buf.number = old_number;
    return true;
  }

  bool handle(const string &raw) {
    autosave_if_needed(buf, last_autosave, autosave_sec);

    string in = trim_copy(raw);
    if (in.empty())
      return true;
    bool had_colon = false;
    if (in[0] == ':') {
      had_colon = true;
      in = trim_copy(in.substr(1));
      if (in.empty())
        return true;
    }

    bool ed_keep = true;
    if (handle_ed_syntax(in, ed_keep))
      return ed_keep;

    {
      std::istringstream ss(in);
      string tok;
      ss >> tok;
      auto it = aliases.find(tok);
      if (it != aliases.end()) {
        string rest;
        std::getline(ss, rest);
        in = it->second + rest;
      }
    }

    if (!in.empty() && in[0] == '/') {
      string q = in.substr(1);
      last_search = q;
      last_icase = false;
      last_index = 0;
      search_plain(buf, q, false);
      return true;
    }

    std::istringstream ss(in);
    string cmd;
    ss >> cmd;
    string rest;
    std::getline(ss, rest);
    rest = trim_copy(rest);
    string lc = lower(cmd);

    if (lc == "help" || lc == "h" || lc == "?") {
      help_topic(rest);
      return true;
    }
    if (lc == "open") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() != 1) {
        cout << P.warn << "usage: open <path>" << C_RESET << "\n";
        return true;
      }
      if (buf.dirty) {
        cout << P.warn
             << "Unsaved changes. Save or discard them before opening another "
                "file."
             << C_RESET << "\n";
        return true;
      }
      load(args[0]);
      return true;
    }
    if (lc == "info") {
      info();
      return true;
    }
    if (lc == "wq") {
      if (save("")) {
        if (guard_dirty_backgrounds("wq"))
          return true;
        cout << P.dim << "bye!" << C_RESET << "\n";
        (void)run_hook("on_quit");
        return false;
      }
      return true;
    }
    if (lc == "q!" || lc == "quit!") {
      discard_all_recovery();
      cout << P.dim << "bye!" << C_RESET << "\n";
      (void)run_hook("on_quit");
      return false;
    }
    if (lc == "w!" || lc == "write!") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() > 1) {
        cout << P.warn << "usage: write! [path]" << C_RESET << "\n";
        return true;
      }
      string target = args.empty() ? buf.path : expand_path(args[0]);
      if (target.empty()) {
        cout << P.warn << "save: no filename (use: write! <path>)" << C_RESET
             << "\n";
        return true;
      }
      string old_recovery = recover_path_for(buf);
      string old_legacy_recovery = legacy_recover_path_for(buf);
      string err;
      if (atomic_save(target, buf, false, err)) {
        if (target != buf.path)
          buf.path = target;
        buf.dirty = false;
        buf.undo.mark_all_dirty();
        buf.redo.mark_all_dirty();
        add_recent(target);
        note("force-saved " + target);
        unlink(old_recovery.c_str());
        unlink(old_legacy_recovery.c_str());
        unlink(recover_path_for(buf).c_str());
        unlink(legacy_recover_path_for(buf).c_str());
        cout << P.ok << "saved to " << escape_for_display(target) << C_RESET
             << "\n";
        (void)run_hook("on_save");
      } else
        cout << P.err << "save: " << err << C_RESET << "\n";
      return true;
    }
    if (lc == "w") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() > 1) {
        cout << P.warn << "usage: w [path]" << C_RESET << "\n";
        return true;
      }
      save(args.empty() ? string() : args[0]);
      return true;
    }
    if (lc == "write") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() > 2) {
        cout << P.warn << "usage: write [range] [path]" << C_RESET << "\n";
        return true;
      }
      if (args.size() < 2 || !looks_like_range_token(args[0])) {
        if (args.size() > 1) {
          cout << P.warn << "usage: write [range] [path]" << C_RESET << "\n";
          return true;
        }
        save(args.empty() ? string() : args[0]);
        return true;
      }
    }
    if (lc == "saveas") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() != 1) {
        cout << P.warn << "usage: saveas <path>" << C_RESET << "\n";
        return true;
      }
      save(args[0]);
      return true;
    }

    if (lc == "quit" || lc == "q") {
      if (guard_dirty_backgrounds("quit"))
        return true;
      if (buf.dirty) {
        if (ed_mode) {
          if (!ed_quit_warned) {
            cout << "?\n";
            ed_quit_warned = true;
            return true;
          }
          discard_recovery(buf);
          return false;
        }
        cout << P.warn << "Save changes to file? [y]es/[n]o/[c]ancel "
             << C_RESET << std::flush;
        char c = 0;
        std::cin.get(c);
        string dump;
        std::getline(std::cin, dump);
        if (c == 'y' || c == 'Y') {
          if (!save(""))
            return true;
        } else if (c == 'c' || c == 'C')
          return true;
        else
          discard_recovery(buf);
      }
      cout << P.dim << "bye!" << C_RESET << "\n";
      (void)run_hook("on_quit");
      return false;
    }

    if (lc == "print" || lc == "p") {
      size_t lo = 1, hi = buf.lines.size();
      if (!parse_range(rest, buf.lines.size(), lo, hi)) {
        cout << P.warn << "bad range" << C_RESET << "\n";
        return true;
      }
      print(lo, hi);
      return true;
    }
    if (lc == "r") {
      long n = 0;
      if (!parse_long(rest, n)) {
        cout << P.warn << "usage: r <n>" << C_RESET << "\n";
        return true;
      }
      if (n < 1 || (size_t)n > buf.lines.size()) {
        cout << P.warn << "no such line" << C_RESET << "\n";
        return true;
      }
      print((size_t)n, (size_t)n);
      return true;
    }
    if (lc == "goto") {
      long n = 0;
      if (!parse_long(rest, n)) {
        cout << P.warn << "usage: goto <n>" << C_RESET << "\n";
        return true;
      }
      if (n < 1 || (size_t)n > buf.lines.size()) {
        cout << P.warn << "no such line" << C_RESET << "\n";
        return true;
      }
      print((size_t)n, (size_t)n);
      return true;
    }

    if (lc == "append" || lc == "a") {
      append_mode();
      return true;
    }
    if (lc == "insert" || lc == "i") {
      long n = 0;
      if (!parse_long(rest, n)) {
        cout << P.warn << "usage: insert <n>" << C_RESET << "\n";
        return true;
      }
      if (n < 1 || (size_t)n > buf.lines.size() + 1) {
        cout << P.warn << "invalid target line" << C_RESET << "\n";
        return true;
      }
      insert_mode((size_t)n - 1);
      return true;
    }

    if (lc == "edit") {
      std::istringstream ts(rest);
      string ntok;
      ts >> ntok;
      long n = 0;
      if (ntok.empty() || !parse_long(ntok, n)) {
        cout << P.warn << "usage: edit <n> [text...]" << C_RESET << "\n";
        return true;
      }
      if (n < 1 || (size_t)n > buf.lines.size()) {
        cout << P.warn << "no such line" << C_RESET << "\n";
        return true;
      }

      string after;
      std::getline(ts, after);

      if (after.empty()) {
        cout << "old: " << buf.lines[(size_t)n - 1] << "\n";
        cout << "new> " << std::flush;
        string newline;
        if (!std::getline(std::cin, newline)) {
          cout << "\n";
          return true;
        }
        push_undo();
        buf.lines[(size_t)n - 1] = newline;
        buf.current_line = (size_t)n;
        buf.dirty = true;
        cout << "edited line " << n << "\n";
        return true;
      }

      if (!after.empty() && (after[0] == ' ' || after[0] == '\t'))
        after.erase(after.begin());

      push_undo();
      buf.lines[(size_t)n - 1] = after;
      buf.current_line = (size_t)n;
      buf.dirty = true;
      cout << "edited line " << n << "\n";
      return true;
    }

    if (lc == "delete" || lc == "d") {
      if (buf.lines.empty()) {
        cout << "(empty)\n";
        return true;
      }
      size_t lo = 1, hi = buf.lines.size();
      if (!parse_range(rest, buf.lines.size(), lo, hi)) {
        cout << P.warn << "bad range" << C_RESET << "\n";
        return true;
      }
      push_undo();
      size_t count = hi - lo + 1;
      buf.lines.erase(buf.lines.begin() + (long)lo - 1,
                      buf.lines.begin() + (long)lo - 1 + (long)count);
      buf.current_line = buf.lines.empty() ? 0 : std::min(lo, buf.lines.size());
      buf.dirty = true;
      cout << "deleted " << count << " line(s)\n";
      return true;
    }

    if (lc == "move" || lc == "m") {
      std::istringstream ts(rest);
      long from = 0, to = 0;
      string extra;
      if (!(ts >> from >> to) || (ts >> extra)) {
        cout << P.warn << "usage: move <from> <to>" << C_RESET << "\n";
        return true;
      }
      if (from < 1 || (size_t)from > buf.lines.size() || to < 0 ||
          (size_t)to > buf.lines.size()) {
        cout << P.warn << "bad indexes" << C_RESET << "\n";
        return true;
      }
      push_undo();
      string s = buf.lines[(size_t)from - 1];
      buf.lines.erase(buf.lines.begin() + (long)from - 1);
      if (to > from)
        to--;
      if (to > (long)buf.lines.size())
        to = (long)buf.lines.size();
      buf.lines.insert(buf.lines.begin() + (long)to, s);
      buf.current_line = (size_t)to + 1;
      buf.dirty = true;
      cout << "moved line " << from << " to " << to << "\n";
      return true;
    }

    if (lc == "join") {
      size_t lo = 1, hi = buf.lines.size();
      if (!parse_range(rest, buf.lines.size(), lo, hi) || hi <= lo) {
        cout << P.warn << "bad range" << C_RESET << "\n";
        return true;
      }
      push_undo();
      std::ostringstream out;
      for (size_t i = lo; i <= hi; i++) {
        if (i > lo)
          out << " ";
        out << buf.lines[i - 1];
      }
      buf.lines.erase(buf.lines.begin() + (long)lo - 1,
                      buf.lines.begin() + (long)hi);
      buf.lines.insert(buf.lines.begin() + (long)lo - 1, out.str());
      buf.current_line = lo;
      buf.dirty = true;
      cout << "joined\n";
      return true;
    }

    if (lc == "indent") {
      if (buf.lines.empty()) {
        cout << "(empty)\n";
        return true;
      }
      size_t lo = 1, hi = buf.lines.size();
      if (!parse_range(rest, buf.lines.size(), lo, hi)) {
        cout << P.warn << "bad range" << C_RESET << "\n";
        return true;
      }
      if (!indentation_language(lang)) {
        cout << P.warn << "indent: no indentation rules for " << lang_name()
             << C_RESET << "\n";
        return true;
      }
      vector<string> indented = reindent_lines(buf, lo, hi, lang, indent_width);
      if (indented == buf.lines) {
        cout << "indent: no changes\n";
        return true;
      }
      push_undo();
      buf.lines.swap(indented);
      buf.dirty = true;
      cout << "indented " << lo << "-" << hi << "\n";
      return true;
    }

    if (lc == "find") {
      if (rest.empty()) {
        cout << P.warn << "usage: find <text>" << C_RESET << "\n";
        return true;
      }
      last_search = rest;
      last_icase = false;
      last_index = 0;
      search_plain(buf, rest, false);
      return true;
    }
    if (lc == "findi") {
      if (rest.empty()) {
        cout << P.warn << "usage: findi <text>" << C_RESET << "\n";
        return true;
      }
      last_search = rest;
      last_icase = true;
      last_index = 0;
      search_plain(buf, rest, true);
      return true;
    }
    if (lc == "findre") {
      bool icase = false;
      string pat = rest;
      if (pat.rfind("-i ", 0) == 0) {
        icase = true;
        pat = trim_copy(pat.substr(3));
      }
      if (pat.empty()) {
        cout << P.warn << "usage: findre [-i] <regex>" << C_RESET << "\n";
        return true;
      }
      search_regex(buf, pat, icase);
      return true;
    }
    if (lc == "findrei") {
      if (rest.empty()) {
        cout << P.warn << "usage: findrei <regex>" << C_RESET << "\n";
        return true;
      }
      search_regex(buf, rest, true);
      return true;
    }
    if (cmd == "N") {
      next_match(true);
      return true;
    }
    if (lc == "n") {
      next_match(false);
      return true;
    }

    if (lc == "repl" || lc == "replg") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() != 2) {
        cout << P.warn << "usage: repl[g] <old> <new>" << C_RESET << "\n";
        return true;
      }
      repl(lc == "replg", args[0], args[1]);
      return true;
    }

    if (lc == "read") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.empty() ||
          args.size() > 2) {
        cout << P.warn << "usage: read <path> [n]" << C_RESET << "\n";
        return true;
      }
      string p = expand_path(args[0]);
      long n = -1;
      if (args.size() == 2 && (!parse_long(args[1], n) || n < 0)) {
        cout << P.warn << "usage: read <path> [n]" << C_RESET << "\n";
        return true;
      }
      std::ifstream in2(p);
      if (!in2.good()) {
        cout << P.err << "read: cannot open" << C_RESET << "\n";
        return true;
      }
      vector<string> R;
      string L;
      while (std::getline(in2, L)) {
        rstrip_newline(L);
        R.push_back(L);
      }
      if (in2.bad()) {
        cout << P.err << "read: I/O error" << C_RESET << "\n";
        return true;
      }
      if (R.empty()) {
        cout << "read 0 line(s) from " << escape_for_display(p) << "\n";
        return true;
      }
      size_t at = (n < 0) ? buf.lines.size() : (size_t)n;
      if (at > buf.lines.size())
        at = buf.lines.size();
      bool appended = at == buf.lines.size();
      push_undo();
      buf.lines.insert(buf.lines.begin() + (long)at, R.begin(), R.end());
      if (appended)
        buf.final_newline = true;
      buf.dirty = true;
      cout << "read " << R.size() << " line(s) from " << escape_for_display(p)
           << "\n";
      return true;
    }

    if (lc == "write") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() != 2) {
        cout << P.warn << "usage: write <range> <path>" << C_RESET << "\n";
        return true;
      }
      size_t lo = 1, hi = buf.lines.size();
      if (!parse_range(args[0], buf.lines.size(), lo, hi)) {
        cout << P.warn << "bad range" << C_RESET << "\n";
        return true;
      }
      string outp = args[1];
      outp = expand_path(outp);
      Buffer tmp;
      if (hi >= lo)
        tmp.lines.assign(buf.lines.begin() + (long)lo - 1,
                         buf.lines.begin() + (long)hi);
      string err;
      if (atomic_save(outp, tmp, buf.backup, err)) {
        cout << "wrote " << (hi >= lo ? hi - lo + 1 : 0) << " line(s) to "
             << outp << "\n";
      } else
        cout << P.err << "write: " << err << C_RESET << "\n";
      return true;
    }

    if (lc == "filter") {
      std::istringstream ts(rest);
      string rng;
      ts >> rng;
      string ex;
      std::getline(ts, ex);
      ex = trim_copy(ex);
      if (ex.empty() || ex[0] != '!') {
        cout << P.warn << "usage: filter <range> !shell" << C_RESET << "\n";
        return true;
      }
      size_t lo = 1, hi = buf.lines.size();
      if (!parse_range(rng, buf.lines.size(), lo, hi)) {
        cout << P.warn << "bad range" << C_RESET << "\n";
        return true;
      }
      string ferr;
      vector<string> filtered = buf.lines;
      if (run_filter_replace(filtered, lo, hi, ex.substr(1), ferr)) {
        push_undo();
        buf.lines.swap(filtered);
        buf.dirty = true;
        cout << "filtered\n";
      } else {
        cout << P.err << "filter failed: " << ferr << C_RESET << "\n";
      }
      return true;
    }

    if (lc == "undo" || lc == "u") {
      long k = 1;
      if (!rest.empty() && (!parse_long(rest, k) || k < 1)) {
        cout << P.warn << "usage: undo [positive-count]" << C_RESET << "\n";
        return true;
      }
      bool any = false;
      while (k-- > 0) {
        Snap s;
        if (!buf.undo.pop(s)) {
          if (!any)
            cout << "nothing to undo\n";
          break;
        }
        buf.redo.push(buf.lines, buf.dirty, buf.final_newline,
                      buf.current_line);
        buf.lines = std::move(s.lines);
        buf.dirty = s.dirty;
        buf.final_newline = s.final_newline;
        buf.current_line = s.current_line;
        any = true;
      }
      if (any)
        cout << "undo\n";
      return true;
    }
    if (lc == "redo") {
      Snap s;
      if (!buf.redo.pop(s)) {
        cout << "nothing to redo\n";
        return true;
      }
      buf.undo.push(buf.lines, buf.dirty, buf.final_newline, buf.current_line);
      buf.lines = std::move(s.lines);
      buf.dirty = s.dirty;
      buf.final_newline = s.final_newline;
      buf.current_line = s.current_line;
      cout << "redo\n";
      return true;
    }

    if (lc == "set") {
      std::istringstream ts(rest);
      string what, val;
      ts >> what >> val;
      what = lower(what);
      val = lower(val);
      if (what.empty()) {
        show_settings();
      } else if (what == "number") {
        if (val == "on" || val == "1" || val == "true") {
          buf.number = true;
          cout << "number: on\n";
          save_config();
        } else if (val == "off" || val == "0" || val == "false") {
          buf.number = false;
          cout << "number: off\n";
          save_config();
        } else
          cout << P.warn << "usage: set number on|off" << C_RESET << "\n";
      } else if (what == "backup") {
        if (val == "on" || val == "1" || val == "true") {
          buf.backup = true;
          cout << "backup: on\n";
          save_config();
        } else if (val == "off" || val == "0" || val == "false") {
          buf.backup = false;
          cout << "backup: off\n";
          save_config();
        } else
          cout << P.warn << "usage: set backup on|off" << C_RESET << "\n";
      } else if (what == "autosave") {
        long s = 0;
        if (!parse_long(val, s)) {
          cout << P.warn << "usage: set autosave <seconds>" << C_RESET << "\n";
          return true;
        }
        autosave_sec = (int)std::min<long>(INT_MAX, std::max<long>(0, s));
        cout << "autosave: " << autosave_sec << "s\n";
        save_config();
      } else if (what == "wrap") {
        bool b = false;
        if (!parse_bool_string(val, b)) {
          cout << P.warn << "usage: set wrap on|off" << C_RESET << "\n";
          return true;
        }
        wrap_long = b;
        cout << "wrap: " << (wrap_long ? "on" : "off") << "\n";
        save_config();
      } else if (what == "truncate") {
        bool b = false;
        if (!parse_bool_string(val, b)) {
          cout << P.warn << "usage: set truncate on|off" << C_RESET << "\n";
          return true;
        }
        truncate_long = b;
        cout << "truncate: " << (truncate_long ? "on" : "off") << "\n";
        save_config();
      } else if (what == "status") {
        bool b = false;
        if (!parse_bool_string(val, b)) {
          cout << P.warn << "usage: set status on|off" << C_RESET << "\n";
          return true;
        }
        show_status_line = b;
        cout << "status: " << (show_status_line ? "on" : "off") << "\n";
        save_config();
      } else if (what == "indentwidth") {
        long n = 0;
        if (!parse_long(val, n) || n < 1 || n > 16) {
          cout << P.warn << "usage: set indentwidth <1-16>" << C_RESET << "\n";
          return true;
        }
        indent_width = (int)n;
        cout << "indentwidth: " << indent_width << "\n";
        save_config();
      } else if (what == "edmode") {
        bool b = false;
        if (!parse_bool_string(val, b)) {
          cout << P.warn << "usage: set edmode on|off" << C_RESET << "\n";
          return true;
        }
        ed_mode = b;
        ed_quit_warned = false;
        cout << "edmode: " << (ed_mode ? "on" : "off") << "\n";
        save_config();
      } else if (what == "lang") {
        if (val == "cpp" || val == "c" || val == "c++" || val == "hpp" ||
            val == "h")
          lang = Lang::Cpp;
        else if (val == "py" || val == "python")
          lang = Lang::Python;
        else if (val == "sh" || val == "bash" || val == "zsh" || val == "shell")
          lang = Lang::Shell;
        else if (val == "rb" || val == "ruby")
          lang = Lang::Ruby;
        else if (val == "js" || val == "javascript" || val == "ts" ||
                 val == "typescript")
          lang = Lang::JS;
        else if (val == "html" || val == "htm")
          lang = Lang::HTML;
        else if (val == "css")
          lang = Lang::CSS;
        else if (val == "json")
          lang = Lang::JSON;
        else if (val == "go")
          lang = Lang::Go;
        else if (val == "rs" || val == "rust")
          lang = Lang::Rust;
        else if (val == "java")
          lang = Lang::Java;
        else if (val == "cs" || val == "csharp" || val == "c#")
          lang = Lang::CSharp;
        else if (val == "php")
          lang = Lang::PHP;
        else if (val == "md" || val == "markdown")
          lang = Lang::Markdown;
        else if (val == "yaml" || val == "yml")
          lang = Lang::YAML;
        else if (val == "toml")
          lang = Lang::TOML;
        else if (val == "sql")
          lang = Lang::SQL;
        else if (val == "xml" || val == "svg")
          lang = Lang::XML;
        else if (val == "swift")
          lang = Lang::Swift;
        else if (val == "kt" || val == "kotlin")
          lang = Lang::Kotlin;
        else if (val == "lua")
          lang = Lang::Lua;
        else if (val == "pl" || val == "perl")
          lang = Lang::Perl;
        else if (val == "r")
          lang = Lang::R;
        else if (val == "make" || val == "makefile" || val == "mk")
          lang = Lang::Make;
        else if (val == "docker" || val == "dockerfile")
          lang = Lang::Docker;
        else if (val == "ini" || val == "config" || val == "conf")
          lang = Lang::Ini;
        else if (val == "diff" || val == "patch")
          lang = Lang::Diff;
        else if (val == "modula2" || val == "mod2" || val == "m2")
          lang = Lang::Modula2;
        else if (val == "modula3" || val == "mod3" || val == "m3")
          lang = Lang::Modula3;
        else if (val == "pascal" || val == "delphi" || val == "fpc" ||
                 val == "objectpascal")
          lang = Lang::ObjectPascal;
        else
          lang = Lang::Plain;
        cout << "lang: set\n";
      } else
        cout << P.warn << "unknown setting" << C_RESET << "\n";
      return true;
    }

    if (lc == "syntax") {
      if (rest.empty()) {
        cout << P.warn << "usage: syntax <name>" << C_RESET << "\n";
        return true;
      }
      return handle("set lang " + rest);
    }

    if (lc == "edmode") {
      if (rest.empty())
        return handle("set edmode " + string(ed_mode ? "off" : "on"));
      return handle("set edmode " + rest);
    }

    if (lc == "number") {
      buf.number = !buf.number;
      cout << "number: " << (buf.number ? "on" : "off") << "\n";
      save_config();
      return true;
    }

    if (lc == "theme") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() != 1) {
        cout << P.warn << "usage: theme <name>" << C_RESET << "\n";
        return true;
      }
      if (lower(args[0]) == "preview") {
        theme_preview();
        return true;
      }
      cycle_theme(args[0]);
      return true;
    }

    if (lc == "config") {
      show_config_paths();
      return true;
    }
    if (lc == "recent") {
      show_recent();
      return true;
    }
    if (lc == "messages") {
      show_messages();
      return true;
    }
    if (lc == "history") {
      show_history();
      return true;
    }
    if (lc == "aliases") {
      show_aliases();
      return true;
    }

    if (lc == "lua-themes") {
      list_lua_themes();
      return true;
    }

    if (lc == "highlight") {
      string v = lower(rest);
      if (v == "on" || v == "1" || v == "true") {
        buf.highlight = true;
        cout << "highlight: on\n";
        save_config();
      } else if (v == "off" || v == "0" || v == "false") {
        buf.highlight = false;
        cout << "highlight: off\n";
        save_config();
      } else
        cout << P.warn << "usage: highlight on|off" << C_RESET << "\n";
      return true;
    }

    if (lc == "alias") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() < 2) {
        cout << P.warn << "usage: alias <from> <to...>" << C_RESET << "\n";
        return true;
      }
      string from = args[0];
      string to = join_words(args, 1);
      if (from.empty() || to.empty()) {
        cout << P.warn << "usage: alias <from> <to...>" << C_RESET << "\n";
        return true;
      }
      aliases[from] = to;
      cout << "alias: " << from << " -> " << to << "\n";
      save_config();
      return true;
    }
    if (lc == "new") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() > 1) {
        cout << P.warn << "usage: new [path]" << C_RESET << "\n";
        return true;
      }
      string p = args.empty() ? string() : expand_path(args[0]);
      open_new_buffer(p);
      return true;
    }
    if (lc == "bnext") {
      bnext();
      return true;
    }
    if (lc == "bprev") {
      bprev();
      return true;
    }
    if (lc == "lsb") {
      list_buffers();
      return true;
    }
    if (lc == "buffer") {
      long n = 0;
      if (!parse_long(rest, n) || n < 0) {
        cout << P.warn << "usage: buffer <n>" << C_RESET << "\n";
        return true;
      }
      switch_buffer((size_t)n);
      return true;
    }
    if (lc == "close") {
      close_buffer();
      return true;
    }

    if (lc == "diff") {
      show_diff();
      return true;
    }

    if (lc == "pwd") {
      std::error_code ec;
      auto p = fs::current_path(ec);
      if (ec)
        cout << P.err << "pwd: " << ec.message() << C_RESET << "\n";
      else
        cout << p.string() << "\n";
      return true;
    }
    if (lc == "ls") {
      bool all = false, longfmt = false;
      string target = ".";
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error)) {
        cout << P.warn << "ls: " << parse_error << C_RESET << "\n";
        return true;
      }
      for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-a")
          all = true;
        else if (args[i] == "-l")
          longfmt = true;
        else
          target = args[i];
      }
      if (target.empty())
        target = ".";
      ls_list(target, all, longfmt);
      return true;
    }

    if (lc == "cd") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() != 1) {
        cout << P.warn
             << "cd: requires a directory path (try ., .., ~, or a folder name)"
             << C_RESET << "\n";
        return true;
      }
      string target = expand_path(args[0]);
      std::error_code ec;
      fs::file_status st = fs::status(target, ec);
      if (ec || !fs::exists(st)) {
        cout << P.err << "cd: no such directory: " << target << C_RESET << "\n";
        return true;
      }
      if (!fs::is_directory(st)) {
        cout << P.err << "cd: not a directory: " << target << C_RESET << "\n";
        return true;
      }
      fs::current_path(target, ec);
      if (ec)
        cout << P.err << "cd: " << ec.message() << C_RESET << "\n";
      else
        cout << P.ok << "cd: " << fs::current_path().string() << C_RESET
             << "\n";
      return true;
    }

    if (lc == "clear") {
      clear_screen();
      return true;
    }

    if (lc == "lsp") {
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.empty()) {
        cout << P.warn
             << "usage: lsp start <server...> | lsp format | lsp status | lsp "
                "stop"
             << C_RESET << "\n";
        return true;
      }
      string action = lower(args[0]);
      if (action == "status") {
        cout << "lsp: " << (lsp.running() ? "running" : "stopped") << "\n";
        return true;
      }
      if (action == "stop") {
        lsp.stop();
        cout << "lsp: stopped\n";
        return true;
      }
      if (action == "start") {
        if (args.size() < 2) {
          cout << P.warn << "usage: lsp start <server...>" << C_RESET << "\n";
          return true;
        }
        vector<string> command(args.begin() + 1, args.end());
        string error;
        if (lsp.start(command, error))
          cout << P.ok << "lsp: started" << C_RESET << "\n";
        else
          cout << P.err << "lsp: " << error << C_RESET << "\n";
        return true;
      }
      if (action == "format" || action == "indent") {
        if (buf.path.empty()) {
          cout << P.warn << "lsp: save the buffer before formatting" << C_RESET
               << "\n";
          return true;
        }
        Buffer formatted;
        formatted.path = buf.path;
        formatted.lines = buf.lines;
        formatted.final_newline = buf.final_newline;
        string error;
        if (!lsp.format(formatted, lang_name(), indent_width, error)) {
          cout << P.err << "lsp: " << error << C_RESET << "\n";
          return true;
        }
        if (formatted.lines == buf.lines &&
            formatted.final_newline == buf.final_newline) {
          cout << "lsp: no formatting changes\n";
          return true;
        }
        push_undo();
        buf.lines.swap(formatted.lines);
        buf.final_newline = formatted.final_newline;
        buf.dirty = true;
        buf.current_line = buf.lines.empty()
                               ? 0
                               : std::min(buf.current_line, buf.lines.size());
        cout << P.ok << "lsp: formatting applied" << C_RESET << "\n";
        return true;
      }
      cout
          << P.warn
          << "usage: lsp start <server...> | lsp format | lsp status | lsp stop"
          << C_RESET << "\n";
      return true;
    }

    if (lc == "lua") {
      if (!L) {
        cout << P.err << "lua: not available" << C_RESET << "\n";
        return true;
      }
      if (rest.empty()) {
        cout << P.warn << "usage: lua <code>" << C_RESET << "\n";
        return true;
      }
      int rc = luaL_loadstring(L, rest.c_str());
      if (rc != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        cout << P.err << "lua: " << (msg ? msg : "") << C_RESET << "\n";
        lua_pop(L, 1);
        return true;
      }
      rc = lua_pcall(L, 0, LUA_MULTRET, 0);
      if (rc != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        cout << P.err << "lua: " << (msg ? msg : "") << C_RESET << "\n";
        lua_pop(L, 1);
      }
      fflush(stdout);
      return true;
    }

    if (lc == "luafile") {
      if (!L) {
        cout << P.err << "luafile: lua not available" << C_RESET << "\n";
        return true;
      }
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() != 1) {
        cout << P.warn << "usage: luafile <path>" << C_RESET << "\n";
        return true;
      }
      string p = expand_path(args[0]);
      int rc = luaL_loadfile(L, p.c_str());
      if (rc != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        cout << P.err << "luafile: " << (msg ? msg : "") << C_RESET << "\n";
        lua_pop(L, 1);
        return true;
      }
      rc = lua_pcall(L, 0, 0, 0);
      if (rc != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        cout << P.err << "luafile: " << (msg ? msg : "") << C_RESET << "\n";
        lua_pop(L, 1);
      }
      fflush(stdout);
      return true;
    }

    if (lc == "plugin") {
      std::istringstream ps(rest);
      string sub, key;
      ps >> sub >> key;
      sub = lower(sub);
      if (sub == "trusted" || sub == "list") {
        if (trusted_plugins.empty())
          cout << "no trusted plugins\n";
        else
          for (const auto &k : trusted_plugins)
            cout << "- " << k << "\n";
        return true;
      }
      if (key.empty()) {
        cout << P.warn << "usage: plugin trust|untrust <name|path>" << C_RESET
             << "\n";
        return true;
      }
      string trust_key = plugin_key_for(key);
      if (sub == "trust") {
        trust_plugin_key(trust_key);
        cout << "trusted plugin: " << trust_key << "\n";
        return true;
      }
      if (sub == "untrust") {
        if (untrust_plugin_key(trust_key))
          cout << "untrusted plugin: " << trust_key << "\n";
        else
          cout << "plugin was not trusted: " << trust_key << "\n";
        return true;
      }
      cout << P.warn << "usage: plugin trust|untrust|trusted <name|path>"
           << C_RESET << "\n";
      return true;
    }

    if (lc == "run-plugin") {
      if (!had_colon) {
        cout << P.warn
             << "run-plugin must be invoked as :run-plugin <name|path>"
             << C_RESET << "\n";
        return true;
      }
      if (!L) {
        cout << P.err << "run-plugin: lua not available" << C_RESET << "\n";
        return true;
      }
      vector<string> args;
      string parse_error;
      if (!split_quoted_words(rest, args, parse_error) || args.size() != 1) {
        cout << P.warn << "usage: run-plugin <name|path>" << C_RESET << "\n";
        return true;
      }
      string key = args[0];
      string path;
      string disp;
      auto itp = plugin_files.find(key);
      if (itp != plugin_files.end()) {
        path = itp->second;
        disp = key;
      } else {
        string p = expand_path(key);
        if (!file_exists(p)) {
          cout << P.err << "run-plugin: no such plugin or path: " << key
               << C_RESET << "\n";
          return true;
        }
        path = p;
        disp = fs::path(path).filename().string();
      }
      (void)run_plugin_file(disp, path);
      return true;
    }

    if (lc == "plugins") {
      if (plugin_names.empty()) {
        cout << "no plugins found (reload-plugins to rescan)\n";
      } else {
        cout << "available plugins:\n";
        for (const auto &n : plugin_names) {
          auto itp = plugin_files.find(n);
          bool is_current = (!current_plugin.empty() && current_plugin == n);
          if (itp != plugin_files.end()) {
            cout << "- " << n << " (" << itp->second << ")"
                 << (is_current ? " *" : "") << "\n";
          } else {
            cout << "- " << n << (is_current ? " *" : "") << "\n";
          }
        }
      }
      return true;
    }

    if (lc == "reload-plugins") {
      if (!L) {
        cout << P.err << "reload-plugins: lua not available" << C_RESET << "\n";
        return true;
      }
      load_lua_plugins();
      cout << "plugins reloaded\n";
      return true;
    }

    if (lc == "version" || lc == "ver") {
      cout << P.title << "tedit " << TEDIT_VERSION << C_RESET << "\n";
      return true;
    }

    if (ed_mode)
      cout << "?\n";
    else
      cout << P.warn << "unknown command - type 'help'" << C_RESET << "\n";
    return true;
  }
};
