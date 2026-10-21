#define TEDIT_VERSION "2.3.1"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <libgen.h>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/ioctl.h>
#include <termios.h>
#endif
#include <chrono>
#include <climits>
#include <ctime>
#include <deque>
#include <filesystem>
#include <limits>
#include <map>
#include <poll.h>
#include <pwd.h>
#include <random>
#include <unordered_set>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace fs = std::filesystem;
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::vector;

struct Editor;
static Editor *g_editor = nullptr;

static int l_tedit_echo(lua_State *L);
static int l_tedit_command(lua_State *L);
static int l_tedit_print(lua_State *L);
static int l_tedit_line_count(lua_State *L);
static int l_tedit_get_line(lua_State *L);
static int l_tedit_set_line(lua_State *L);
static int l_tedit_insert_line(lua_State *L);
static int l_tedit_delete_line(lua_State *L);
static int l_tedit_append_line(lua_State *L);
static int l_tedit_path(lua_State *L);
static int l_tedit_is_dirty(lua_State *L);
static void register_tedit_lua_api(lua_State *L);

// This is an amalgamation. The implementation order follows dependencies.
// clang-format off
#include "platform.cpp"
#include "theme.cpp"
#include "text.cpp"
#include "buffer.cpp"
#include "file_io.cpp"
#include "ranges.cpp"
#include "search.cpp"
#include "filter.cpp"
#include "listing.cpp"
#include "highlight.cpp"
#include "indent.cpp"
#include "lsp.cpp"
#include "terminal.cpp"
#include "line_reader.cpp"
#include "editor.cpp"
#include "lua_bridge.cpp"
#include "main.cpp"
// clang-format on
