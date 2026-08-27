static bool use_color() {
  const char *no_color = std::getenv("NO_COLOR");
  return is_tty_stdout() && (!no_color || !*no_color);
}
static string color_code(const char *code) {
  return use_color() ? string(code) : string();
}
static const string C_RESET = color_code("\033[0m");
static const string C_DIM = color_code("\033[2m");
static const string C_BOLD = color_code("\033[1m");
static const string C_GREEN = color_code("\033[32m");
static const string C_RED = color_code("\033[31m");
static const string C_CYAN = color_code("\033[36m");
static const string C_BLUE = color_code("\033[34m");
static const string C_YEL = color_code("\033[33m");
static const string C_BRIGHT_BLACK = color_code("\033[90m");
static const string C_BRIGHT_WHITE = color_code("\033[97m");
static const string C_BRIGHT_CYAN = color_code("\033[96m");
static const string C_BRIGHT_BLUE = color_code("\033[94m");
static const string C_BRIGHT_GREEN = color_code("\033[92m");
static const string C_BRIGHT_YEL = color_code("\033[93m");
static const string C_BRIGHT_RED = color_code("\033[91m");
static const string C_MAGENTA = color_code("\033[35m");
static const string C_BRIGHT_MAGENTA = color_code("\033[95m");

enum class Theme {
  Default,
  Dark,
  Neon,
  Matrix,
  Paper,
  Yellow,
  Iceberg,
  Ocean,
  Solarized,
  Gruvbox,
  Rose
};

struct ThemePalette {
  string accent, ok, warn, err, dim;
  string prompt, input, gutter, title;
  string help_cmd, help_arg, help_text;
  string syntax_keyword, syntax_control, syntax_declaration, syntax_type;
  string syntax_builtin, syntax_function, syntax_string, syntax_number;
  string syntax_literal, syntax_comment, syntax_operator, syntax_preprocessor;
  string syntax_attribute, syntax_key, syntax_tag, syntax_selector;
  string syntax_variable, syntax_punctuation;

  ThemePalette() = default;
  ThemePalette(string accent_, string ok_, string warn_, string err_,
               string dim_, string prompt_, string input_, string gutter_,
               string title_, string help_cmd_, string help_arg_,
               string help_text_)
      : accent(std::move(accent_)), ok(std::move(ok_)), warn(std::move(warn_)),
        err(std::move(err_)), dim(std::move(dim_)), prompt(std::move(prompt_)),
        input(std::move(input_)), gutter(std::move(gutter_)),
        title(std::move(title_)), help_cmd(std::move(help_cmd_)),
        help_arg(std::move(help_arg_)), help_text(std::move(help_text_)) {}
};

