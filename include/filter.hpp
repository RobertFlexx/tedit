#ifndef TEDIT_FILTER_HPP
#define TEDIT_FILTER_HPP

#include "config.hpp"

bool run_filter_replace(vector<string>& lines, size_t lo, size_t hi, const string& shcmd, string &err);

#endif // TEDIT_FILTER_HPP
