#include "highlight.hpp"
#include "utils.hpp"

Lang detect_lang(const string& path){
    string ext = lower(fs::path(path).extension().string());
    if(ext==".c"||ext==".cc"||ext==".cpp"||ext==".cxx"||ext==".h"||ext==".hh"||ext==".hpp") return Lang::Cpp;
    if(ext==".py") return Lang::Python;
    if(ext==".sh"||ext==".bash"||ext==".zsh") return Lang::Shell;
    if(ext==".rb") return Lang::Ruby;
    if(ext==".js"||ext==".mjs"||ext==".ts") return Lang::JS;
    if(ext==".html"||ext==".htm") return Lang::HTML;
    if(ext==".css") return Lang::CSS;
    if(ext==".json") return Lang::JSON;
    return Lang::Plain;
}

static string colorize(const string& L, const Buffer& b, const ThemePalette& P){
    if(!use_color() || !b.highlight) return L;
    string s=L;
    try{
        s = std::regex_replace(s, std::regex(R"("([^"\\]|\\.)*")"), P.accent+"$&"+C_RESET);
        s = std::regex_replace(s, std::regex(R"(//.*$)"), P.dim+"$&"+C_RESET);
        s = std::regex_replace(s, std::regex(R"(\b(auto|break|case|class|const|continue|default|delete|do|else|enum|for|friend|if|inline|namespace|new|noexcept|operator|private|protected|public|return|sizeof|static|struct|switch|template|this|throw|try|typedef|typename|union|using|virtual|void|volatile|while)\b)"), P.ok+"$&"+C_RESET);
    }catch(...){}
    return s;
}

string colorize_lang(const string& L, const Buffer& b, const ThemePalette& P, Lang lang){
    if(!use_color() || !b.highlight) return L;

    if(lang==Lang::Cpp || lang==Lang::Plain) return colorize(L, b, P);

    string s=L;
    try{
        auto qd = std::regex(R"("([^"\\]|\\.)*")");
        auto qs = std::regex(R"('([^'\\]|\\.)*')");
        switch(lang){
            case Lang::Python:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, qs, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(#.*$)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(False|True|None|def|class|return|import|from|if|else|elif|for|while|try|except|finally|with|as|lambda|pass|yield|raise|global|nonlocal|assert|async|await|in|is|and|or|not)\b)"), P.ok+"$&"+C_RESET);
                break;
            case Lang::Shell:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, qs, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(#.*$)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(if|then|else|elif|fi|for|in|do|done|case|esac|function|select|until|time|echo|exit|return)\b)"), P.ok+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\$[A-Za-z_][A-Za-z0-9_]*|\$\{[^}]+\})"), P.accent+"$&"+C_RESET);
                break;
            case Lang::Ruby:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, qs, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(#.*$)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(def|class|module|if|else|elsif|end|do|while|until|return|yield|begin|rescue|ensure|case|when|then|super|self|nil|true|false)\b)"), P.ok+"$&"+C_RESET);
                break;
            case Lang::JS:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, qs, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(//.*$)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(function|return|let|const|var|if|else|for|while|class|extends|import|export|new|try|catch|finally|throw|switch|case|default|break|continue|yield|await|async)\b)"), P.ok+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(true|false|null|undefined|NaN|Infinity)\b)"), P.ok+"$&"+C_RESET);
                break;
            case Lang::HTML:
                s = std::regex_replace(s, std::regex(R"(<!--.*-->)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(<[^>]+>)"), P.accent+"$&"+C_RESET);
                break;
            case Lang::CSS:
                s = std::regex_replace(s, std::regex(R"(\/\*.*\*\/)"), P.dim+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b([A-Za-z_-]+)(?=\s*:))"), P.ok+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"([{};:,])"), P.accent+"$&"+C_RESET);
                break;
            case Lang::JSON:
                s = std::regex_replace(s, qd, P.accent+"$&"+C_RESET);
                s = std::regex_replace(s, std::regex(R"(\b(true|false|null)\b)"), P.ok+"$&"+C_RESET);
                break;
            default: break;
    }
} catch(...) {}
    return s;
}