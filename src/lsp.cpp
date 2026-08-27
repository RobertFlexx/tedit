struct JsonValue {
  enum class Type {
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
  } type = Type::Null;
  bool boolean = false;
  long long number = 0;
  string text;
  vector<JsonValue> array;
  std::map<string, JsonValue> object;
  const JsonValue *get(const string &key) const {
    auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
  }
};

struct JsonParser {
  const string &source;
  size_t position = 0;
  void space() {
    while (position < source.size() &&
           std::isspace((unsigned char)source[position]))
      ++position;
  }
  bool parse(JsonValue &value) {
    space();
    if (!value_at(value))
      return false;
    space();
    return position == source.size();
  }
  bool value_at(JsonValue &value) {
    space();
    if (position >= source.size())
      return false;
    char c = source[position];
    if (c == '"') {
      value.type = JsonValue::Type::String;
      return string_at(value.text);
    }
    if (c == '{')
      return object_at(value);
    if (c == '[')
      return array_at(value);
    if (c == '-' || std::isdigit((unsigned char)c)) {
      size_t start = position++;
      while (position < source.size() &&
             (std::isdigit((unsigned char)source[position]) ||
              source[position] == '.'))
        ++position;
      string token = source.substr(start, position - start);
      char *end = nullptr;
      value.number = std::strtoll(token.c_str(), &end, 10);
      value.type = JsonValue::Type::Number;
      return end && end != token.c_str();
    }
    if (source.compare(position, 4, "null") == 0) {
      position += 4;
      return true;
    }
    if (source.compare(position, 4, "true") == 0) {
      position += 4;
      value.type = JsonValue::Type::Boolean;
      value.boolean = true;
      return true;
    }
    if (source.compare(position, 5, "false") == 0) {
      position += 5;
      value.type = JsonValue::Type::Boolean;
      return true;
    }
    return false;
  }
  bool string_at(string &out) {
    if (position >= source.size() || source[position++] != '"')
      return false;
    out.clear();
    while (position < source.size()) {
      unsigned char c = (unsigned char)source[position++];
      if (c == '"')
        return true;
      if (c != '\\') {
        out.push_back((char)c);
        continue;
      }
      if (position >= source.size())
        return false;
      char e = source[position++];
      if (e == 'n')
        out.push_back('\n');
      else if (e == 'r')
        out.push_back('\r');
      else if (e == 't')
        out.push_back('\t');
      else if (e == 'b')
        out.push_back('\b');
      else if (e == 'f')
        out.push_back('\f');
      else if (e == '"' || e == '\\' || e == '/')
        out.push_back(e);
      else if (e == 'u') {
        if (position + 4 > source.size())
          return false;
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
          char h = source[position++];
          value <<= 4;
          if (h >= '0' && h <= '9')
            value += (unsigned)(h - '0');
          else if (h >= 'a' && h <= 'f')
            value += (unsigned)(h - 'a' + 10);
          else if (h >= 'A' && h <= 'F')
            value += (unsigned)(h - 'A' + 10);
          else
            return false;
        }
        if (value < 0x80)
          out.push_back((char)value);
        else if (value < 0x800) {
          out.push_back((char)(0xc0 | (value >> 6)));
          out.push_back((char)(0x80 | (value & 63)));
        } else {
          out.push_back((char)(0xe0 | (value >> 12)));
          out.push_back((char)(0x80 | ((value >> 6) & 63)));
          out.push_back((char)(0x80 | (value & 63)));
        }
      } else
        return false;
    }
    return false;
  }
  bool array_at(JsonValue &value) {
    value.type = JsonValue::Type::Array;
    ++position;
    space();
    if (position < source.size() && source[position] == ']') {
      ++position;
      return true;
    }
    while (position < source.size()) {
      JsonValue item;
      if (!value_at(item))
        return false;
      value.array.push_back(std::move(item));
      space();
      if (position < source.size() && source[position] == ']') {
        ++position;
        return true;
      }
      if (position >= source.size() || source[position++] != ',')
        return false;
    }
    return false;
  }
  bool object_at(JsonValue &value) {
    value.type = JsonValue::Type::Object;
    ++position;
    space();
    if (position < source.size() && source[position] == '}') {
      ++position;
      return true;
    }
    while (position < source.size()) {
      string key;
      if (!string_at(key)) {
        space();
        if (!string_at(key))
          return false;
      }
      space();
      if (position >= source.size() || source[position++] != ':')
        return false;
      JsonValue item;
      if (!value_at(item))
        return false;
      value.object.emplace(std::move(key), std::move(item));
      space();
      if (position < source.size() && source[position] == '}') {
        ++position;
        return true;
      }
      if (position >= source.size() || source[position++] != ',')
        return false;
      space();
    }
    return false;
  }
};

