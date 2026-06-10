static inline string trim_copy(const string& s){
    size_t i=0,j=s.size();
    while(i<j && std::isspace((unsigned char)s[i])) i++;
    while(j>i && std::isspace((unsigned char)s[j-1])) j--;
    return s.substr(i,j-i);
}
static inline void rstrip_newline(string& s){
    while(!s.empty() && (s.back()=='\n'||s.back()=='\r')) s.pop_back();
}
static inline string lower(string s){ for(char& c: s) c=(char)std::tolower((unsigned char)c); return s; }


static inline bool parse_long(const string& s, long& out){
    if(s.empty()) return false;
    errno = 0;
    char *e=nullptr; long v=strtol(s.c_str(), &e, 10);
    if(errno == ERANGE) return false;
    if(e==s.c_str()||*e) { return false; }
    out = v;
    return true;
}

static inline int digits_for(size_t n){ int w=1; while(n>=10){ n/=10; w++; } return w; }
