#ifndef TEDIT_UTILS_HPP
#define TEDIT_UTILS_HPP

#include "config.hpp"

bool is_tty_stdout();
bool safe_backup_copy(const string &src, const string &dst, string &err);
string sh_escape(const string &s);
int run_shell_cmd(const string &cmd);
string home_path();
bool file_exists(const string& p);
string tedit_config_dir();
string tedit_plugins_dir();
string tedit_themes_dir();
string trim_copy(const string& s);
void rstrip_newline(string& s);
string lower(string s);
bool parse_long(const string& s, long& out);
int digits_for(size_t n);

#endif // TEDIT_UTILS_HPP
