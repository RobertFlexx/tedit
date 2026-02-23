#ifndef TEDIT_CONFIG_HPP
#define TEDIT_CONFIG_HPP

#define _POSIX_C_SOURCE 200809L
#define TEDIT_VERSION "2.1.0"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <libgen.h>
#include <regex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <sys/ioctl.h>
#endif
#include <filesystem>
#include <chrono>
#include <map>
#include <random>
#include <ctime>
#include <vector>

extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace fs = std::filesystem;
using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;

#endif // TEDIT_CONFIG_HPP
