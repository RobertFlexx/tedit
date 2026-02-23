#ifndef TEDIT_SEARCH_HPP
#define TEDIT_SEARCH_HPP

#include "config.hpp"
#include "buffer.hpp"

size_t search_plain_allhits(const Buffer& b, const string& q, bool icase, vector<size_t>& out_lines);
size_t search_plain(const Buffer& b, const string& q, bool icase);
size_t search_regex(const Buffer& b, const string& pat);
int replace_first_line(const string& s,const string& needle,const string& repl,string& out);
int replace_all_line(const string& s,const string& needle,const string& repl,string& out);

#endif // TEDIT_SEARCH_HPP
