struct Buffer{
    string path;
    vector<string> lines;
    bool dirty=false;
    bool number=true;
    bool backup=true;
    bool highlight=false;
};

static size_t char_count(const Buffer& b){ size_t t=0; for(auto& L: b.lines) t += L.size()+1; return t; }


struct Snap{ vector<string> lines; };
static const size_t UNDO_MAX=200;
struct Stack{
    vector<Snap> st;
    void clear(){ for(auto& s: st) s.lines.clear(); st.clear(); }
    void push(const Buffer& b){ if(st.size()==UNDO_MAX) st.erase(st.begin()); st.push_back(Snap{b.lines}); }
    bool pop(Snap& s){ if(st.empty()) return false; s=st.back(); st.pop_back(); return true; }
};
