static bool use_color(){
    return is_tty_stdout();
}
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

static ThemePalette palette_for(Theme t){
    if(!use_color()) return {"","","","","", "", "", "", "", "", "", ""};

    switch(t){
        case Theme::Dark:
            return {
                C_CYAN,
                C_GREEN,
                C_YEL,
                C_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_CYAN,
                C_BRIGHT_WHITE,
                C_BRIGHT_BLACK,
                C_BOLD + C_CYAN,
                C_BRIGHT_CYAN,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        case Theme::Neon:
            return {
                C_BRIGHT_MAGENTA,
                C_BRIGHT_GREEN,
                C_BRIGHT_YEL,
                C_BRIGHT_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_MAGENTA,
                C_BRIGHT_CYAN,
                C_BRIGHT_BLACK,
                C_BOLD + C_BRIGHT_MAGENTA,
                C_BRIGHT_MAGENTA,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        case Theme::Matrix:
            return {
                C_GREEN,
                C_BRIGHT_GREEN,
                C_YEL,
                C_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_GREEN,
                C_BRIGHT_GREEN,
                C_BRIGHT_BLACK,
                C_BOLD + C_GREEN,
                C_BRIGHT_GREEN,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        case Theme::Paper:
            return {
                C_BRIGHT_BLACK,
                C_GREEN,
                C_YEL,
                C_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BOLD + C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        case Theme::Yellow:
            return {
                C_BRIGHT_YEL,
                C_BRIGHT_GREEN,
                C_YEL,
                C_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_YEL,
                C_BRIGHT_WHITE,
                C_BRIGHT_BLACK,
                C_BOLD + C_BRIGHT_YEL,
                C_BRIGHT_YEL,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        case Theme::Iceberg:
            return {
                C_BRIGHT_CYAN,
                C_BRIGHT_GREEN,
                C_YEL,
                C_RED,
                C_BRIGHT_BLACK,
                C_BRIGHT_CYAN,
                C_BRIGHT_WHITE,
                C_BRIGHT_BLACK,
                C_BOLD + C_BRIGHT_CYAN,
                C_BRIGHT_CYAN,
                C_BRIGHT_BLACK,
                C_BRIGHT_BLACK
            };
        default:
            return {
                C_CYAN,
                C_GREEN,
                C_YEL,
                C_RED,
                C_DIM,
                C_BRIGHT_CYAN,
                C_BRIGHT_WHITE,
                C_BRIGHT_BLACK,
                C_BOLD + C_CYAN,
                C_CYAN,
                C_DIM,
                C_DIM
            };
    }
}




static bool load_theme_from_lua_file(const string& name, ThemePalette& outP){
    string dir = tedit_themes_dir();
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path p = fs::path(dir) / (name + ".lua");

    if(!fs::exists(p, ec) || !fs::is_regular_file(p, ec)){
        return false;
    }

    lua_State* LT = luaL_newstate();
    if(!LT){
        cout<<C_RED<<"theme: failed to initialize lua state"<<C_RESET<<"\n";
        return false;
    }
    luaL_openlibs(LT);

    

    lua_register(LT, "tedit_command", l_tedit_command);
    lua_register(LT, "tedit_echo",    l_tedit_echo);
    lua_register(LT, "tedit_print",   l_tedit_print);

    int rc = luaL_loadfile(LT, p.string().c_str());
    if(rc != LUA_OK){
        const char* msg = lua_tostring(LT, -1);
        cout<<C_RED<<"theme: lua load error ("<<p.string()<<"): "<<(msg?msg:"")<<C_RESET<<"\n";
        lua_pop(LT, 1);
        lua_close(LT);
        return false;
    }

    rc = lua_pcall(LT, 0, LUA_MULTRET, 0);
    if(rc != LUA_OK){
        const char* msg = lua_tostring(LT, -1);
        cout<<C_RED<<"theme: lua runtime error ("<<p.string()<<"): "<<(msg?msg:"")<<C_RESET<<"\n";
        lua_pop(LT, 1);
        lua_close(LT);
        return false;
    }

    if(lua_gettop(LT) == 0){
        lua_getglobal(LT, "theme");
    }
    if(!lua_istable(LT, -1)){
        cout<<C_RED<<"theme: lua file did not return a theme table"<<C_RESET<<"\n";
        lua_close(LT);
        return false;
    }

    ThemePalette base = palette_for(Theme::Default);
    ThemePalette P = base;

    auto assign = [&](const char* key, string& dst){
        lua_getfield(LT, -1, key);
        if(lua_isstring(LT, -1)){
            const char* v = lua_tostring(LT, -1);
            if(v) dst = v;
        }
        lua_pop(LT, 1);
    };

    assign("accent",     P.accent);
    assign("ok",         P.ok);
    assign("warn",       P.warn);
    assign("err",        P.err);
    assign("dim",        P.dim);
    assign("prompt",     P.prompt);
    assign("input",      P.input);
    assign("gutter",     P.gutter);
    assign("title",      P.title);
    assign("help_cmd",   P.help_cmd);
    assign("help_arg",   P.help_arg);
    assign("help_text",  P.help_text);

    lua_close(LT);
    outP = P;
    return true;
}
