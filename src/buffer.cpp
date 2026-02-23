#include "buffer.hpp"

size_t char_count(const Buffer& b){
    size_t t=0;
    for(auto& L: b.lines) t += L.size()+1;
    return t;
}

void Stack::clear(){
    for(auto& s: st) s.lines.clear();
    st.clear();
}

void Stack::push(const Buffer& b){
    if(st.size()==UNDO_MAX) st.erase(st.begin());
    st.push_back(Snap{b.lines});
}

bool Stack::pop(Snap& s){
    if(st.empty()) return false;
    s=st.back();
    st.pop_back();
    return true;
}
