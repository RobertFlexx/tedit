static bool run_filter_replace(vector<string>& lines, size_t lo, size_t hi, const string& shcmd, string &err){
    if (lo < 1 || hi < lo || hi > lines.size()) {
        err = "invalid range";
        return false;
    }

    char in_tpl[]  = "/tmp/tedit_in_XXXXXX";
    char out_tpl[] = "/tmp/tedit_out_XXXXXX";

    int in_fd = ::mkstemp(in_tpl);
    if (in_fd < 0) {
        err = "mkstemp(in): " + string(std::strerror(errno));
        return false;
    }

    {
        FILE *f = ::fdopen(in_fd, "w");
        if (!f) {
            err = "fdopen(in): " + string(std::strerror(errno));
            ::close(in_fd);
            ::unlink(in_tpl);
            return false;
        }
        for (size_t i = lo; i <= hi; ++i) {
            if (std::fputs(lines[i - 1].c_str(), f) == EOF || std::fputc('\n', f) == EOF) {
                err = "write temp: " + string(std::strerror(errno));
                ::fclose(f);
                ::unlink(in_tpl);
                return false;
            }
        }
        std::fflush(f);
        ::fclose(f);
    }

    int out_fd = ::mkstemp(out_tpl);
    if (out_fd < 0) {
        err = "mkstemp(out): " + string(std::strerror(errno));
        ::unlink(in_tpl);
        return false;
    }
    ::close(out_fd);

    
    string shell_line = "sh -c " +
    sh_escape(shcmd + " < " + sh_escape(in_tpl) + " > " + sh_escape(out_tpl));

    int rc = run_shell_cmd(shell_line);
    ::unlink(in_tpl);
    if (rc != 0) {
        err = "filter failed (exit " + std::to_string(rc) + ")";
        ::unlink(out_tpl);
        return false;
    }

    vector<string> out_lines;
    {
        std::ifstream ifs(out_tpl);
        if (!ifs) {
            err = "cannot read filter output";
            ::unlink(out_tpl);
            return false;
        }
        string L;
        while (std::getline(ifs, L)) {
            rstrip_newline(L);
            out_lines.push_back(L);
        }
    }
    ::unlink(out_tpl);

    size_t count = hi - lo + 1;
    lines.erase(lines.begin() + (long)lo - 1,
                lines.begin() + (long)lo - 1 + (long)count);
    lines.insert(lines.begin() + (long)lo - 1, out_lines.begin(), out_lines.end());
    return true;
}