static string json_escape(const string &input) {
  string out = "\"";
  static const char hex[] = "0123456789abcdef";
  for (unsigned char c : input) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back((char)c);
    } else if (c == '\n')
      out += "\\n";
    else if (c == '\r')
      out += "\\r";
    else if (c == '\t')
      out += "\\t";
    else if (c < 0x20) {
      out += "\\u00";
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 15]);
    } else
      out.push_back((char)c);
  }
  out.push_back('"');
  return out;
}

static string path_to_uri(const string &path) {
  std::error_code ec;
  fs::path absolute = fs::absolute(path.empty() ? "." : path, ec);
  string raw = ec ? path : absolute.string();
  string uri = "file://";
  static const char hex[] = "0123456789ABCDEF";
  for (unsigned char c : raw) {
    if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' ||
        c == '~')
      uri.push_back((char)c);
    else {
      uri.push_back('%');
      uri.push_back(hex[c >> 4]);
      uri.push_back(hex[c & 15]);
    }
  }
  return uri;
}

struct LspTextEdit {
  size_t start = 0, end = 0;
  string replacement;
};

struct LspClient {
  pid_t pid = -1;
  int input = -1, output = -1;
  long long next_id = 1;
  bool initialized = false;
  string opened_uri;
  int version = 0;
  ~LspClient() { stop(); }
  bool running() const { return pid > 0; }
  bool start(const vector<string> &command, string &err) {
    stop();
    if (command.empty()) {
      err = "missing server command";
      return false;
    }
    int to_child[2], from_child[2];
    if (::pipe(to_child)) {
      err = std::strerror(errno);
      return false;
    }
    if (::pipe(from_child)) {
      err = std::strerror(errno);
      ::close(to_child[0]);
      ::close(to_child[1]);
      return false;
    }
    pid = ::fork();
    if (pid < 0) {
      err = std::strerror(errno);
      ::close(to_child[0]);
      ::close(to_child[1]);
      ::close(from_child[0]);
      ::close(from_child[1]);
      return false;
    }
    if (pid == 0) {
      ::dup2(to_child[0], STDIN_FILENO);
      ::dup2(from_child[1], STDOUT_FILENO);
      ::close(to_child[0]);
      ::close(to_child[1]);
      ::close(from_child[0]);
      ::close(from_child[1]);
      vector<char *> argv;
      for (const auto &arg : command)
        argv.push_back(const_cast<char *>(arg.c_str()));
      argv.push_back(nullptr);
      ::execvp(command[0].c_str(), argv.data());
      _exit(127);
    }
    ::close(to_child[0]);
    ::close(from_child[1]);
    input = to_child[1];
    output = from_child[0];
    return initialize(err);
  }
  void stop() {
    if (input >= 0)
      ::close(input);
    if (output >= 0)
      ::close(output);
    input = output = -1;
    if (pid > 0) {
      ::kill(pid, SIGTERM);
      while (::waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
      }
    }
    pid = -1;
    initialized = false;
    opened_uri.clear();
    version = 0;
  }
  bool send_body(const string &body, string &err) {
    string message =
        "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    return write_all(input, message.data(), message.size(), err, "lsp write");
  }
  bool read_exact(char *data, size_t length, int timeout_ms) {
    size_t done = 0;
    while (done < length) {
      pollfd descriptor{output, POLLIN, 0};
      int ready = ::poll(&descriptor, 1, timeout_ms);
      if (ready <= 0)
        return false;
      ssize_t n = ::read(output, data + done, length - done);
      if (n <= 0)
        return false;
      done += (size_t)n;
    }
    return true;
  }
  bool read_message(string &body, string &err) {
    string header;
    char c = 0;
    while (header.size() < 16384) {
      if (!read_exact(&c, 1, 5000)) {
        err = "language server response timed out";
        return false;
      }
      header.push_back(c);
      if (header.size() >= 4 &&
          header.compare(header.size() - 4, 4, "\r\n\r\n") == 0)
        break;
    }
    size_t marker = header.find("Content-Length:");
    if (marker == string::npos) {
      err = "invalid language server header";
      return false;
    }
    marker += 15;
    while (marker < header.size() &&
           std::isspace((unsigned char)header[marker]))
      ++marker;
    size_t end = marker;
    while (end < header.size() && std::isdigit((unsigned char)header[end]))
      ++end;
    unsigned long long length =
        std::strtoull(header.substr(marker, end - marker).c_str(), nullptr, 10);
    if (length > 64U * 1024U * 1024U) {
      err = "language server response is too large";
      return false;
    }
    body.resize((size_t)length);
    return read_exact(body.data(), body.size(), 5000);
  }
  bool wait_id(long long id, JsonValue &root, string &err) {
    for (int attempts = 0; attempts < 32; ++attempts) {
      string body;
      if (!read_message(body, err))
        return false;
      JsonValue parsed;
      JsonParser parser{body};
      if (!parser.parse(parsed))
        continue;
      const JsonValue *found = parsed.get("id");
      if (found && found->type == JsonValue::Type::Number &&
          found->number == id) {
        root = std::move(parsed);
        return true;
      }
    }
    err = "language server did not return the requested response";
    return false;
  }
  bool initialize(string &err) {
    long long id = next_id++;
    std::error_code cwd_error;
    string root_uri = path_to_uri(fs::current_path(cwd_error).string());
    string body = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
                  ",\"method\":\"initialize\",\"params\":{\"processId\":" +
                  std::to_string((long long)::getpid()) +
                  ",\"rootUri\":" + json_escape(root_uri) +
                  ",\"capabilities\":{}}}";
    JsonValue response;
    if (!send_body(body, err) || !wait_id(id, response, err)) {
      stop();
      return false;
    }
    if (response.get("error")) {
      err = "language server initialization failed";
      stop();
      return false;
    }
    if (!send_body(
            "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}",
            err)) {
      stop();
      return false;
    }
    initialized = true;
    return true;
  }
  static string buffer_text(const Buffer &buffer) {
    string text;
    text.reserve(char_count(buffer));
    for (size_t i = 0; i < buffer.lines.size(); ++i) {
      text += buffer.lines[i];
      if (i + 1 < buffer.lines.size() || buffer.final_newline)
        text.push_back('\n');
    }
    return text;
  }
  static size_t utf16_to_byte(const string &line, size_t units) {
    size_t i = 0, used = 0;
    while (i < line.size() && used < units) {
      unsigned char c = (unsigned char)line[i];
      size_t n = c < 0x80             ? 1
                 : (c & 0xe0) == 0xc0 ? 2
                 : (c & 0xf0) == 0xe0 ? 3
                                      : 4;
      unsigned codepoint = 0;
      if (n == 4 && i + 3 < line.size())
        codepoint = ((c & 7) << 18) |
                    (((unsigned char)line[i + 1] & 63) << 12) |
                    (((unsigned char)line[i + 2] & 63) << 6) |
                    ((unsigned char)line[i + 3] & 63);
      used += codepoint > 0xffff ? 2 : 1;
      i = std::min(line.size(), i + n);
    }
    return i;
  }
  static bool position_offset(const Buffer &buffer, const JsonValue &position,
                              size_t &offset) {
    const JsonValue *line = position.get("line");
    const JsonValue *character = position.get("character");
    if (!line || !character || line->number < 0 || character->number < 0 ||
        (size_t)line->number > buffer.lines.size())
      return false;
    offset = 0;
    for (size_t i = 0; i < (size_t)line->number; ++i) {
      offset += buffer.lines[i].size();
      if (i + 1 < buffer.lines.size() || buffer.final_newline)
        ++offset;
    }
    if ((size_t)line->number < buffer.lines.size())
      offset += utf16_to_byte(buffer.lines[(size_t)line->number],
                              (size_t)character->number);
    return true;
  }
  bool format(Buffer &buffer, const string &language, int tab_size,
              string &err) {
    if (!running() || !initialized) {
      err = "no language server is running";
      return false;
    }
    string uri = path_to_uri(buffer.path);
    string text = buffer_text(buffer);
    if (opened_uri != uri) {
      if (!opened_uri.empty())
        (void)send_body("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/"
                        "didClose\",\"params\":{\"textDocument\":{\"uri\":" +
                            json_escape(opened_uri) + "}}}",
                        err);
      opened_uri = uri;
      version = 1;
      if (!send_body("{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/"
                     "didOpen\",\"params\":{\"textDocument\":{\"uri\":" +
                         json_escape(uri) +
                         ",\"languageId\":" + json_escape(language) +
                         ",\"version\":1,\"text\":" + json_escape(text) + "}}}",
                     err))
        return false;
    } else {
      ++version;
      if (!send_body(
              "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/"
              "didChange\",\"params\":{\"textDocument\":{\"uri\":" +
                  json_escape(uri) + ",\"version\":" + std::to_string(version) +
                  "},\"contentChanges\":[{\"text\":" + json_escape(text) +
                  "}]}}",
              err))
        return false;
    }
    long long id = next_id++;
    string request = "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) +
                     ",\"method\":\"textDocument/"
                     "formatting\",\"params\":{\"textDocument\":{\"uri\":" +
                     json_escape(uri) +
                     "},\"options\":{\"tabSize\":" + std::to_string(tab_size) +
                     ",\"insertSpaces\":true}}}";
    JsonValue response;
    if (!send_body(request, err) || !wait_id(id, response, err))
      return false;
    const JsonValue *result = response.get("result");
    if (!result) {
      const JsonValue *failure = response.get("error");
      const JsonValue *message = failure ? failure->get("message") : nullptr;
      err = message && message->type == JsonValue::Type::String
                ? message->text
                : "language server returned an error";
      return false;
    }
    if (result->type == JsonValue::Type::Null)
      return true;
    if (result->type != JsonValue::Type::Array) {
      err = "invalid formatting response";
      return false;
    }
    vector<LspTextEdit> edits;
    for (const auto &item : result->array) {
      const JsonValue *range = item.get("range");
      const JsonValue *replacement = item.get("newText");
      if (!range || !replacement || range->type != JsonValue::Type::Object ||
          replacement->type != JsonValue::Type::String)
        continue;
      const JsonValue *start = range->get("start");
      const JsonValue *end = range->get("end");
      if (!start || !end)
        continue;
      LspTextEdit edit;
      if (position_offset(buffer, *start, edit.start) &&
          position_offset(buffer, *end, edit.end)) {
        edit.replacement = replacement->text;
        edits.push_back(std::move(edit));
      }
    }
    std::sort(edits.begin(), edits.end(),
              [](const LspTextEdit &a, const LspTextEdit &b) {
                return a.start > b.start;
              });
    for (const auto &edit : edits) {
      if (edit.start > edit.end || edit.end > text.size()) {
        err = "invalid formatting edit range";
        return false;
      }
      text.replace(edit.start, edit.end - edit.start, edit.replacement);
    }
    vector<string> lines;
    size_t begin = 0;
    for (size_t i = 0; i < text.size(); ++i)
      if (text[i] == '\n') {
        string line = text.substr(begin, i - begin);
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        lines.push_back(std::move(line));
        begin = i + 1;
      }
    bool final_newline = !text.empty() && text.back() == '\n';
    if (begin < text.size())
      lines.push_back(text.substr(begin));
    buffer.lines = std::move(lines);
    buffer.final_newline = final_newline;
    return true;
  }
};
