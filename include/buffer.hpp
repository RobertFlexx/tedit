#ifndef TEDIT_BUFFER_HPP
#define TEDIT_BUFFER_HPP

#include "config.hpp"

struct Buffer{
    string path;
    vector<string> lines;
    bool dirty=false;
    bool number=true;
    bool backup=true;
    bool highlight=false;
};

size_t char_count(const Buffer& b);

struct Snap{ vector<string> lines; };
static const size_t UNDO_MAX=200;
struct Stack{
    vector<Snap> st;
    void clear();
    void push(const Buffer& b);
    bool pop(Snap& s);
};

#endif // TEDIT_BUFFER_HPP
