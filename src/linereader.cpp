#include "linereader.hpp"
#include "utils.hpp"

void LineReader::set_theme_colors(const ThemePalette& P){
    color_input = P.input;
    color_reset = C_RESET;
}

vector<string> LineReader::split_words(const string& s){
    vector<string> v;
    std::istringstream in(s);
    string w;
    while(in>>w) v.push_back(w);
    return v;
}

string LineReader::expand_home_in_token(string in){
    if(in.empty()) return in;
    if(in=="~") return home_path();
    if(in.size()>=2 && in[0]=='~' && in[1]=='/'){
        return home_path()+in.substr(1);
    }
    return in;
}

vector<string> LineReader::complete_dirs_only(const string& token){
    string t = expand_home_in_token(token);
    vector<string> out; string base=t, dir=".";
    auto pos = t.find_last_of('/');
    if(pos!=string::npos){ dir = t.substr(0,pos); base = t.substr(pos+1); }
    std::error_code ec;
    for(auto& e: fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)){
        string name = e.path().filename().string();
        if(name.rfind(base,0)==0 && e.is_directory()){
            string cand = (dir=="."? name: dir+"/"+name);
            out.push_back(cand + "/");
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

vector<string> LineReader::complete_fs(const string& token){
    vector<string> out; string base=token, dir="";
    auto pos = token.find_last_of('/');
    if(pos!=string::npos){ dir = token.substr(0,pos); base = token.substr(pos+1); } else dir = ".";
    std::error_code ec;
    for(auto& e: fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec)){
        string name = e.path().filename().string();
        if(name.rfind(base,0)==0){
            string cand = (dir=="."? name: dir+"/"+name);
            if(e.is_directory()) cand += "/";
            out.push_back(cand);
        }
    }
    std::sort(out.begin(), out.end()); return out;
}

vector<string> LineReader::complete(const string& buf){
    auto toks = split_words(buf); vector<string> out;
    bool at_start = toks.empty();
    bool fresh = (!buf.empty() && std::isspace((unsigned char)buf.back()));

    if(at_start){ out = commands; return out; }

    if(toks.size()==1 && !fresh){
        string pref=toks[0];
        for(const auto& c: commands) if(c.rfind(pref,0)==0) out.push_back(c);
        return out;
    }

    string first = toks[0];

    if(first=="cd"){
        if(toks.size()==1 && fresh){
            return complete_dirs_only("");
        }
        string last = fresh ? string("") : toks.back();
        return complete_dirs_only(last);
    }

    string last = fresh ? string("") : toks.back();
    return complete_fs(last);
}

void LineReader::remember(const string& s){
    if(s.empty()) return;
    if(history.empty() || history.back()!=s){
        if(history.size()<HIST_MAX) history.push_back(s);
        else { history.erase(history.begin()); history.push_back(s); }
    }
}

string LineReader::read(const string& prompt){
    bool tty=false;
    #if defined(__unix__) || defined(__APPLE__)
    tty = isatty(STDIN_FILENO);
    #endif
    if(!tty){
        cout<<prompt<<std::flush;
        string s; if(!std::getline(std::cin, s)) return string(); return s;
    }

    #if defined(__unix__) || defined(__APPLE__)
    cout<<prompt<<std::flush;

    struct termios orig{};
    bool have_orig = false;
    if(isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO,&orig)!=-1){
        struct termios t=orig;
        t.c_lflag &= ~(ECHO|ICANON);
        t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
        if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&t)==0) {
            have_orig = true;
        }
    }

    struct TermiosGuard {
        bool active;
        struct termios o;
        TermiosGuard(bool a, const struct termios& oo):active(a),o(oo){}
        ~TermiosGuard(){
            if(active) tcsetattr(STDIN_FILENO,TCSAFLUSH,&o);
        }
    } guard(have_orig, orig);

    string buf; size_t cursor=0; int hist_idx=(int)history.size();

    auto refresh=[&](){
        cout<<"\r\033[2K"<<prompt<<color_input<<buf<<color_reset;
        size_t tail = buf.size() - cursor;
        if(tail>0) cout<<"\033["<<tail<<"D";
        cout.flush();
    };

    refresh();
    while(true){
        char c=0; ssize_t n=::read(STDIN_FILENO,&c,1);
        if(n<=0) return string();
        if(c=='\r'||c=='\n'){ cout<<"\r\n"; break; }
        else if((unsigned char)c==127||c=='\b'){
            if(cursor>0){ buf.erase(buf.begin()+cursor-1); cursor--; refresh(); }
        }
        else if(c=='\t'){
            auto opts=complete(buf);
            if(opts.empty()){
            } else if(opts.size()==1){
                auto toks=split_words(buf);
                size_t lastsp=buf.find_last_of(' ');
                string prefix=(lastsp==string::npos? string(): buf.substr(0,lastsp+1));
                buf = prefix + opts[0];
                cursor=buf.size(); refresh();
            } else {
                cout<<"\r\n";
                size_t shown=0;
                for(auto& o:opts){ cout<<o<<"  "; if(++shown%6==0) cout<<"\r\n"; }
                if((shown%6)!=0) cout<<"\r\n";
                refresh();
            }
        }
        else if(c==27){
            char seq[2];
            if(::read(STDIN_FILENO,seq,1)<=0) continue;
            if(seq[0]=='['){
                char k;
                if(::read(STDIN_FILENO,&k,1)<=0) continue;
                if(k=='A'){
                    if(hist_idx>0){ hist_idx--; buf=history[hist_idx]; cursor=buf.size(); refresh(); }
                } else if(k=='B'){
                    if(hist_idx<(int)history.size()-1){ hist_idx++; buf=history[hist_idx]; cursor=buf.size(); refresh(); }
                    else { hist_idx=(int)history.size(); buf.clear(); cursor=0; refresh(); }
                } else if(k=='C'){ if(cursor<buf.size()){ cursor++; refresh(); } }
                else if(k=='D'){ if(cursor>0){ cursor--; refresh(); } }
            }
        }
        else if(c==1){ cursor=0; refresh(); }
        else if(c==5){ cursor=buf.size(); refresh(); }
        else{
            buf.insert(buf.begin()+cursor,c);
            cursor++; refresh();
        }
    }
    return buf;
    #else
    string s; cout<<prompt; if(!std::getline(std::cin,s)) return string(); return s;
    #endif
}