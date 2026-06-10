int main(int argc, char** argv){
    std::ios::sync_with_stdio(false); std::cin.tie(nullptr);

    if(argc >= 2){
        string arg1 = argv[1];
        if(arg1 == "--version" || arg1 == "-V"){
            cout<<"tedit "<<TEDIT_VERSION<<"\n";
            return 0;
        }
        if(arg1 == "--help" || arg1 == "-h"){
            cout<<"usage: tedit [file ...]\n"
                <<"       tedit --help\n"
                <<"       tedit --version\n"
                <<"\n"
                <<"Open one or more files. Extra files start as buffers.\n";
            return 0;
        }
    }

    Editor ed;

    ed.load_config();

    if(argc>=2){
        ed.load(argv[1]);
        for(int i=2;i<argc;i++) ed.add_background_buffer(argv[i]);
    } else { ed.buf.path.clear(); }

    ed.banner();
    cout<<ed.P.title<<"tedit "<<TEDIT_VERSION<<C_RESET<<"\n"
    <<ed.P.dim<<"file: "<<C_RESET<<( ed.buf.path.empty()? "(unnamed)": ed.buf.path )<<"\n"
    <<ed.P.dim<<"lines: "<<C_RESET<<ed.buf.lines.size()<<"  "
    <<ed.P.dim<<"buffers: "<<C_RESET<<ed.buffer_count()<<"  "
    <<ed.P.dim<<"help: "<<C_RESET<<"help, help <command>"<<"\n";
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
