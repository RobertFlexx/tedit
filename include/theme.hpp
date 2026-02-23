#ifndef TEDIT_THEME_HPP
#define TEDIT_THEME_HPP

#include "config.hpp"

bool use_color();

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

enum class Theme { Default, Dark, Neon, Matrix, Paper, Yellow, Iceberg };

struct ThemePalette {
    string accent, ok, warn, err, dim;
    string prompt, input, gutter, title;
    string help_cmd, help_arg, help_text;
};

ThemePalette palette_for(Theme t);
bool load_theme_from_lua_file(const string& name, ThemePalette& outP);

#endif // TEDIT_THEME_HPP
