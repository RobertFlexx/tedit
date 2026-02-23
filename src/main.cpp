#include "editor.hpp"

int main(int argc, char** argv){
    std::ios::sync_with_stdio(false); std::cin.tie(nullptr);
    Editor ed;

    ed.load_config();

    if(argc>=2){ ed.load(argv[1]); } else { ed.buf.path.clear(); }

    ed.banner();
    cout<<palette_for(ed.theme).accent<<"tedit — editing "
    <<( ed.buf.path.empty()? "(unnamed)": ed.buf.path )<<" ("
    <<ed.buf.lines.size()<<" lines). Type 'help'."<<C_RESET<<"\n";
    ed.tip();

    for(;;){
        ed.status();
        string line = ed.lr.read(ed.prompt_str());
        if(!std::cin.good() && line.empty()){ cout<<"\n"; break; }
        if(line.empty()) continue;
        ed.lr.remember(line);
        bool keep = ed.handle(line);
        if(!keep) break;
    }
    return 0;
}
