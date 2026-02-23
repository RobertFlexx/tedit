#include "terminal.hpp"
#include "theme.hpp"

int term_width(){
#if defined(__unix__) || defined(__APPLE__)
    struct winsize ws{};
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==0 && ws.ws_col>0) return ws.ws_col;
#endif
    return 80;
}

void print_wrapped_with_gutter(const string& ansi,
                                      const string& first_prefix,
                                      const string& cont_prefix,
                                      int avail_cols)
{
    int col = 0;
    bool esc = false;

    auto print_prefix = [&](const string& p){
        cout<<p;
    };

    print_prefix(first_prefix);

    for(size_t k=0;k<ansi.size();++k){
        char ch = ansi[k];
        if(!esc){
            if(ch=='\033'){ esc=true; cout<<ch; continue; }
            if(ch=='\n'){
                cout<<"\n";
                col = 0;
                print_prefix(cont_prefix);
                continue;
            }
            if(avail_cols>0 && col>=avail_cols){
                cout<<"\n";
                col = 0;
                print_prefix(cont_prefix);
            }
            cout<<ch;
            col++;
        }else{
            cout<<ch;
            if(ch=='m') esc=false;
        }
    }
    cout<<C_RESET<<"\n";
}