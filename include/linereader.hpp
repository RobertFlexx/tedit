#ifndef TEDIT_LINEREADER_HPP
#define TEDIT_LINEREADER_HPP

#include "config.hpp"
#include "theme.hpp"

struct LineReader{
    vector<string> history;
    size_t HIST_MAX=800;
    vector<string> commands;
    string color_input = "";
    string color_reset = C_RESET;

    void set_theme_colors(const ThemePalette& P);
    static vector<string> split_words(const string& s);
    static string expand_home_in_token(string in);
    static vector<string> complete_dirs_only(const string& token);
    static vector<string> complete_fs(const string& token);
    vector<string> complete(const string& buf);
    void remember(const string& s);
    string read(const string& prompt);
};

#endif // TEDIT_LINEREADER_HPP
