struct Editor{
    Buffer buf; Stack undo, redo; LineReader lr;

    Theme theme = Theme::Default;
    ThemePalette P = palette_for(theme);

    
    string active_lua_theme;

    vector<Buffer> others;
    string last_search; bool last_icase=false; size_t last_index=0;
    int autosave_sec = 120;
    std::chrono::steady_clock::time_point last_autosave = std::chrono::steady_clock::now();
    std::map<string,string> aliases;
    vector<string> recent_files;
    vector<string> messages;
    vector<string> trusted_plugins;

    bool wrap_long = true;
    bool truncate_long = false;

    Lang lang = Lang::Plain;

    lua_State* L = nullptr;
    vector<string> plugin_names;
    std::map<string,string> plugin_files;
    string current_plugin;

    Editor(){
        g_editor = this;
        lr.commands = {
            "help","open","info","write","w","wq","saveas","quit","q","print","p","r",
            "append","a","insert","i","edit","delete","d","move","m","join","find","findi","findre","findrei",
            "repl","replg","read","undo","u","redo","set","filter","ls","pwd","number",
            "goto","n","N","new","bnext","bprev","lsb","buffer","close","theme","highlight","alias","diff",
            "cd","clear","version","lua","luafile","run-plugin","plugins","reload-plugins",
            "lua-themes","config","recent","messages","syntax","plugin","w!","q!","quit!","write!"
        };
        lr.set_theme_colors(P);
        init_lua();
    }

    ~Editor(){
        close_lua();
    }

    string cfg_path() const { return tedit_config_dir() + "/.teditrc"; }

    static string theme_name(Theme t){
        switch(t){
            case Theme::Dark:    return "dark";
            case Theme::Neon:    return "neon";
            case Theme::Matrix:  return "matrix";
            case Theme::Paper:   return "paper";
            case Theme::Yellow:  return "yellow";
            case Theme::Iceberg: return "iceberg";
            default:             return "default";
        }
    }
    static bool theme_from_name(const string& s, Theme& out){
        string n=lower(s);
        if(n=="default"){ out = Theme::Default; return true; }
        if(n=="dark"){    out = Theme::Dark;    return true; }
        if(n=="neon"){    out = Theme::Neon;    return true; }
        if(n=="matrix"){  out = Theme::Matrix;  return true; }
        if(n=="paper"){   out = Theme::Paper;   return true; }
        if(n=="yellow"){  out = Theme::Yellow;  return true; }
        if(n=="iceberg"){ out = Theme::Iceberg; return true; }
        return false;
    }
    static bool parse_bool_string(const string& v, bool& out){
        string s=lower(trim_copy(v));
        if(s=="1"||s=="on"||s=="true"||s=="yes"){ out=true; return true; }
        if(s=="0"||s=="off"||s=="false"||s=="no"){ out=false; return true; }
        return false;
    }
    static string esc(const string& in){ string r; r.reserve(in.size()); for(char ch: in){ if(ch=='\\' || ch=='\t') r.push_back('\\'); r.push_back(ch); } return r; }
    static string unesc(const string& in){ string r; r.reserve(in.size()); bool e=false; for(char ch: in){ if(e){ r.push_back(ch); e=false; } else if(ch=='\\') e=true; else r.push_back(ch); } return r; }

    static bool looks_like_range_token(const string& s){
        if(s.empty()) return false;
        bool has_value=false;
        for(char ch: s){
            if(std::isdigit((unsigned char)ch) || ch=='$'){ has_value=true; continue; }
            if(ch=='-') continue;
            return false;
        }
        return has_value;
    }

    string onoff(bool v) const { return v ? "on" : "off"; }
    size_t buffer_count() const { return others.size() + 1; }
    size_t current_buffer_index() const { return 0; }

    void note(const string& s){
        messages.push_back(s);
        if(messages.size() > 80) messages.erase(messages.begin());
    }

    void add_recent(const string& p){
        if(p.empty()) return;
        string e = expand_path(p);
        recent_files.erase(std::remove(recent_files.begin(), recent_files.end(), e), recent_files.end());
        recent_files.insert(recent_files.begin(), e);
        if(recent_files.size() > 20) recent_files.resize(20);
        save_config();
    }

    bool is_trusted_plugin(const string& key) const {
        return std::find(trusted_plugins.begin(), trusted_plugins.end(), key) != trusted_plugins.end();
    }

    void trust_plugin_key(const string& key){
        if(key.empty()) return;
        if(!is_trusted_plugin(key)) trusted_plugins.push_back(key);
        save_config();
    }

    bool untrust_plugin_key(const string& key){
        auto it = std::find(trusted_plugins.begin(), trusted_plugins.end(), key);
        if(it == trusted_plugins.end()) return false;
        trusted_plugins.erase(it);
        save_config();
        return true;
    }

    string plugin_key_for(const string& raw) const {
        auto it = plugin_files.find(raw);
        if(it != plugin_files.end()) return it->second;
        return expand_path(raw);
    }

    void show_settings(){
        cout<<"settings:\n";
        cout<<"  theme="<<(!active_lua_theme.empty()?active_lua_theme:theme_name(theme))<<"\n";
        cout<<"  highlight="<<onoff(buf.highlight)<<"\n";
        cout<<"  number="<<onoff(buf.number)<<"\n";
        cout<<"  backup="<<onoff(buf.backup)<<"\n";
        cout<<"  autosave="<<autosave_sec<<"\n";
        cout<<"  wrap="<<onoff(wrap_long)<<"\n";
        cout<<"  truncate="<<onoff(truncate_long)<<"\n";
        cout<<"  lang="<<lang_name()<<"\n";
    }

    string lang_name() const {
        switch(lang){
            case Lang::Cpp: return "cpp";
            case Lang::Python: return "python";
            case Lang::Shell: return "shell";
            case Lang::Ruby: return "ruby";
            case Lang::JS: return "js";
            case Lang::HTML: return "html";
            case Lang::CSS: return "css";
            case Lang::JSON: return "json";
            default: return "plain";
        }
    }

    void show_config_paths(){
        cout<<"config: "<<tedit_config_dir()<<"\n";
        cout<<"plugins: "<<tedit_plugins_dir()<<"\n";
        cout<<"themes: "<<tedit_themes_dir()<<"\n";
        cout<<"recovery: "<<tedit_recovery_dir()<<"\n";
        cout<<"rc: "<<cfg_path()<<"\n";
    }

    void show_recent(){
        if(recent_files.empty()){ cout<<"no recent files\n"; return; }
        for(size_t i=0;i<recent_files.size();++i) cout<<i+1<<" "<<recent_files[i]<<"\n";
    }

    void show_messages(){
        if(messages.empty()){ cout<<"no messages\n"; return; }
        for(const auto& m: messages) cout<<m<<"\n";
    }