static ThemePalette with_syntax_palette(ThemePalette P, Theme t) {
  P.syntax_comment = P.dim;
  P.syntax_string = C_GREEN;
  P.syntax_number = C_BRIGHT_YEL;
  P.syntax_literal = C_YEL;
  P.syntax_operator = C_BRIGHT_WHITE;
  P.syntax_punctuation = C_BRIGHT_BLACK;
  P.syntax_variable = C_YEL;

  switch (t) {
  case Theme::Neon:
  case Theme::Rose:
    P.syntax_keyword = C_BRIGHT_MAGENTA;
    P.syntax_control = C_MAGENTA;
    P.syntax_declaration = C_BRIGHT_BLUE;
    P.syntax_type = C_BRIGHT_CYAN;
    P.syntax_builtin = C_CYAN;
    P.syntax_function = C_BRIGHT_BLUE;
    P.syntax_preprocessor = C_BRIGHT_MAGENTA;
    P.syntax_attribute = C_CYAN;
    P.syntax_key = C_BRIGHT_BLUE;
    P.syntax_tag = C_BRIGHT_MAGENTA;
    P.syntax_selector = C_BRIGHT_CYAN;
    break;
  case Theme::Matrix:
    P.syntax_keyword = C_BRIGHT_GREEN;
    P.syntax_control = C_GREEN;
    P.syntax_declaration = C_BRIGHT_CYAN;
    P.syntax_type = C_CYAN;
    P.syntax_builtin = C_BRIGHT_GREEN;
    P.syntax_function = C_BRIGHT_WHITE;
    P.syntax_preprocessor = C_YEL;
    P.syntax_attribute = C_CYAN;
    P.syntax_key = C_BRIGHT_CYAN;
    P.syntax_tag = C_BRIGHT_GREEN;
    P.syntax_selector = C_BRIGHT_WHITE;
    break;
  case Theme::Paper:
    P.syntax_keyword = C_MAGENTA;
    P.syntax_control = C_RED;
    P.syntax_declaration = C_BLUE;
    P.syntax_type = C_CYAN;
    P.syntax_builtin = C_BLUE;
    P.syntax_function = C_MAGENTA;
    P.syntax_preprocessor = C_RED;
    P.syntax_attribute = C_CYAN;
    P.syntax_key = C_BLUE;
    P.syntax_tag = C_MAGENTA;
    P.syntax_selector = C_CYAN;
    break;
  case Theme::Yellow:
  case Theme::Gruvbox:
    P.syntax_keyword = C_BRIGHT_YEL;
    P.syntax_control = C_BRIGHT_RED;
    P.syntax_declaration = C_MAGENTA;
    P.syntax_type = C_BRIGHT_CYAN;
    P.syntax_builtin = C_CYAN;
    P.syntax_function = C_BRIGHT_GREEN;
    P.syntax_preprocessor = C_BRIGHT_MAGENTA;
    P.syntax_attribute = C_CYAN;
    P.syntax_key = C_BRIGHT_BLUE;
    P.syntax_tag = C_BRIGHT_YEL;
    P.syntax_selector = C_BRIGHT_CYAN;
    break;
  case Theme::Iceberg:
  case Theme::Ocean:
  case Theme::Solarized:
    P.syntax_keyword = C_BLUE;
    P.syntax_control = C_BRIGHT_MAGENTA;
    P.syntax_declaration = C_BRIGHT_BLUE;
    P.syntax_type = C_BRIGHT_CYAN;
    P.syntax_builtin = C_CYAN;
    P.syntax_function = C_BRIGHT_GREEN;
    P.syntax_preprocessor = C_MAGENTA;
    P.syntax_attribute = C_BRIGHT_CYAN;
    P.syntax_key = C_BRIGHT_BLUE;
    P.syntax_tag = C_BRIGHT_MAGENTA;
    P.syntax_selector = C_CYAN;
    break;
  default:
    P.syntax_keyword = C_MAGENTA;
    P.syntax_control = C_BRIGHT_MAGENTA;
    P.syntax_declaration = C_BLUE;
    P.syntax_type = C_BRIGHT_CYAN;
    P.syntax_builtin = C_CYAN;
    P.syntax_function = C_BRIGHT_BLUE;
    P.syntax_preprocessor = C_BRIGHT_MAGENTA;
    P.syntax_attribute = C_BRIGHT_CYAN;
    P.syntax_key = C_BLUE;
    P.syntax_tag = C_BRIGHT_MAGENTA;
    P.syntax_selector = C_BRIGHT_CYAN;
    break;
  }
  return P;
}

