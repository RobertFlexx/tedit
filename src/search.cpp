#include "search.hpp"
#include "utils.hpp"

size_t search_plain_allhits(const Buffer& b, const string& q, bool icase, vector<size_t>& out_lines){
    out_lines.clear();
    if(q.empty()) return 0;
    string qq = icase? lower(q): q;
    for(size_t i=0;i<b.lines.size();++i){
        string L = icase? lower(b.lines[i]) : b.lines[i];
        if(L.find(qq)!=string::npos) out_lines.push_back(i+1);
    }
    return out_lines.size();
}

size_t search_plain(const Buffer& b, const string& q, bool icase){
    vector<size_t> hits;
    size_t n = search_plain_allhits(b,q,icase,hits);
    if(!n){ cout<<"no matches\n"; return 0; }
    for(auto ln: hits) cout<<"match at "<<ln<<": "<<b.lines[ln-1]<<"\n";
    return n;
}

size_t search_regex(const Buffer& b, const string& pat){
    size_t hits=0; try{
        std::regex rx(pat);
        for(size_t i=0;i<b.lines.size();++i){
            if(std::regex_search(b.lines[i], rx)){
                cout<<"match at "<<i+1<<": "<<b.lines[i]<<"\n"; hits++;
            }
        }
    } catch(const std::exception& e){ cout<<"regex: "<<e.what()<<"\n"; return 0; }
    if(!hits) cout<<"no matches\n";
    return hits;
}

int replace_first_line(const string& s,const string& needle,const string& repl,string& out){
    auto pos=s.find(needle);
    if(pos==string::npos){ out=s; return 0; }
    out = s.substr(0,pos) + repl + s.substr(pos+needle.size());
    return 1;
}

int replace_all_line(const string& s,const string& needle,const string& repl,string& out){
    if(needle.empty()){ out=s; return 0; }
    int cnt=0; size_t pos=0; out.clear();
    while(true){
        auto p=s.find(needle,pos);
        if(p==string::npos){ out += s.substr(pos); break; }
        out += s.substr(pos, p-pos);
        out += repl;
        pos = p + needle.size();
        cnt++;
    }
    return cnt;
}