    void help_topic(const string& topic){
        string raw = trim_copy(topic);
        string t = lower(raw);
        if(t.empty()){ help(); return; }
        if(raw=="N"){
            cout<<"N\nRepeats the previous plain find/findi search and jumps to the previous match.\n";
            return;
        }
        struct HelpEntry { const char* names; const char* usage; const char* text; };
        static const HelpEntry entries[] = {
            {"help h ?", "help [command]", "Shows the full command list, or detailed help for one command. Command names and common aliases both work."},
            {"open", "open <path>", "Loads a file into the current buffer. Paths support ~ expansion. If the current buffer has unsaved changes, save or quit first."},
            {"info", "info", "Shows current file path, dirty state, line count, character count, on-disk size, and file mode when available."},
            {"w write", "write [path] | write <range> <path>", "Saves the current buffer. With a path, saves there and adopts that path. With a range and path, writes only selected lines without changing the current buffer path."},
            {"w! write!", "write! [path]", "Force-saves the current buffer without creating a backup file for that save. Useful when backup files are unwanted for one write."},
            {"wq", "wq", "Saves the current buffer to its current path, then exits if the save succeeds."},
            {"q quit", "quit", "Exits the editor. If the current buffer is dirty, prompts to save, discard, or cancel."},
            {"q! quit!", "quit!", "Exits immediately without saving unsaved changes. Hooks still run on quit."},
            {"saveas", "saveas <path>", "Saves the current buffer to a new path and makes that path the active buffer path."},
            {"p print", "print [range]", "Prints the whole buffer or a range. Ranges use forms like 3, 3-8, 3-, -8, or $."},
            {"r", "r <n>", "Prints exactly one line by number."},
            {"a append", "append", "Starts append mode. Enter lines to append to the end of the buffer; a single dot ends input."},
            {"i insert", "insert <n>", "Starts insert mode before line n. Enter lines to insert; a single dot ends input."},
            {"edit", "edit <n> [text...]", "Replaces line n. If text is omitted, prompts interactively for the replacement line."},
            {"d delete", "delete [range]", "Deletes a range of lines and records the change for undo."},
            {"m move", "move <from> <to>", "Moves one line to a new zero-based insertion position and records the change for undo."},
            {"join", "join <range>", "Joins all lines in a range into one line separated by spaces."},
            {"find", "find <text>", "Searches for literal text, case-sensitive, and prints every matching line."},
            {"findi", "findi <text>", "Searches for literal text, case-insensitive, and prints every matching line."},
            {"findre", "findre [-i] <regex>", "Searches with a regular expression. Use -i for case-insensitive regex matching."},
            {"findrei", "findrei <regex>", "Shortcut for case-insensitive regular expression search."},
            {"n", "n", "Repeats the previous plain find/findi search and jumps to the next match."},
            {"N", "N", "Repeats the previous plain find/findi search and jumps to the previous match."},
            {"goto", "goto <n>", "Prints line n so you can quickly jump to a location in the file."},
            {"repl", "repl <old> <new>", "Replaces the first occurrence of old with new on each line."},
            {"replg", "replg <old> <new>", "Replaces every occurrence of old with new on each line."},
            {"read", "read <path> [n]", "Reads another file and inserts it after line n. If n is omitted, inserts at the end. Paths support ~ expansion."},
            {"filter", "filter <range> !shell", "Runs a shell command with the selected range on stdin and replaces that range with command output."},
            {"undo u", "undo [count]", "Reverts the most recent edit, or count edits. Undo stores line snapshots."},
            {"redo", "redo", "Reapplies one change that was undone."},
            {"set", "set [name value]", "Without arguments, lists settings. Supports number, backup, autosave, wrap, truncate, and lang."},
            {"number", "number", "Toggles line numbers and saves the setting."},
            {"highlight", "highlight on|off", "Turns syntax highlighting on or off for the active buffer and saves the setting."},
            {"syntax", "syntax <name>", "Alias for set lang <name>. Useful values include cpp, python, shell, ruby, js, html, css, json, and plain."},
            {"theme", "theme <name> | theme preview", "Applies a built-in or Lua theme. theme preview prints samples for built-in themes."},
            {"alias", "alias <from> <to...>", "Creates a command alias stored in config. The alias replaces the first command word before dispatch."},
            {"new", "new [path]", "Pushes the current buffer into the buffer list and opens a new empty or file-backed buffer."},
            {"bnext", "bnext", "Cycles to the next buffer."},
            {"bprev", "bprev", "Cycles to the previous buffer."},
            {"lsb", "lsb", "Lists the active buffer as 0 and background buffers as 1..n. Dirty buffers are marked with an asterisk."},
            {"buffer buffers", "buffer <n>", "Switches directly to buffer n from lsb output. Buffer 0 is the current buffer."},
            {"close", "close", "Closes the current buffer if it is clean. If other buffers exist, switches to one of them; otherwise starts a new unnamed buffer."},
            {"config", "config", "Prints the config, plugin, theme, recovery, and rc file paths used by tedit."},
            {"recent", "recent", "Lists recent files opened or saved in this session and persisted in config."},
            {"messages", "messages", "Shows recent editor events such as opens and saves."},
            {"diff", "diff", "Shows a unified diff between the current buffer and the on-disk file."},
            {"ls", "ls [-l] [-a] [path]", "Lists files from inside the editor. Use -a for hidden files and -l for size and permissions."},
            {"pwd", "pwd", "Prints the editor process current working directory."},
            {"cd", "cd <dir>", "Changes the editor process working directory. Supports ., .., and ~ paths."},
            {"clear", "clear", "Clears the terminal screen and scrollback using ANSI escape sequences."},
            {"version ver", "version", "Prints the tedit version compiled into this binary."},
            {"lua", "lua <code>", "Runs inline Lua code in tedit's Lua state. Lua code is trusted code and runs with your user permissions."},
            {"luafile", "luafile <path>", "Runs a Lua script file. Paths support ~ expansion. Script code is trusted code."},
            {"run-plugin", ":run-plugin <name|path>", "Runs a Lua plugin by configured plugin name or file path. The leading colon is required for safety."},
            {"plugin", "plugin trust|untrust|trusted [name|path]", "Manages trusted plugin warning sources. Trusting suppresses heuristic warnings for that plugin path or name."},
            {"plugins", "plugins", "Lists Lua plugin files found under ~/tedit-config/plugins and marks the current plugin when applicable."},
            {"reload-plugins", "reload-plugins", "Rescans ~/tedit-config/plugins for Lua plugin files without restarting tedit."},
            {"lua-themes", "lua-themes", "Lists Lua theme files found under ~/tedit-config/themes and marks the active Lua theme."}
        };
        for(const auto& e: entries){
            std::istringstream names(e.names);
            string name;
            while(names>>name){
                if(t==lower(name)){
                    cout<<e.usage<<"\n"<<e.text<<"\n";
                    return;
                }
            }
        }
        cout<<"no focused help for "<<topic<<"; type help for all commands\n";
    }

    void init_lua(){
        if(L) return;
        L = luaL_newstate();
        if(!L){
            cout<<C_RED<<"lua: failed to initialize state"<<C_RESET<<"\n";
            return;
        }
        luaL_openlibs(L);
        lua_register(L, "tedit_command", l_tedit_command);
        lua_register(L, "tedit_echo",    l_tedit_echo);
        lua_register(L, "tedit_print",   l_tedit_print);
        load_lua_plugins();
    }

    void close_lua(){
        if(L){
            lua_close(L);
            L = nullptr;
        }
    }

