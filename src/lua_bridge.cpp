static int l_tedit_echo(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  if (g_editor) {
    cout << g_editor->P.accent;
    if (s)
      cout << escape_for_display(s);
    cout << C_RESET << "\n";
  }
  return 0;
}

static int l_tedit_command(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  if (g_editor && s) {
    string cmd = s;
    if (!cmd.empty())
      g_editor->handle(cmd);
  }
  return 0;
}

static int l_tedit_print(lua_State *L) {
  lua_Integer ln = luaL_checkinteger(L, 1);
  if (g_editor) {
    if (ln >= 1 && (size_t)ln <= g_editor->buf.lines.size()) {
      g_editor->print((size_t)ln, (size_t)ln);
    }
  }
  return 0;
}

static int l_tedit_line_count(lua_State *L) {
  lua_pushinteger(L, g_editor ? (lua_Integer)g_editor->buf.lines.size() : 0);
  return 1;
}

static bool lua_line_index(lua_State *L, int arg, bool allow_end,
                           size_t &index) {
  lua_Integer line = luaL_checkinteger(L, arg);
  size_t count = g_editor ? g_editor->buf.lines.size() : 0;
  lua_Integer maximum = (lua_Integer)count + (allow_end ? 1 : 0);
  if (line < 1 || line > maximum) {
    luaL_error(L, "line number out of range");
    return false;
  }
  index = (size_t)line - 1;
  return true;
}

static int l_tedit_get_line(lua_State *L) {
  if (!g_editor) {
    lua_pushnil(L);
    return 1;
  }
  size_t index = 0;
  if (!lua_line_index(L, 1, false, index))
    return 0;
  const string &line = g_editor->buf.lines[index];
  lua_pushlstring(L, line.data(), line.size());
  return 1;
}

static int l_tedit_set_line(lua_State *L) {
  if (!g_editor)
    return luaL_error(L, "editor is unavailable");
  size_t index = 0;
  if (!lua_line_index(L, 1, false, index))
    return 0;
  size_t length = 0;
  const char *value = luaL_checklstring(L, 2, &length);
  string replacement(value, length);
  if (g_editor->buf.lines[index] != replacement) {
    g_editor->push_undo();
    g_editor->buf.lines[index] = std::move(replacement);
    g_editor->buf.dirty = true;
  }
  lua_pushboolean(L, 1);
  return 1;
}

static int l_tedit_insert_line(lua_State *L) {
  if (!g_editor)
    return luaL_error(L, "editor is unavailable");
  size_t index = 0;
  if (!lua_line_index(L, 1, true, index))
    return 0;
  size_t length = 0;
  const char *value = luaL_checklstring(L, 2, &length);
  g_editor->push_undo();
  g_editor->buf.lines.insert(g_editor->buf.lines.begin() + (long)index,
                             string(value, length));
  if (index + 1 == g_editor->buf.lines.size())
    g_editor->buf.final_newline = true;
  g_editor->buf.dirty = true;
  lua_pushboolean(L, 1);
  return 1;
}

static int l_tedit_delete_line(lua_State *L) {
  if (!g_editor)
    return luaL_error(L, "editor is unavailable");
  size_t index = 0;
  if (!lua_line_index(L, 1, false, index))
    return 0;
  g_editor->push_undo();
  g_editor->buf.lines.erase(g_editor->buf.lines.begin() + (long)index);
  g_editor->buf.dirty = true;
  lua_pushboolean(L, 1);
  return 1;
}

static int l_tedit_append_line(lua_State *L) {
  if (!g_editor)
    return luaL_error(L, "editor is unavailable");
  size_t length = 0;
  const char *value = luaL_checklstring(L, 1, &length);
  g_editor->push_undo();
  g_editor->buf.lines.emplace_back(value, length);
  g_editor->buf.final_newline = true;
  g_editor->buf.dirty = true;
  lua_pushinteger(L, (lua_Integer)g_editor->buf.lines.size());
  return 1;
}

static int l_tedit_path(lua_State *L) {
  if (!g_editor || g_editor->buf.path.empty()) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushlstring(L, g_editor->buf.path.data(), g_editor->buf.path.size());
  return 1;
}

static int l_tedit_is_dirty(lua_State *L) {
  lua_pushboolean(L, g_editor && g_editor->buf.dirty);
  return 1;
}

static void set_api_function(lua_State *L, const char *name,
                             lua_CFunction function) {
  lua_pushcfunction(L, function);
  lua_setfield(L, -2, name);
}

static void register_tedit_lua_api(lua_State *L) {
  // Preserve the original globals for compatibility with existing plugins.
  lua_register(L, "tedit_command", l_tedit_command);
  lua_register(L, "tedit_echo", l_tedit_echo);
  lua_register(L, "tedit_print", l_tedit_print);

  lua_newtable(L);
  set_api_function(L, "command", l_tedit_command);
  set_api_function(L, "echo", l_tedit_echo);
  set_api_function(L, "print", l_tedit_print);
  set_api_function(L, "line_count", l_tedit_line_count);
  set_api_function(L, "get_line", l_tedit_get_line);
  set_api_function(L, "set_line", l_tedit_set_line);
  set_api_function(L, "insert_line", l_tedit_insert_line);
  set_api_function(L, "delete_line", l_tedit_delete_line);
  set_api_function(L, "append_line", l_tedit_append_line);
  set_api_function(L, "path", l_tedit_path);
  set_api_function(L, "is_dirty", l_tedit_is_dirty);
  lua_pushstring(L, TEDIT_VERSION);
  lua_setfield(L, -2, "version");
  lua_setglobal(L, "tedit");
}
