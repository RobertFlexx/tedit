#ifndef TEDIT_TERMINAL_HPP
#define TEDIT_TERMINAL_HPP

#include "config.hpp"

int term_width();
void print_wrapped_with_gutter(const string& ansi,
                                      const string& first_prefix,
                                      const string& cont_prefix,
                                      int avail_cols);

#endif // TEDIT_TERMINAL_HPP
