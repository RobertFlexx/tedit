#ifndef TEDIT_HIGHLIGHT_HPP
#define TEDIT_HIGHLIGHT_HPP

#include "config.hpp"
#include "buffer.hpp"
#include "theme.hpp"

enum class Lang { Plain, Cpp, Python, Shell, Ruby, JS, HTML, CSS, JSON };

Lang detect_lang(const string& path);
string colorize_lang(const string& L, const Buffer& b, const ThemePalette& P, Lang lang);

#endif // TEDIT_HIGHLIGHT_HPP