static ThemePalette palette_for(Theme t) {
  if (!use_color())
    return {};

  switch (t) {
  case Theme::Dark:
    return with_syntax_palette({C_CYAN, C_GREEN, C_YEL, C_RED, C_BRIGHT_BLACK,
                                C_BRIGHT_CYAN, C_BRIGHT_WHITE, C_BRIGHT_BLACK,
                                C_BOLD + C_CYAN, C_BRIGHT_CYAN, C_BRIGHT_BLACK,
                                C_BRIGHT_BLACK},
                               t);
  case Theme::Neon:
    return with_syntax_palette({C_BRIGHT_MAGENTA, C_BRIGHT_GREEN, C_BRIGHT_YEL,
                                C_BRIGHT_RED, C_BRIGHT_BLACK, C_BRIGHT_MAGENTA,
                                C_BRIGHT_CYAN, C_BRIGHT_BLACK,
                                C_BOLD + C_BRIGHT_MAGENTA, C_BRIGHT_MAGENTA,
                                C_BRIGHT_BLACK, C_BRIGHT_BLACK},
                               t);
  case Theme::Matrix:
    return with_syntax_palette({C_GREEN, C_BRIGHT_GREEN, C_YEL, C_RED,
                                C_BRIGHT_BLACK, C_BRIGHT_GREEN, C_BRIGHT_GREEN,
                                C_BRIGHT_BLACK, C_BOLD + C_GREEN,
                                C_BRIGHT_GREEN, C_BRIGHT_BLACK, C_BRIGHT_BLACK},
                               t);
  case Theme::Paper:
    return with_syntax_palette({C_BRIGHT_BLACK, C_GREEN, C_YEL, C_RED,
                                C_BRIGHT_BLACK, C_BRIGHT_BLACK, C_BRIGHT_BLACK,
                                C_BRIGHT_BLACK, C_BOLD + C_BRIGHT_BLACK,
                                C_BRIGHT_BLACK, C_BRIGHT_BLACK, C_BRIGHT_BLACK},
                               t);
  case Theme::Yellow:
    return with_syntax_palette({C_BRIGHT_YEL, C_BRIGHT_GREEN, C_YEL, C_RED,
                                C_BRIGHT_BLACK, C_BRIGHT_YEL, C_BRIGHT_WHITE,
                                C_BRIGHT_BLACK, C_BOLD + C_BRIGHT_YEL,
                                C_BRIGHT_YEL, C_BRIGHT_BLACK, C_BRIGHT_BLACK},
                               t);
  case Theme::Iceberg:
    return with_syntax_palette({C_BRIGHT_CYAN, C_BRIGHT_GREEN, C_YEL, C_RED,
                                C_BRIGHT_BLACK, C_BRIGHT_CYAN, C_BRIGHT_WHITE,
                                C_BRIGHT_BLACK, C_BOLD + C_BRIGHT_CYAN,
                                C_BRIGHT_CYAN, C_BRIGHT_BLACK, C_BRIGHT_BLACK},
                               t);
  case Theme::Ocean:
    return with_syntax_palette({C_BRIGHT_CYAN, C_BRIGHT_GREEN, C_BRIGHT_YEL,
                                C_BRIGHT_RED, C_BRIGHT_BLACK, C_BRIGHT_BLUE,
                                C_BRIGHT_WHITE, C_BLUE, C_BOLD + C_BRIGHT_BLUE,
                                C_BRIGHT_CYAN, C_BRIGHT_BLUE, C_BRIGHT_BLACK},
                               t);
  case Theme::Solarized:
    return with_syntax_palette({C_CYAN, C_GREEN, C_YEL, C_RED, C_BRIGHT_BLACK,
                                C_BLUE, C_CYAN, C_BRIGHT_BLACK, C_BOLD + C_YEL,
                                C_CYAN, C_BLUE, C_BRIGHT_BLACK},
                               t);
  case Theme::Gruvbox:
    return with_syntax_palette(
        {C_BRIGHT_YEL, C_BRIGHT_GREEN, C_YEL, C_BRIGHT_RED, C_BRIGHT_BLACK,
         C_YEL, C_BRIGHT_WHITE, C_BRIGHT_BLACK, C_BOLD + C_BRIGHT_YEL,
         C_BRIGHT_YEL, C_MAGENTA, C_BRIGHT_BLACK},
        t);
  case Theme::Rose:
    return with_syntax_palette({C_BRIGHT_MAGENTA, C_BRIGHT_GREEN, C_BRIGHT_YEL,
                                C_BRIGHT_RED, C_BRIGHT_BLACK, C_MAGENTA,
                                C_BRIGHT_WHITE, C_BRIGHT_BLACK,
                                C_BOLD + C_BRIGHT_MAGENTA, C_BRIGHT_MAGENTA,
                                C_MAGENTA, C_BRIGHT_BLACK},
                               t);
  default:
    return with_syntax_palette({C_CYAN, C_GREEN, C_YEL, C_RED, C_DIM,
                                C_BRIGHT_CYAN, C_BRIGHT_WHITE, C_BRIGHT_BLACK,
                                C_BOLD + C_CYAN, C_CYAN, C_DIM, C_DIM},
                               t);
  }
}

static bool safe_theme_name(const string &name) {
  if (name.empty() || name == "." || name == "..")
    return false;
  for (unsigned char c : name) {
    if (!std::isalnum(c) && c != '-' && c != '_')
      return false;
  }
  return true;
}

