static bool parse_range(const string& arg, size_t nlines, size_t& out_lo, size_t& out_hi){
    auto norm_token = [&](string t)->string{
        t = trim_copy(t);
        if(t=="$") return std::to_string(nlines? nlines: 0);
        return t;
    };
    string s; for(char c: arg){ if(!std::isspace((unsigned char)c)) s.push_back(c); }
    if(s.empty()){ out_lo=1; out_hi=nlines? nlines:0; return true; }
    auto dash = s.find('-'); long lo=-1, hi=-1;
    if(dash==string::npos){
        string t=norm_token(s);
        if(!parse_long(t, lo) || lo<=0) return false;
        hi = lo;
    }else{
        string L=norm_token(s.substr(0,dash));
        string R=norm_token(s.substr(dash+1));
        if(L.empty()) lo=1; else { if(!parse_long(L, lo) || lo<=0) return false; }
        if(R.empty()) hi=(long)nlines; else { if(!parse_long(R, hi) || hi<=0) return false; }
    }
    if(nlines==0){ out_lo=1; out_hi=0; return true; }
    if((size_t)lo<1) lo=1;
    if((size_t)hi>nlines) hi=(long)nlines;
    if(lo>hi) return false;
    out_lo=(size_t)lo;
    out_hi=(size_t)hi;
    return true;
}
