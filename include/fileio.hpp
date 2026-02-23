#ifndef TEDIT_FILEIO_HPP
#define TEDIT_FILEIO_HPP

#include "config.hpp"
#include "buffer.hpp"

void load_file(const string& path, Buffer& b);
bool atomic_save_to_fd(FILE* tf, const Buffer& b, string& err);
bool atomic_save(const string& path, const Buffer& b, bool backup, string& err);

#endif // TEDIT_FILEIO_HPP
