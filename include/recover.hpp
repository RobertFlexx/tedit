#ifndef TEDIT_RECOVER_HPP
#define TEDIT_RECOVER_HPP

#include "config.hpp"
#include "buffer.hpp"

string recover_path_for(const Buffer& b);
void autosave_if_needed(const Buffer& b, std::chrono::steady_clock::time_point& last, int interval_sec);
bool maybe_recover(Buffer& b);

#endif // TEDIT_RECOVER_HPP
