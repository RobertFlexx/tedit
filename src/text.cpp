static inline string trim_copy(const string &s) {
  size_t i = 0, j = s.size();
  while (i < j && std::isspace((unsigned char)s[i]))
    i++;
  while (j > i && std::isspace((unsigned char)s[j - 1]))
    j--;
  return s.substr(i, j - i);
}
static inline void rstrip_newline(string &s) {
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
    s.pop_back();
}
static inline string lower(string s) {
  for (char &c : s)
    c = (char)std::tolower((unsigned char)c);
  return s;
}

// Never send file-controlled terminal escape sequences to the user's terminal.
// The underlying bytes remain unchanged in the buffer and on disk.
static string escape_for_display(const string &s) {
  static const char hex[] = "0123456789abcdef";
  string out;
  out.reserve(s.size());
  for (unsigned char c : s) {
    if (c == '\t') {
      out += "\\t";
    } else if (c < 0x20 || c == 0x7f) {
      out += "\\x";
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 0x0f]);
    } else {
      out.push_back((char)c);
    }
  }
  return out;
}

static size_t previous_utf8_boundary(const string &s, size_t position) {
  if (position == 0)
    return 0;
  size_t i = position - 1;
  while (i > 0 && ((unsigned char)s[i] & 0xc0) == 0x80)
    --i;
  return i;
}

static size_t next_utf8_boundary(const string &s, size_t position) {
  if (position >= s.size())
    return s.size();
  size_t i = position + 1;
  while (i < s.size() && ((unsigned char)s[i] & 0xc0) == 0x80)
    ++i;
  return i;
}

static size_t display_columns(const string &s) {
  std::mbstate_t state{};
  size_t columns = 0;
  const char *p = s.data();
  size_t remaining = s.size();
  while (remaining) {
    wchar_t wc = 0;
    size_t n = std::mbrtowc(&wc, p, remaining, &state);
    if (n == (size_t)-1 || n == (size_t)-2) {
      state = std::mbstate_t{};
      ++p;
      --remaining;
      ++columns;
      continue;
    }
    if (n == 0)
      n = 1;
    int width = ::wcwidth(wc);
    columns += width > 0 ? (size_t)width : 0;
    p += n;
    remaining -= n;
  }
  return columns;
}

static bool split_quoted_words(const string &input, vector<string> &words,
                               string &err) {
  words.clear();
  string current;
  char quote = 0;
  bool escaped = false;
  bool started = false;
  for (char c : input) {
    if (escaped) {
      current.push_back(c);
      escaped = false;
      started = true;
      continue;
    }
    if (c == '\\' && quote != '\'') {
      escaped = true;
      started = true;
      continue;
    }
    if (quote) {
      if (c == quote)
        quote = 0;
      else
        current.push_back(c);
      started = true;
      continue;
    }
    if (c == '\'' || c == '"') {
      quote = c;
      started = true;
      continue;
    }
    if (std::isspace((unsigned char)c)) {
      if (started) {
        words.push_back(std::move(current));
        current.clear();
        started = false;
      }
      continue;
    }
    current.push_back(c);
    started = true;
  }
  if (escaped) {
    err = "trailing escape";
    return false;
  }
  if (quote) {
    err = "unterminated quote";
    return false;
  }
  if (started)
    words.push_back(std::move(current));
  return true;
}

static string join_words(const vector<string> &words, size_t begin) {
  string result;
  for (size_t i = begin; i < words.size(); ++i) {
    if (i > begin)
      result.push_back(' ');
    result += words[i];
  }
  return result;
}

static inline bool parse_long(const string &s, long &out) {
  if (s.empty())
    return false;
  errno = 0;
  char *e = nullptr;
  long v = strtol(s.c_str(), &e, 10);
  if (errno == ERANGE)
    return false;
  if (e == s.c_str() || *e) {
    return false;
  }
  out = v;
  return true;
}

static inline int digits_for(size_t n) {
  int w = 1;
  while (n >= 10) {
    n /= 10;
    w++;
  }
  return w;
}