    void load_lua_plugins(){
        plugin_names.clear();
        plugin_files.clear();
        if(!L) return;
        string dir = tedit_plugins_dir();
        std::error_code ec;
        if(!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;
        fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
        for(; !ec && it!=end; it.increment(ec)){
            const auto& entry = *it;
            if(!entry.is_regular_file()) continue;
            fs::path p = entry.path();
            if(p.extension() != ".lua") continue;
            string fpath = p.string();
            string name  = p.stem().string();
            plugin_names.push_back(name);
            plugin_files[name] = fpath;
        }
    }

    static bool scan_plugin_text(const string& content, vector<string>& hits){
        static const char* patterns[] = {
            "os.execute", "io.popen", "dofile", "loadfile",
            "package.loadlib", "debug.", "os.remove", "os.rename"
        };
        hits.clear();
        for(const char* pat : patterns){
            if(content.find(pat) != string::npos){
                hits.emplace_back(pat);
            }
        }
        return !hits.empty();
    }

    bool run_plugin_file(const string& display_name, const string& path){
        if(!L){
            cout<<P.err<<"run-plugin: lua not available"<<C_RESET<<"\n";
            return false;
        }

        std::ifstream in(path);
        if(!in.good()){
            cout<<P.err<<"run-plugin: cannot open "<<path<<C_RESET<<"\n";
            return false;
        }
        std::ostringstream buf;
        buf<<in.rdbuf();
        string content = buf.str();

        vector<string> hits;
        bool trusted = is_trusted_plugin(display_name) || is_trusted_plugin(path);
        if(scan_plugin_text(content, hits) && !trusted){
            cout<<P.warn<<"This plugin looks potentially malicious (uses: ";
            for(size_t i=0;i<hits.size();++i){
                if(i) cout<<", ";
                cout<<hits[i];
            }
            cout<<")."<<C_RESET<<"\n";
            cout<<P.warn<<"This plugin is potentially malicious. Do you really want to run this? [y/N] "<<C_RESET<<std::flush;
            char c = 0;
            std::cin.get(c);
            string dump;
            std::getline(std::cin, dump);
            if(c!='y' && c!='Y'){
                cout<<P.dim<<"run-plugin: aborted by user"<<C_RESET<<"\n";
                return false;
            }
        }

        int rc = luaL_loadfile(L, path.c_str());
        if(rc != LUA_OK){
            const char* msg = lua_tostring(L, -1);
            cout<<P.err<<"run-plugin: load error: "<<(msg?msg:"")<<C_RESET<<"\n";
            lua_pop(L, 1);
            return false;
        }
        rc = lua_pcall(L, 0, 0, 0);
        if(rc != LUA_OK){
            const char* msg = lua_tostring(L, -1);
            cout<<P.err<<"run-plugin: runtime error: "<<(msg?msg:"")<<C_RESET<<"\n";
            lua_pop(L, 1);
            return false;
        }

        current_plugin = display_name;
        cout<<P.ok<<"run-plugin: loaded "<<display_name<<C_RESET<<"\n";
        return true;
    }

    
    void list_lua_themes(){
        string dir = tedit_themes_dir();
        std::error_code ec;
        if(!fs::exists(dir, ec) || !fs::is_directory(dir, ec)){
            cout<<"no lua themes directory ("<<dir<<")\n";
            return;
        }
        vector<string> names;
        fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
        for(; !ec && it!=end; it.increment(ec)){
            const auto& e = *it;
            if(!e.is_regular_file()) continue;
            fs::path p = e.path();
            if(p.extension() == ".lua") names.push_back(p.stem().string());
        }
        std::sort(names.begin(), names.end());
        if(names.empty()){
            cout<<"no lua themes found\n";
            return;
        }
        cout<<"lua themes:\n";
        for(const auto& n : names){
            bool is_current = !active_lua_theme.empty() && lower(n) == lower(active_lua_theme);
            cout<<"- "<<n<<(is_current?" *":"")<<"\n";
        }
    }

    void save_config(){
        std::ofstream out(cfg_path(), std::ios::binary|std::ios::trunc);
        if(!out.good()) return;
        
        if(!active_lua_theme.empty()){
            out<<"theme="<<active_lua_theme<<"\n";
        } else {
            out<<"theme="<<theme_name(theme)<<"\n";
        }
        out<<"highlight="<<(buf.highlight?"on":"off")<<"\n";
        out<<"number="<<(buf.number?"on":"off")<<"\n";
        out<<"backup="<<(buf.backup?"on":"off")<<"\n";
        out<<"autosave="<<(autosave_sec)<<"\n";
        out<<"wrap="<<(wrap_long?"on":"off")<<"\n";
        out<<"truncate="<<(truncate_long?"on":"off")<<"\n";
        for(auto& kv: aliases) out<<"alias\t"<<esc(kv.first)<<"\t"<<esc(kv.second)<<"\n";
        for(auto& p: recent_files) out<<"recent\t"<<esc(p)<<"\n";
        for(auto& p: trusted_plugins) out<<"trust\t"<<esc(p)<<"\n";
    }
    void load_config(){
        std::ifstream in(cfg_path());
        if(!in.good()) return;
        string line;
        while(std::getline(in,line)){
            if(line.empty()) continue;
            if(line.rfind("alias\t",0)==0){
                size_t p1=line.find('\t',6); if(p1==string::npos) continue;
                size_t p2=line.find('\t',p1+1);
                if(p2==string::npos){
                    string from=unesc(line.substr(p1+1));
                    aliases[from]="";
                }else{
                    string from=unesc(line.substr(p1+1, p2-(p1+1)));
                    string to  =unesc(line.substr(p2+1));
                    aliases[from]=to;
                }
                continue;
            }
            if(line.rfind("recent\t",0)==0){
                string p = unesc(line.substr(7));
                if(!p.empty() && std::find(recent_files.begin(), recent_files.end(), p) == recent_files.end()) recent_files.push_back(p);
                continue;
            }
            if(line.rfind("trust\t",0)==0){
                string key = unesc(line.substr(6));
                if(!is_trusted_plugin(key)) trusted_plugins.push_back(key);
                continue;
            }
            auto eq=line.find('=');
            if(eq==string::npos) continue;
            string key=lower(trim_copy(line.substr(0,eq)));
            string val=trim_copy(line.substr(eq+1));
            if(key=="theme"){
                Theme t;
                if(theme_from_name(val, t)){
                    theme = t;
                    active_lua_theme.clear(); 
                    P=palette_for(theme);
                    lr.set_theme_colors(P);
                } else {
                    
                    ThemePalette luaP;
                    if(load_theme_from_lua_file(val, luaP)){
                        P = luaP;
                        active_lua_theme = val;
                        lr.set_theme_colors(P);
                    }
                }
            }
            else if(key=="highlight"){ bool b; if(parse_bool_string(val,b)) buf.highlight=b; }
            else if(key=="number"){ bool b; if(parse_bool_string(val,b)) buf.number=b; }
            else if(key=="backup"){ bool b; if(parse_bool_string(val,b)) buf.backup=b; }
            else if(key=="autosave"){ long s; if(parse_long(val,s)) autosave_sec=(int)std::max<long>(0,s); }
            else if(key=="wrap"){ bool b; if(parse_bool_string(val,b)) wrap_long=b; }
            else if(key=="truncate"){ bool b; if(parse_bool_string(val,b)) truncate_long=b; }
        }
    }

    void confetti(){
        if(!use_color()) return;
        const char* art[] = {" *  .  *   . *",".  *  *  .    *","   *  .   *  . "};
        cout<<P.accent<<art[0]<<C_RESET<<"\n"<<P.ok<<art[1]<<C_RESET<<"\n"<<P.warn<<art[2]<<C_RESET<<"\n";
    }
    void tip(){
        static const char* tips[] = {
            "Tip: use 'goto <n>' to jump to a line.",
            "Tip: 'n' and 'N' hop through last search results.",
            "Tip: ':filter 1,10 !sed -n \"p\"' pipes lines through a shell.",
            "Tip: 'theme neon' and 'highlight on' for vibes.",
            "Tip: 'alias dd \"delete 1-$\"' to delete all quickly.",
            "Tip: 'diff' shows changes vs on-disk.",
            "Tip: 'Tab': first word = commands only; after 'cd ' => directories only."
        };
        std::mt19937_64 rng((unsigned)time(nullptr));
        cout<<P.dim<<tips[rng()% (sizeof(tips)/sizeof(*tips))]<<C_RESET<<"\n";
    }
    void banner(){
        string bpath = home_path()+"/.tedit_banner";
        if(!file_exists(bpath)) return;
        std::ifstream in(bpath);
        if(!in.good()) return;
        cout<<P.accent;
        string L; while(std::getline(in,L)) cout<<L<<"\n";
        cout<<C_RESET;
    }

    string prompt_str() const {
        string dirty = buf.dirty? "*" : "";
        return (use_color()? P.prompt : string("")) + dirty + "tedit> " + (use_color()? C_RESET: string(""));
    }

    void status(){
        using std::chrono::system_clock;
        auto t = system_clock::to_time_t(system_clock::now());
        char tb[32]; strftime(tb,sizeof(tb),"%H:%M:%S", localtime(&t));
        string tname = theme_name(theme);
        cout<<P.dim<<"["<<current_buffer_index()<<"/"<<(buffer_count()-1)<<" "<< (buf.path.empty()? "(unnamed)": buf.path) << "] "
        <<"lines="<<buf.lines.size()<<" chars="<<char_count(buf)
        <<(buf.dirty?" *":"")
        <<" | "<<tb<<" | theme:"<<tname
        <<" | hl:"<<(buf.highlight?"on":"off")
        <<" | wrap:"<<(wrap_long?"on":"off")
        <<" | plugin:"<<(current_plugin.empty() ? "none" : current_plugin)
        <<C_RESET<<"\n";
    }

    void help(){
        auto CMD = [&](const string& cmd, const string& args, const string& desc){
            std::ostringstream left;
            string label = cmd;
            if(!args.empty()) label += " " + args;
            left<<std::left<<std::setw(40)<<label;
            string reset = use_color()? C_RESET : string();
            cout<<P.help_cmd<<left.str()<<reset
            <<" - "<<P.help_text<<desc<<reset<<"\n";
        };

        cout<<P.title<<"Commands (':' optional, except where noted)"<<C_RESET<<"\n";
        CMD("open <path>",            "", "open file");
        CMD("info",                   "", "buffer + file info");
        CMD("w|write [path]",         "", "save (atomic), optional new path");
        CMD("write [range] <path>",   "", "write selected lines to path");
        CMD("w!|write! <path>",       "", "force save without backup");
        CMD("wq",                     "", "save & quit");
        CMD("q!|quit!",               "", "quit without saving");
        CMD("saveas <path>",          "", "save to path");
        CMD("q|quit",                 "", "quit (prompts if unsaved)");
        CMD("p|print [range]",        "", "print lines");
        CMD("r <n>",                  "", "show one line");
        CMD("a|append",               "", "append lines ('.' ends; use \".\" for a literal)");
        CMD("i|insert <n>",           "", "insert before line n");
        CMD("edit <n> [text...]",     "", "replace line n (prompts if no text)");
        CMD("d|delete [range]",       "", "delete lines");
        CMD("m|move <from> <to>",     "", "move line");
        CMD("join <range>",           "", "join lines with space");
        CMD("/text | find | findi | findre", "", "search (regex via findre)");
        CMD("n | N",                  "", "next/prev match from last search");
        CMD("goto <n>",               "", "jump to line");
        CMD("repl old new | replg old new", "", "replace first/global per line");
        CMD("read <path> [n]",        "", "insert file after n (default=end)");
        CMD("filter <range> !shell",  "", "pipe range through shell and replace (safe temp names)");
        CMD("undo | u [k]",           "", "undo (optionally k steps)");
        CMD("redo",                   "", "redo");
        CMD("set",                    "", "show current settings");
        CMD("set number on|off",      "", "toggle line numbers");
        CMD("set backup on|off",      "", "toggle on-save ~ backup");
        CMD("set autosave <sec>",     "", "autosave interval");
        CMD("set wrap on|off",        "", "soft-wrap long lines under the gutter");
        CMD("set truncate on|off",    "", "truncate line display when wrap=off");
        CMD("set lang <name>",        "", "override syntax (auto by extension)");
        CMD("highlight on|off",       "", "simple syntax highlighting");
        CMD("syntax <name>",          "", "alias for set lang <name>");
        CMD("theme <name>",           "", "default|dark|neon|matrix|paper|yellow|iceberg or lua theme");
        CMD("theme preview",          "", "show built-in theme samples");
        CMD("alias <from> <to...>",   "", "define command alias");
        CMD("new [path]",             "", "open new buffer (push current)");
        CMD("bnext | bprev | lsb",    "", "cycle/list buffers");
        CMD("buffer <n> | close",     "", "switch to or close a buffer");
        CMD("config | recent | messages", "", "show paths, recent files, or message log");
        CMD("diff",                   "", "show diff vs on-disk (safe)");
        CMD("ls [-l] [-a] [path] | pwd","", "filesystem helpers");
        CMD("cd <dir>",               "", "change directory (./ ../ ~/)");
        CMD("clear",                  "", "clear screen and scrollback");
        CMD("version",                "", "show tedit version");
        CMD("lua <code>",             "", "run Lua code");
        CMD("luafile <path>",         "", "run Lua script file");
        CMD("run-plugin <name>",      "", "run Lua plugin (requires :run-plugin)");
        CMD("plugin trust <name|path>", "", "trust a plugin warning source");
        CMD("plugins",                "", "list available Lua plugins");
        CMD("reload-plugins",         "", "rescan Lua plugins from tedit-config/plugins");
        
        CMD("lua-themes",             "", "list available Lua themes");
        cout<<P.dim<<"Tab: first word => commands only; after 'cd ' => directories only."<<C_RESET<<"\n";
    }

    void load(const string& p){
        string path = expand_path(p);
        buf.path=path; load_file(path, buf);
        lang = detect_lang(path);
        add_recent(path);
        note("opened " + path);
        cout<<P.ok<<"opened "<<path<<C_RESET<<"\n";
        (void)maybe_recover(buf);
    }

    bool run_hook(const char* name){
        string h = home_path()+"/.tedit/hooks/";
        h += name;
        if(!file_exists(h)) return true;
        vector<string> args;
        if(!buf.path.empty()) args.push_back(buf.path);
        int rc = run_exec_file(h, args);
        return rc==0;
    }

    bool save(const string& maybe){
        string target = maybe.empty()? buf.path : expand_path(maybe);
        if(target.empty()){
            cout<<P.warn<<"save: no filename (use: write <path>)"<<C_RESET<<"\n"; return false;
        }
        string err;
        bool ok = atomic_save(target, buf, buf.backup, err);
        if(!ok){
            cout<<P.err<<"save: "<<err<<C_RESET<<"\n";
            return false;
        }
        if(target!=buf.path) buf.path=target;
        buf.dirty=false;
        add_recent(target);
        note("saved " + target);
        cout<<P.ok<<"saved to "<<target<<C_RESET<<"\n";
        confetti();
        string rec = recover_path_for(buf);
        unlink(rec.c_str());
        string oldrec = legacy_recover_path_for(buf);
        unlink(oldrec.c_str());
        (void)run_hook("on_save");
        return true;
    }

    void push_undo(){ undo.push(buf); redo.clear(); }

    void append_mode(){
        cout<<"enter text; '.' alone ends (use \".\" for a literal '.')\n";
        string s; size_t added=0;
        while(true){
            cout<<"> "<<std::flush;
            if(!std::getline(std::cin,s)){ cout<<"\n"; break; }
            if(s=="\".\"") s=".";
            else if(s==".") break;
            buf.lines.push_back(s); added++;
        }
        if(added){ buf.dirty=true; cout<<"appended "<<added<<" line(s)\n"; }
    }

    void insert_mode(size_t before){
        cout<<"enter text; '.' alone ends (use \".\" for a literal '.')\n";
        string s; size_t added=0;
        while(true){
            cout<<"> "<<std::flush;
            if(!std::getline(std::cin,s)){ cout<<"\n"; break; }
            if(s=="\".\"") s=".";
            else if(s==".") break;
            if(before + added > buf.lines.size()) {
                buf.lines.push_back(s);
            } else {
                buf.lines.insert(buf.lines.begin()+ (long)before + (long)added, s);
            }
            added++;
        }
        if(added){ buf.dirty=true; cout<<"inserted "<<added<<" line(s)\n"; }
    }

    int gutter_width() const {
        if(!buf.number) return 0;
        int w = digits_for(buf.lines.size()==0?1:buf.lines.size());
        return w + 3;
    }

    void print_line(size_t i){
        const int termw = term_width();
        const int gw = gutter_width();
        const int avail = std::max(10, termw - gw);

        std::ostringstream first, cont;
        if(buf.number){
            first<<P.gutter<<std::setw(gw-3)<<i<<" | "<<C_RESET;
            cont <<P.gutter<<std::string(gw-3, ' ')<<" | "<<C_RESET;
        }

        string colored = colorize_lang(buf.lines[i-1], buf, P, lang);

        if(wrap_long){
            print_wrapped_with_gutter(colored, first.str(), cont.str(), avail);
        } else {
            if(truncate_long){
                int col=0; bool esc=false;
                cout<<first.str();
                for(size_t k=0;k<colored.size();++k){
                    char ch=colored[k];
                    if(!esc){
                        if(ch=='\033'){ esc=true; cout<<ch; continue; }
                        if(col>=avail-3){ cout<<"..."; break; }
                        cout<<ch; col++;
                    }else{
                        cout<<ch; if(ch=='m') esc=false;
                    }
                }
                cout<<C_RESET<<"\n";
            }else{
                cout<<first.str()<<colored<<C_RESET<<"\n";
            }
        }
    }

    void print(size_t lo, size_t hi){
        if(hi==0){ cout<<"(empty)\n"; return; }
        for(size_t i=lo;i<=hi;i++) print_line(i);
    }

    void repl(bool global, const string& old, const string& nw){
        if(old.empty()){ cout<<P.warn<<"usage: repl[g] <old> <new>"<<C_RESET<<"\n"; return; }
        push_undo(); int total=0;
        for(string& L: buf.lines){
            string out; int c = global? replace_all_line(L,old,nw,out): replace_first_line(L,old,nw,out);
            if(c){ L.swap(out); total+=c; }
        }
        if(total){ buf.dirty=true; cout<<"replaced "<<total<<" occurrence"<<(total==1?"":"s")<<(global?" (global)":" (first per line)")<<"\n"; }
        else { cout<<"no occurrences\n"; }
    }

    void info(){
        struct stat st{}; bool have = (!buf.path.empty() && ::stat(buf.path.c_str(), &st)==0);
        cout<<"file: "<<(buf.path.empty()? "(unnamed)": buf.path)<<(buf.dirty?" *":"")<<"\n";
        cout<<"  lines: "<<buf.lines.size()<<", chars: "<<char_count(buf)<<"\n";
        if(have){ cout<<"  size: "<<(long long)st.st_size<<" bytes, mode: "<<std::oct<< (st.st_mode & 0777) << std::dec <<"\n"; }
        else cout<<"  on-disk: (none)\n";
    }

    void next_match(bool reverse){
        if(last_search.empty()){ cout<<"(no previous search)\n"; return; }
        vector<size_t> hits;
        search_plain_allhits(buf,last_search,last_icase,hits);
        if(hits.empty()){ cout<<"no matches\n"; return; }
        if(!reverse){
            auto it = std::upper_bound(hits.begin(), hits.end(), last_index);
            if(it==hits.end()) it=hits.begin();
            last_index = *it;
        }else{
            auto it = std::lower_bound(hits.begin(), hits.end(), last_index);
            if(it==hits.begin()) it=hits.end();
            --it; last_index = *it;
        }
        print(last_index,last_index);
    }

    void cycle_theme(const string& name){
        Theme t;
        if(theme_from_name(name, t)){
            theme = t;
            active_lua_theme.clear(); 
            P = palette_for(theme);
            lr.set_theme_colors(P);
            cout<<P.ok<<"theme set"<<C_RESET<<"\n";
            save_config(); 
            return;
        }

        ThemePalette luaP;
        if(load_theme_from_lua_file(name, luaP)){
            P = luaP;
            active_lua_theme = name; 
            lr.set_theme_colors(P);
            cout<<P.ok<<"theme set (lua: "<<name<<")"<<C_RESET<<"\n";
            save_config(); 
            return;
        }

        cout<<P.err<<"Theme not found"<<C_RESET<<"\n";
    }

    void open_new_buffer(const string& path){
        others.push_back(buf);
        Buffer nb;
        if(!path.empty()){ nb.path=path; load_file(path, nb); maybe_recover(nb); }
        buf = std::move(nb);
        lang = detect_lang(buf.path);
        add_recent(buf.path);
        cout<<P.ok<<"(new buffer) "<<(path.empty()? "(unnamed)":path)<<C_RESET<<"\n";
    }
    void add_background_buffer(const string& path){
        string p = expand_path(path);
        Buffer nb;
        nb.path = p;
        load_file(p, nb);
        maybe_recover(nb);
        others.push_back(std::move(nb));
        add_recent(p);
    }
    void list_buffers(){
        cout<<C_BOLD<<"* 0 "<<(buf.path.empty()?"(unnamed)":buf.path)<<(buf.dirty?" *":"")<<C_RESET<<"\n";
        for(size_t i=0;i<others.size();++i){
            const auto& b=others[i];
            cout<<"  "<<i+1<<" "<<(b.path.empty()?"(unnamed)":b.path)<<(b.dirty?" *":"")<<"\n";
        }
    }
    void bnext(){
        if(others.empty()){ cout<<"(only one buffer)\n"; return; }
        others.insert(others.begin(), buf);
        buf = others.back();
        others.pop_back();
        lang = detect_lang(buf.path);
        cout<<"[bnext] "<<(buf.path.empty()? "(unnamed)":buf.path)<<"\n";
    }
    void bprev(){
        if(others.empty()){ cout<<"(only one buffer)\n"; return; }
        auto prev = others.front();
        others.erase(others.begin());
        others.push_back(buf);
        buf = prev;
        lang = detect_lang(buf.path);
        cout<<"[bprev] "<<(buf.path.empty()? "(unnamed)":buf.path)<<"\n";
    }
    void switch_buffer(size_t idx){
        if(idx == 0){ cout<<"[buffer] "<<(buf.path.empty()?"(unnamed)":buf.path)<<"\n"; return; }
        if(idx > others.size()){ cout<<P.warn<<"buffer: no such buffer"<<C_RESET<<"\n"; return; }
        Buffer cur = buf;
        buf = others[idx-1];
        others[idx-1] = cur;
        lang = detect_lang(buf.path);
        cout<<"[buffer] "<<(buf.path.empty()?"(unnamed)":buf.path)<<"\n";
    }
    bool close_buffer(){
        if(buf.dirty){ cout<<P.warn<<"close: unsaved changes (use q! to discard or save first)"<<C_RESET<<"\n"; return true; }
        if(others.empty()){
            buf = Buffer{};
            lang = Lang::Plain;
            cout<<"[close] new unnamed buffer\n";
            return true;
        }
        buf = others.back();
        others.pop_back();
        lang = detect_lang(buf.path);
        cout<<"[close] "<<(buf.path.empty()?"(unnamed)":buf.path)<<"\n";
        return true;
    }
    void theme_preview(){
        Theme themes[] = {Theme::Default, Theme::Dark, Theme::Neon, Theme::Matrix, Theme::Paper, Theme::Yellow, Theme::Iceberg};
        for(Theme t: themes){
            ThemePalette q = palette_for(t);
            cout<<q.title<<theme_name(t)<<C_RESET<<" "<<q.accent<<"accent"<<C_RESET<<" "<<q.ok<<"ok"<<C_RESET<<" "<<q.warn<<"warn"<<C_RESET<<" "<<q.err<<"err"<<C_RESET<<"\n";
        }
    }
    void show_diff(){
        if(buf.path.empty() || !file_exists(buf.path)){ cout<<"diff: no on-disk version\n"; return; }
        char tpat[]="/tmp/tedit_diff_XXXXXX";
        int tfd = mkstemp(tpat); if(tfd<0){ cout<<"diff: mkstemp failed\n"; return; }
        FILE* tf = fdopen(tfd,"w"); if(!tf){ close(tfd); unlink(tpat); cout<<"diff: fdopen failed\n"; return; }
        string err;
        if(!atomic_save_to_fd(tf,buf,err)){ unlink(tpat); cout<<"diff: "<<err<<"\n"; return; }

        string inner = "diff -u -- " + sh_escape(buf.path) + " " + sh_escape(tpat) + " || true";
        string cmd   = "sh -c " + sh_escape(inner);
        run_shell_cmd(cmd);
        unlink(tpat);
    }

    void clear_screen(){
        cout<<"\033[3J\033[H\033[2J"<<std::flush;
    }

    static string expand_path(const string& in){
        if(in.empty()) return in;
        if(in=="~") return home_path();
        if(in.size()>=2 && in[0]=='~' && in[1]=='/') return home_path()+in.substr(1);
        return in;
    }

    bool handle(const string& raw){
        autosave_if_needed(buf, last_autosave, autosave_sec);

        string in = trim_copy(raw);
        if(in.empty()) return true;
        bool had_colon = false;
        if(in[0]==':'){
            had_colon = true;
            in = trim_copy(in.substr(1));
            if(in.empty()) return true;
        }

        {
            std::istringstream ss(in); string tok; ss>>tok;
            auto it = aliases.find(tok);
            if(it!=aliases.end()){
                string rest; std::getline(ss,rest);
                in = it->second + rest;
            }
        }

        if(!in.empty() && in[0]=='/'){
            string q=in.substr(1); last_search=q; last_icase=false; last_index=0;
            search_plain(buf,q,false); return true;
        }

        std::istringstream ss(in); string cmd; ss>>cmd; string rest; std::getline(ss,rest); rest=trim_copy(rest);
        string lc = lower(cmd);

        if(lc=="help"||lc=="h"||lc=="?") { help_topic(rest); return true; }
        if(lc=="open"){
            if(rest.empty()){ cout<<P.warn<<"usage: open <path>"<<C_RESET<<"\n"; return true; }
            if(!buf.path.empty() && buf.dirty){ cout<<P.warn<<"Unsaved changes. Use wq or quit."<<C_RESET<<"\n"; return true; }
            load(rest); return true;
        }
        if(lc=="info"){ info(); return true; }
        if(lc=="wq"){ if(save("")){ cout<<P.dim<<"bye!"<<C_RESET<<"\n"; (void)run_hook("on_quit"); return false; } return true; }
        if(lc=="q!"||lc=="quit!"){ cout<<P.dim<<"bye!"<<C_RESET<<"\n"; (void)run_hook("on_quit"); return false; }
        if(lc=="w!"||lc=="write!"){
            string target = rest.empty()? buf.path : expand_path(rest);
            if(target.empty()){ cout<<P.warn<<"save: no filename (use: write! <path>)"<<C_RESET<<"\n"; return true; }
            string err;
            if(atomic_save(target, buf, false, err)){
                if(target!=buf.path) buf.path=target;
                buf.dirty=false;
                add_recent(target);
                note("force-saved " + target);
                cout<<P.ok<<"saved to "<<target<<C_RESET<<"\n";
            }else cout<<P.err<<"save: "<<err<<C_RESET<<"\n";
            return true;
        }
        if(lc=="w"){ save(rest); return true; }
        if(lc=="write"){
            std::istringstream ts(rest); string tok1; ts>>tok1;
            string tok2; ts>>tok2;
            if(tok2.empty() || !looks_like_range_token(tok1)){ save(rest); return true; }
        }
        if(lc=="saveas"){ if(rest.empty()){ cout<<P.warn<<"usage: saveas <path>"<<C_RESET<<"\n"; return true; } save(rest); return true; }

        if(lc=="quit"||lc=="q"){
            if(buf.dirty){
                cout<<P.warn<<"Save changes to file? [y]es/[n]o/[c]ancel "<<C_RESET<<std::flush;
                char c=0; std::cin.get(c); string dump; std::getline(std::cin,dump);
                if(c=='y'||c=='Y'){ if(!save("")) return true; }
                else if(c=='c'||c=='C') return true;
            }
            cout<<P.dim<<"bye!"<<C_RESET<<"\n"; (void)run_hook("on_quit"); return false;
        }

        if(lc=="print"||lc=="p"){
            size_t lo=1,hi=buf.lines.size();
            if(!parse_range(rest,buf.lines.size(),lo,hi)){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
            print(lo,hi); return true;
        }
        if(lc=="r"){
            long n=0; if(!parse_long(rest,n)){ cout<<P.warn<<"usage: r <n>"<<C_RESET<<"\n"; return true; }
            if(n<1 || (size_t)n>buf.lines.size()){ cout<<P.warn<<"no such line"<<C_RESET<<"\n"; return true; } print((size_t)n,(size_t)n); return true;
        }
        if(lc=="goto"){
            long n=0; if(!parse_long(rest,n)){ cout<<P.warn<<"usage: goto <n>"<<C_RESET<<"\n"; return true; }
            if(n<1 || (size_t)n>buf.lines.size()){ cout<<P.warn<<"no such line"<<C_RESET<<"\n"; return true; }
            print((size_t)n,(size_t)n); return true;
        }

        if(lc=="append"||lc=="a"){ push_undo(); append_mode(); return true; }
        if(lc=="insert"||lc=="i"){
            long n=0; if(!parse_long(rest,n)){ cout<<P.warn<<"usage: insert <n>"<<C_RESET<<"\n"; return true; }
            if(n<1 || (size_t)n>buf.lines.size()+1){ cout<<P.warn<<"invalid target line"<<C_RESET<<"\n"; return true; }
            push_undo(); insert_mode((size_t)n-1); buf.dirty=true; return true;
        }

        if(lc=="edit"){
            std::istringstream ts(rest);
            string ntok; ts>>ntok;
            long n=0;
            if(ntok.empty() || !parse_long(ntok, n)){
                cout<<P.warn<<"usage: edit <n> [text...]"<<C_RESET<<"\n";
                return true;
            }
            if(n<1 || (size_t)n>buf.lines.size()){
                cout<<P.warn<<"no such line"<<C_RESET<<"\n";
                return true;
            }

            string after;
            std::getline(ts, after);

            if(after.empty()){
                cout<<"old: "<<buf.lines[(size_t)n-1]<<"\n";
                cout<<"new> "<<std::flush;
                string newline;
                if(!std::getline(std::cin, newline)){
                    cout<<"\n";
                    return true;
                }
                push_undo();
                buf.lines[(size_t)n-1] = newline;
                buf.dirty = true;
                cout<<"edited line "<<n<<"\n";
                return true;
            }

            
            if(!after.empty() && (after[0]==' ' || after[0]=='\t')) after.erase(after.begin());

            push_undo();
            buf.lines[(size_t)n-1] = after;
            buf.dirty = true;
            cout<<"edited line "<<n<<"\n";
            return true;
        }

        if(lc=="delete"||lc=="d"){
            if(buf.lines.empty()){ cout<<"(empty)\n"; return true; }
            size_t lo=1,hi=buf.lines.size();
            if(!parse_range(rest,buf.lines.size(),lo,hi)){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
            push_undo();
            size_t count=hi-lo+1;
            buf.lines.erase(buf.lines.begin()+ (long)lo-1,
                            buf.lines.begin()+ (long)lo-1 + (long)count);
            buf.dirty=true;
            cout<<"deleted "<<count<<" line(s)\n";
            return true;
        }

        if(lc=="move"||lc=="m"){
            std::istringstream ts(rest); long from=0,to=0; ts>>from>>to;
            if(!from && !to){ cout<<P.warn<<"usage: move <from> <to>"<<C_RESET<<"\n"; return true; }
            if(from<1 || (size_t)from>buf.lines.size() || to<0 || (size_t)to>buf.lines.size()){
                cout<<P.warn<<"bad indexes"<<C_RESET<<"\n"; return true;
            }
            push_undo();
            string s=buf.lines[(size_t)from-1];
            buf.lines.erase(buf.lines.begin()+ (long)from-1);
            if(to>from) to--;
            if(to>(long)buf.lines.size()) to=(long)buf.lines.size();
            buf.lines.insert(buf.lines.begin()+ (long)to, s);
            buf.dirty=true;
            cout<<"moved line "<<from<<" to "<<to<<"\n";
            return true;
        }

        if(lc=="join"){
            size_t lo=1,hi=buf.lines.size();
            if(!parse_range(rest,buf.lines.size(),lo,hi)||hi<=lo){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
            push_undo();
            std::ostringstream out;
            for(size_t i=lo;i<=hi;i++){ if(i>lo) out<<" "; out<<buf.lines[i-1]; }
            buf.lines.erase(buf.lines.begin()+ (long)lo-1,
                            buf.lines.begin()+ (long)hi);
            buf.lines.insert(buf.lines.begin()+ (long)lo-1, out.str());
            buf.dirty=true;
            cout<<"joined\n";
            return true;
        }

        if(lc=="find"){ if(rest.empty()){ cout<<P.warn<<"usage: find <text>"<<C_RESET<<"\n"; return true; } last_search=rest; last_icase=false; last_index=0; search_plain(buf,rest,false); return true; }
        if(lc=="findi"){ if(rest.empty()){ cout<<P.warn<<"usage: findi <text>"<<C_RESET<<"\n"; return true; } last_search=rest; last_icase=true;  last_index=0; search_plain(buf,rest,true);  return true; }
        if(lc=="findre"){
            bool icase=false;
            string pat=rest;
            if(pat.rfind("-i ",0)==0){ icase=true; pat=trim_copy(pat.substr(3)); }
            if(pat.empty()){ cout<<P.warn<<"usage: findre [-i] <regex>"<<C_RESET<<"\n"; return true; }
            search_regex(buf,pat,icase); return true;
        }
        if(lc=="findrei"){ if(rest.empty()){ cout<<P.warn<<"usage: findrei <regex>"<<C_RESET<<"\n"; return true; } search_regex(buf,rest,true); return true; }
        if(cmd=="N"){ next_match(true);  return true; }
        if(lc=="n"){ next_match(false); return true; }

        if(lc=="repl"||lc=="replg"){ bool g=(lc=="replg"); std::istringstream ts(rest); string old,nw; ts>>old>>nw; repl(g,old,nw); return true; }

        if(lc=="read"){
            std::istringstream ts(rest); string p; long n=-1; ts>>p;
            if(p.empty()){ cout<<P.warn<<"usage: read <path> [n]"<<C_RESET<<"\n"; return true; }
            p = expand_path(p);
            if(!(ts>>n)) n=-1;
            std::ifstream in2(p);
            if(!in2.good()){ cout<<P.err<<"read: cannot open"<<C_RESET<<"\n"; return true; }
            push_undo();
            vector<string> R; string L;
            while(std::getline(in2,L)){ rstrip_newline(L); R.push_back(L); }
            size_t at = (n<0)? buf.lines.size(): (size_t)n;
            if(at>buf.lines.size()) at=buf.lines.size();
            buf.lines.insert(buf.lines.begin()+ (long)at, R.begin(), R.end());
            buf.dirty=true;
            cout<<"read "<<R.size()<<" line(s) from "<<p<<"\n";
            return true;
        }

        if(lc=="write"){
            std::istringstream ts(rest); string tok1; ts>>tok1;
            if(tok1.empty()){ cout<<P.warn<<"usage: write [range] <path>"<<C_RESET<<"\n"; return true; }
            size_t lo=1,hi=buf.lines.size(); string outp;
            string maybe_path;
            ts>>maybe_path;
            if(!maybe_path.empty() && looks_like_range_token(tok1)){
                if(!parse_range(tok1,buf.lines.size(),lo,hi)){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
                outp=maybe_path;
            } else {
                save(rest);
                return true;
            }
            outp = expand_path(outp);
            Buffer tmp;
            if(hi>=lo) tmp.lines.assign(buf.lines.begin()+ (long)lo-1, buf.lines.begin()+ (long)hi);
            string err;
            if(atomic_save(outp, tmp, buf.backup, err)){ cout<<"wrote "<<(hi>=lo?hi-lo+1:0)<<" line(s) to "<<outp<<"\n"; }
            else cout<<P.err<<"write: "<<err<<C_RESET<<"\n";
            return true;
        }

        if(lc=="filter"){
            std::istringstream ts(rest); string rng; ts>>rng; string ex; std::getline(ts,ex); ex=trim_copy(ex);
            if(ex.empty()||ex[0]!='!'){ cout<<P.warn<<"usage: filter <range> !shell"<<C_RESET<<"\n"; return true; }
            size_t lo=1,hi=buf.lines.size();
            if(!parse_range(rng,buf.lines.size(),lo,hi)){ cout<<P.warn<<"bad range"<<C_RESET<<"\n"; return true; }
            push_undo();
            string ferr;
            if(run_filter_replace(buf.lines,lo,hi, ex.substr(1), ferr)){ buf.dirty=true; cout<<"filtered\n"; }
            else { cout<<P.err<<"filter failed: "<<ferr<<C_RESET<<"\n"; }
            return true;
        }

        if(lc=="undo"||lc=="u"){
            long k=1; if(!rest.empty()) parse_long(rest,k);
            bool any=false;
            while(k-- > 0){
                Snap s; if(!undo.pop(s)){ if(!any) cout<<"nothing to undo\n"; break; }
                redo.push(buf); buf.lines = std::move(s.lines); buf.dirty=true; any=true;
            }
            if(any) cout<<"undo\n";
            return true;
        }
        if(lc=="redo"){
            Snap s; if(!redo.pop(s)){ cout<<"nothing to redo\n"; return true; }
            undo.push(buf); buf.lines = std::move(s.lines); buf.dirty=true; cout<<"redo\n"; return true;
        }

        if(lc=="set"){
            std::istringstream ts(rest); string what,val; ts>>what>>val; what=lower(what); val=lower(val);
            if(what.empty()){
                show_settings();
            } else if(what=="number"){
                if(val=="on"||val=="1"||val=="true"){ buf.number=true; cout<<"number: on\n"; save_config(); }
                else if(val=="off"||val=="0"||val=="false"){ buf.number=false; cout<<"number: off\n"; save_config(); }
                else cout<<P.warn<<"usage: set number on|off"<<C_RESET<<"\n";
            } else if(what=="backup"){
                if(val=="on"||val=="1"||val=="true"){ buf.backup=true; cout<<"backup: on\n"; save_config(); }
                else if(val=="off"||val=="0"||val=="false"){ buf.backup=false; cout<<"backup: off\n"; save_config(); }
                else cout<<P.warn<<"usage: set backup on|off"<<C_RESET<<"\n";
            } else if(what=="autosave"){
                long s=0; if(!parse_long(val,s)){ cout<<P.warn<<"usage: set autosave <seconds>"<<C_RESET<<"\n"; return true; }
                autosave_sec = (int)std::max<long>(0,s);
                cout<<"autosave: "<<autosave_sec<<"s\n"; save_config();
            } else if(what=="wrap"){
                bool b=false; if(!parse_bool_string(val,b)){ cout<<P.warn<<"usage: set wrap on|off"<<C_RESET<<"\n"; return true; }
                wrap_long=b; cout<<"wrap: "<<(wrap_long?"on":"off")<<"\n"; save_config();
            } else if(what=="truncate"){
                bool b=false; if(!parse_bool_string(val,b)){ cout<<P.warn<<"usage: set truncate on|off"<<C_RESET<<"\n"; return true; }
                truncate_long=b; cout<<"truncate: "<<(truncate_long?"on":"off")<<"\n"; save_config();
            } else if(what=="lang"){
                if(val=="cpp"||val=="c"||val=="c++"||val=="hpp"||val=="h") lang=Lang::Cpp;
                else if(val=="py"||val=="python") lang=Lang::Python;
                else if(val=="sh"||val=="bash"||val=="zsh"||val=="shell") lang=Lang::Shell;
                else if(val=="rb"||val=="ruby") lang=Lang::Ruby;
                else if(val=="js"||val=="javascript"||val=="ts"||val=="typescript") lang=Lang::JS;
                else if(val=="html"||val=="htm") lang=Lang::HTML;
                else if(val=="css") lang=Lang::CSS;
                else if(val=="json") lang=Lang::JSON;
                else lang=Lang::Plain;
                cout<<"lang: set\n";
            } else cout<<P.warn<<"unknown setting"<<C_RESET<<"\n";
            return true;
        }

        if(lc=="syntax"){
            if(rest.empty()){ cout<<P.warn<<"usage: syntax <name>"<<C_RESET<<"\n"; return true; }
            return handle("set lang " + rest);
        }

        if(lc=="number"){ buf.number = !buf.number; cout<<"number: "<<(buf.number?"on":"off")<<"\n"; save_config(); return true; }

        if(lc=="theme"){
            if(rest.empty()){
                cout<<P.warn<<"usage: theme <name>"<<C_RESET<<"\n";
                return true;
            }
            if(lower(rest)=="preview"){
                theme_preview();
                return true;
            }
            cycle_theme(rest);
            return true;
        }

        if(lc=="config"){ show_config_paths(); return true; }
        if(lc=="recent"){ show_recent(); return true; }
        if(lc=="messages"){ show_messages(); return true; }

        if(lc=="lua-themes"){ 
            list_lua_themes();
            return true;
        }

        if(lc=="highlight"){
            string v=lower(rest);
            if(v=="on"||v=="1"||v=="true"){ buf.highlight=true; cout<<"highlight: on\n"; save_config(); }
            else if(v=="off"||v=="0"||v=="false"){ buf.highlight=false; cout<<"highlight: off\n"; save_config(); }
            else cout<<P.warn<<"usage: highlight on|off"<<C_RESET<<"\n";
            return true;
        }

        if(lc=="alias"){
            std::istringstream ts(rest); string from; ts>>from; string to; std::getline(ts,to); to=trim_copy(to);
            if(from.empty()||to.empty()){ cout<<P.warn<<"usage: alias <from> <to...>"<<C_RESET<<"\n"; return true; }
            aliases[from]=to; cout<<"alias: "<<from<<" -> "<<to<<"\n"; save_config(); return true; }
            if(lc=="new"){ string p=rest.empty()?rest:expand_path(rest); open_new_buffer(p); return true; }
            if(lc=="bnext"){ bnext(); return true; }
            if(lc=="bprev"){ bprev(); return true; }
            if(lc=="lsb"){ list_buffers(); return true; }
            if(lc=="buffer"){
                long n=0; if(!parse_long(rest,n) || n<0){ cout<<P.warn<<"usage: buffer <n>"<<C_RESET<<"\n"; return true; }
                switch_buffer((size_t)n); return true;
            }
            if(lc=="close"){ close_buffer(); return true; }

            if(lc=="diff"){ show_diff(); return true; }

            if(lc=="pwd"){ std::error_code ec; auto p = fs::current_path(ec); if(ec) cout<<P.err<<"pwd: "<<ec.message()<<C_RESET<<"\n"; else cout<<p.string()<<"\n"; return true; }
            if(lc=="ls"){
                bool all=false,longfmt=false; string target=".";
                std::istringstream ts2(rest); string t; vector<string> args;
                while(ts2>>t) args.push_back(t);
                for(size_t i=0;i<args.size();++i){
                    if(args[i]=="-a") all=true;
                    else if(args[i]=="-l") longfmt=true;
                    else target=args[i];
                }
                if(target.empty()) target=".";
                ls_list(target, all, longfmt);
                return true;
            }

            if(lc=="cd"){
                if(rest.empty()){
                    cout<<P.warn<<"cd: requires a directory path (try ., .., ~, or a folder name)"<<C_RESET<<"\n";
                    return true;
                }
                string target = expand_path(rest);
                std::error_code ec;
                fs::file_status st = fs::status(target, ec);
                if(ec || !fs::exists(st)){
                    cout<<P.err<<"cd: no such directory: "<<target<<C_RESET<<"\n";
                    return true;
                }
                if(!fs::is_directory(st)){
                    cout<<P.err<<"cd: not a directory: "<<target<<C_RESET<<"\n";
                    return true;
                }
                fs::current_path(target, ec);
                if(ec) cout<<P.err<<"cd: "<<ec.message()<<C_RESET<<"\n";
                else cout<<P.ok<<"cd: "<<fs::current_path().string()<<C_RESET<<"\n";
                return true;
            }

            if(lc=="clear"){ clear_screen(); return true; }

            if(lc=="lua"){
                if(!L){
                    cout<<P.err<<"lua: not available"<<C_RESET<<"\n";
                    return true;
                }
                if(rest.empty()){
                    cout<<P.warn<<"usage: lua <code>"<<C_RESET<<"\n";
                    return true;
                }
                int rc = luaL_loadstring(L, rest.c_str());
                if(rc != LUA_OK){
                    const char* msg = lua_tostring(L, -1);
                    cout<<P.err<<"lua: "<<(msg?msg:"")<<C_RESET<<"\n";
                    lua_pop(L, 1);
                    return true;
                }
                rc = lua_pcall(L, 0, LUA_MULTRET, 0);
                if(rc != LUA_OK){
                    const char* msg = lua_tostring(L, -1);
                    cout<<P.err<<"lua: "<<(msg?msg:"")<<C_RESET<<"\n";
                    lua_pop(L, 1);
                }
                return true;
            }

            if(lc=="luafile"){
                if(!L){
                    cout<<P.err<<"luafile: lua not available"<<C_RESET<<"\n";
                    return true;
                }
                if(rest.empty()){
                    cout<<P.warn<<"usage: luafile <path>"<<C_RESET<<"\n";
                    return true;
                }
                string p = expand_path(rest);
                int rc = luaL_loadfile(L, p.c_str());
                if(rc != LUA_OK){
                    const char* msg = lua_tostring(L, -1);
                    cout<<P.err<<"luafile: "<<(msg?msg:"")<<C_RESET<<"\n";
                    lua_pop(L, 1);
                    return true;
                }
                rc = lua_pcall(L, 0, 0, 0);
                if(rc != LUA_OK){
                    const char* msg = lua_tostring(L, -1);
                    cout<<P.err<<"luafile: "<<(msg?msg:"")<<C_RESET<<"\n";
                    lua_pop(L, 1);
                }
                return true;
            }

            if(lc=="plugin"){
                std::istringstream ps(rest);
                string sub, key;
                ps>>sub>>key;
                sub=lower(sub);
                if(sub=="trusted"||sub=="list"){
                    if(trusted_plugins.empty()) cout<<"no trusted plugins\n";
                    else for(const auto& k: trusted_plugins) cout<<"- "<<k<<"\n";
                    return true;
                }
                if(key.empty()){
                    cout<<P.warn<<"usage: plugin trust|untrust <name|path>"<<C_RESET<<"\n";
                    return true;
                }
                string trust_key = plugin_key_for(key);
                if(sub=="trust"){
                    trust_plugin_key(trust_key);
                    cout<<"trusted plugin: "<<trust_key<<"\n";
                    return true;
                }
                if(sub=="untrust"){
                    if(untrust_plugin_key(trust_key)) cout<<"untrusted plugin: "<<trust_key<<"\n";
                    else cout<<"plugin was not trusted: "<<trust_key<<"\n";
                    return true;
                }
                cout<<P.warn<<"usage: plugin trust|untrust|trusted <name|path>"<<C_RESET<<"\n";
                return true;
            }

            if(lc=="run-plugin"){
                if(!had_colon){
                    cout<<P.warn<<"run-plugin must be invoked as :run-plugin <name|path>"<<C_RESET<<"\n";
                    return true;
                }
                if(!L){
                    cout<<P.err<<"run-plugin: lua not available"<<C_RESET<<"\n";
                    return true;
                }
                if(rest.empty()){
                    cout<<P.warn<<"usage: run-plugin <name|path>"<<C_RESET<<"\n";
                    return true;
                }
                string key = rest;
                string path;
                string disp;
                auto itp = plugin_files.find(key);
                if(itp != plugin_files.end()){
                    path = itp->second;
                    disp = key;
                } else {
                    string p = expand_path(key);
                    if(!file_exists(p)){
                        cout<<P.err<<"run-plugin: no such plugin or path: "<<key<<C_RESET<<"\n";
                        return true;
                    }
                    path = p;
                    disp = fs::path(path).filename().string();
                }
                (void)run_plugin_file(disp, path);
                return true;
            }

            if(lc=="plugins"){
                if(plugin_names.empty()){
                    cout<<"no plugins found (reload-plugins to rescan)\n";
                } else {
                    cout<<"available plugins:\n";
                    for(const auto& n : plugin_names){
                        auto itp = plugin_files.find(n);
                        bool is_current = (!current_plugin.empty() && current_plugin == n);
                        if(itp != plugin_files.end()){
                            cout<<"- "<<n<<" ("<<itp->second<<")"<<(is_current?" *":"")<<"\n";
                        } else {
                            cout<<"- "<<n<<(is_current?" *":"")<<"\n";
                        }
                    }
                }
                return true;
            }

            if(lc=="reload-plugins"){
                if(!L){
                    cout<<P.err<<"reload-plugins: lua not available"<<C_RESET<<"\n";
                    return true;
                }
                load_lua_plugins();
                cout<<"plugins reloaded\n";
                return true;
            }

            if(lc=="version" || lc=="ver"){
                cout<<P.title<<"tedit "<<TEDIT_VERSION<<C_RESET<<"\n";
                return true;
            }

            cout<<P.warn<<"unknown command - type 'help'"<<C_RESET<<"\n"; return true;
    }
};
