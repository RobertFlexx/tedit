static int l_tedit_echo(lua_State* L){
    const char* s = luaL_checkstring(L, 1);
    if(g_editor){
        cout<<g_editor->P.accent;
        if(s) cout<<s;
        cout<<C_RESET<<"\n";
    }
    return 0;
}

static int l_tedit_command(lua_State* L){
    const char* s = luaL_checkstring(L, 1);
    if(g_editor && s){
        string cmd = s;
        if(!cmd.empty()) g_editor->handle(cmd);
    }
    return 0;
}

static int l_tedit_print(lua_State* L){
    lua_Integer ln = luaL_checkinteger(L, 1);
    if(g_editor){
        if(ln >= 1 && (size_t)ln <= g_editor->buf.lines.size()){
            g_editor->print((size_t)ln, (size_t)ln);
        }
    }
    return 0;
}
