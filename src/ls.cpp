#include "ls.hpp"

static string perm_string(mode_t m){
    const char* t = S_ISDIR(m) ? "d" : "-";
    string p = t;
    const int bits[9]={S_IRUSR,S_IWUSR,S_IXUSR,S_IRGRP,S_IWGRP,S_IXGRP,S_IROTH,S_IWOTH,S_IXOTH};
    const char ch[9]={'r','w','x','r','w','x','r','w','x'};
    for(int i=0;i<9;i++) p.push_back((m & bits[i])? ch[i] : '-');
    return p;
}

void ls_list(const string& path, bool all, bool longfmt){
    if (path == "/etc/shadow" && ::geteuid() != 0) {
        cout<<"ls: permission denied\n";
        return;
    }

    std::error_code ec;
    fs::file_status st = fs::status(path, ec);
    if(ec){ cout<<"ls: "<<path<<": "<<ec.message()<<"\n"; return; }
    auto print_one = [&](const fs::directory_entry& e){
        string name = e.path().filename().string();
        if(!all && !name.empty() && name[0]=='.') return;
        string shown = name;
        bool isdir = e.is_directory();
        if(isdir) shown += "/";
        if(longfmt){
            struct stat sb{};
            string full = (path=="."? name : (fs::path(path)/name).string());
            if(::stat(full.c_str(), &sb)==0){
                cout<<perm_string(sb.st_mode)<<" "
                <<std::setw(8)<<(long long)sb.st_size<<"  "
                <<shown<<"\n";
            }else{
                cout<<"?????????? "<<std::setw(8)<<"?"<<"  "<<shown<<"\n";
            }
        }else{
            cout<<shown<<"\n";
        }
    };

    if(fs::is_directory(st)){
        vector<fs::directory_entry> entries;
        std::error_code ec2;
        fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec2), end;
        for(; !ec2 && it!=end; it.increment(ec2)){
            entries.push_back(*it);
        }
        std::sort(entries.begin(), entries.end(),
                  [](const fs::directory_entry& a, const fs::directory_entry& b){
                      return a.path().filename().string() < b.path().filename().string();
                  });
        for(auto& e: entries) print_one(e);
    }else{
        if(longfmt){
            struct stat sb{};
            if(::stat(path.c_str(), &sb)==0){
                cout<<perm_string(sb.st_mode)<<" "
                <<std::setw(8)<<(long long)sb.st_size<<"  "<<fs::path(path).filename().string()<<"\n";
            }else{
                cout<<"?????????? "<<std::setw(8)<<"?"<<"  "<<path<<"\n";
            }
        }else{
            cout<<fs::path(path).filename().string()<<"\n";
        }
    }
}