#ifndef TEDIT_EDITOR_HPP
#define TEDIT_EDITOR_HPP

#include "config.hpp"
#include "buffer.hpp"
#include "theme.hpp"
#include "linereader.hpp"
#include "highlight.hpp"

struct Editor{
    Buffer buf;
    Stack undo, redo;
    LineReader lr;

    Theme theme = Theme::Default;
    ThemePalette P = palette_for(theme);

    string active_lua_theme;

    vector<Buffer> others;
    string last_search;
    bool last_icase=false;
    size_t last_index=0;
    int autosave_sec = 120;
    std::chrono::steady_clock::time_point last_autosave = std::chrono::steady_clock::now();
    std::map<string,string> aliases;

    bool wrap_long = true;
    bool truncate_long = false;

    Lang lang = Lang::Plain;

    lua_State* L = nullptr;
    vector<string> plugin_names;
    std::map<string,string> plugin_files;
    string current_plugin;

    Editor();
    ~Editor();

    string cfg_path() const;
    static string theme_name(Theme t);
    static bool theme_from_name(const string& s, Theme& out);
    static bool parse_bool_string(const string& v, bool& out);
    static string esc(const string& in);
    static string unesc(const string& in);

    void init_lua();
    void close_lua();
    void load_lua_plugins();
    static bool scan_plugin_text(const string& content, vector<string>& hits);
    bool run_plugin_file(const string& display_name, const string& path);
    void list_lua_themes();
    void save_config();
    void load_config();
    void confetti();
    void tip();
    void banner();
    string prompt_str() const;
    void status();
    void help();
    void load(const string& p);
    bool run_hook(const char* name);
    bool save(const string& maybe);
    void push_undo();
    void append_mode();
    void insert_mode(size_t before);
    int gutter_width() const;
    void print_line(size_t i);
    void print(size_t lo, size_t hi);
    void repl(bool global, const string& old, const string& nw);
    void info();
    void next_match(bool reverse);
    void cycle_theme(const string& name);
    void open_new_buffer(const string& path);
    void list_buffers();
    void bnext();
    void bprev();
    void show_diff();
    void clear_screen();
    static string expand_path(const string& in);
    bool handle(const string& raw);
};

extern Editor* g_editor;

#endif // TEDIT_EDITOR_HPP
