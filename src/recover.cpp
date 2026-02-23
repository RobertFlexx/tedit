#include "recover.hpp"
#include "utils.hpp"
#include "theme.hpp"

string recover_path_for(const Buffer& b){
    string p = b.path.empty()? ".unnamed" : b.path;
    std::hash<string> H; size_t h = H(p);
    std::ostringstream ss; ss<<home_path()<<"/.tedit-recover-"<<std::hex<<h;
    return ss.str();
}

void autosave_if_needed(const Buffer& b, std::chrono::steady_clock::time_point& last, int interval_sec){
    if(interval_sec<=0) return;
    auto now = std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::seconds>(now - last).count() < interval_sec) return;
    if(!b.dirty) { last = now; return; }
    string rp = recover_path_for(b);
    std::ofstream out(rp, std::ios::binary|std::ios::trunc);
    if(out.good()){
        for(auto& L: b.lines) out<<L<<"\n";
        out.close();
    }
    last = now;
}

bool maybe_recover(Buffer& b){
    string rp = recover_path_for(b);
    if(!file_exists(rp)) return false;
    cout<<C_YEL<<"recovery: found snapshot "<<rp<<C_RESET<<"\n";
    std::ifstream in(rp); if(!in.good()) return false;
    b.lines.clear(); string L; while(std::getline(in,L)){ rstrip_newline(L); b.lines.push_back(L); }
    b.dirty=true;
    return true;
}