static bool safe_sgr_value(const string &value) {
  if (value.size() > 96)
    return false;
  size_t i = 0;
  while (i < value.size()) {
    if (value[i] != '\033' || i + 2 >= value.size() || value[i + 1] != '[')
      return false;
    i += 2;
    bool have_digit = false;
    while (i < value.size() && value[i] != 'm') {
      unsigned char c = (unsigned char)value[i];
      if (std::isdigit(c))
        have_digit = true;
      else if (c != ';')
        return false;
      ++i;
    }
    if (i >= value.size() || value[i] != 'm' || !have_digit)
      return false;
    ++i;
  }
  return true;
}

static void theme_instruction_limit(lua_State *L, lua_Debug *) {
  luaL_error(L, "theme exceeded instruction limit");
}

static bool load_theme_from_lua_file(const string &name, ThemePalette &outP) {
  if (!safe_theme_name(name)) {
    cout << C_RED << "theme: invalid theme name" << C_RESET << "\n";
    return false;
  }
  string dir = tedit_themes_dir();
  std::error_code ec;
  fs::create_directories(dir, ec);
  fs::path p = fs::path(dir) / (name + ".lua");

  if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) {
    return false;
  }

  lua_State *LT = luaL_newstate();
  if (!LT) {
    cout << C_RED << "theme: failed to initialize lua state" << C_RESET << "\n";
    return false;
  }
  // Theme files are data, not plugins. Keep the state sandboxed: no OS, I/O,
  // package, debug, or editor-command APIs are exposed.
  lua_sethook(LT, theme_instruction_limit, LUA_MASKCOUNT, 100000);

  int rc = luaL_loadfile(LT, p.string().c_str());
  if (rc != LUA_OK) {
    const char *msg = lua_tostring(LT, -1);
    cout << C_RED << "theme: lua load error (" << p.string()
         << "): " << (msg ? msg : "") << C_RESET << "\n";
    lua_pop(LT, 1);
    lua_close(LT);
    return false;
  }

  rc = lua_pcall(LT, 0, LUA_MULTRET, 0);
  if (rc != LUA_OK) {
    const char *msg = lua_tostring(LT, -1);
    cout << C_RED << "theme: lua runtime error (" << p.string()
         << "): " << (msg ? msg : "") << C_RESET << "\n";
    lua_pop(LT, 1);
    lua_close(LT);
    return false;
  }

  if (lua_gettop(LT) == 0) {
    lua_getglobal(LT, "theme");
  }
  if (!lua_istable(LT, -1)) {
    cout << C_RED << "theme: lua file did not return a theme table" << C_RESET
         << "\n";
    lua_close(LT);
    return false;
  }

  ThemePalette base = palette_for(Theme::Default);
  ThemePalette P = base;

  auto assign = [&](const char *key, string &dst) {
    lua_getfield(LT, -1, key);
    if (lua_isstring(LT, -1)) {
      const char *v = lua_tostring(LT, -1);
      string candidate = v ? string(v) : string();
      if (candidate.empty() || safe_sgr_value(candidate))
        dst = candidate;
    }
    lua_pop(LT, 1);
  };

  assign("accent", P.accent);
  assign("ok", P.ok);
  assign("warn", P.warn);
  assign("err", P.err);
  assign("dim", P.dim);
  assign("prompt", P.prompt);
  assign("input", P.input);
  assign("gutter", P.gutter);
  assign("title", P.title);
  assign("help_cmd", P.help_cmd);
  assign("help_arg", P.help_arg);
  assign("help_text", P.help_text);
  assign("syntax_keyword", P.syntax_keyword);
  assign("syntax_control", P.syntax_control);
  assign("syntax_declaration", P.syntax_declaration);
  assign("syntax_type", P.syntax_type);
  assign("syntax_builtin", P.syntax_builtin);
  assign("syntax_function", P.syntax_function);
  assign("syntax_string", P.syntax_string);
  assign("syntax_number", P.syntax_number);
  assign("syntax_literal", P.syntax_literal);
  assign("syntax_comment", P.syntax_comment);
  assign("syntax_operator", P.syntax_operator);
  assign("syntax_preprocessor", P.syntax_preprocessor);
  assign("syntax_attribute", P.syntax_attribute);
  assign("syntax_key", P.syntax_key);
  assign("syntax_tag", P.syntax_tag);
  assign("syntax_selector", P.syntax_selector);
  assign("syntax_variable", P.syntax_variable);
  assign("syntax_punctuation", P.syntax_punctuation);

  lua_close(LT);
  outP = P;
  return true;
}
