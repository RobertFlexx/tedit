static string content_without_indent(const string &line) {
  size_t i = 0;
  while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
    ++i;
  return line.substr(i);
}

static bool starts_with_word(const string &text, const char *word) {
  size_t length = std::strlen(word);
  return text.compare(0, length, word) == 0 &&
         (text.size() == length ||
          !identifier_continue((unsigned char)text[length]));
}

static bool indentation_language(Lang lang) {
  return lang != Lang::Plain && lang != Lang::Markdown;
}

static vector<string> reindent_lines(const Buffer &buffer, size_t lo, size_t hi,
                                     Lang lang, int width) {
  vector<string> result = buffer.lines;
  if (!indentation_language(lang) || result.empty())
    return result;
  int depth = 0;
  bool block_style =
      lang == Lang::Python || lang == Lang::YAML || lang == Lang::Ruby;
  for (size_t index = 0; index < result.size() && index < hi; ++index) {
    string content = content_without_indent(result[index]);
    string trimmed = trim_copy(content);
    bool blank = trimmed.empty();
    bool closes = false;
    if (block_style) {
      closes = starts_with_word(trimmed, "else") ||
               starts_with_word(trimmed, "elif") ||
               starts_with_word(trimmed, "except") ||
               starts_with_word(trimmed, "finally") ||
               starts_with_word(trimmed, "end") ||
               starts_with_word(trimmed, "when") ||
               starts_with_word(trimmed, "rescue") ||
               starts_with_word(trimmed, "ensure");
    } else {
      closes = !trimmed.empty() &&
               (trimmed[0] == '}' || trimmed[0] == ']' || trimmed[0] == ')');
    }
    int line_depth = std::max(0, depth - (closes ? 1 : 0));
    if (index + 1 >= lo && index + 1 <= hi && !blank) {
      result[index] = string((size_t)line_depth * (size_t)width, ' ') + content;
    }
    if (blank)
      continue;
    if (block_style) {
      depth = line_depth;
      bool opens = !trimmed.empty() && trimmed.back() == ':';
      if (lang == Lang::Ruby) {
        opens = opens || starts_with_word(trimmed, "class") ||
                starts_with_word(trimmed, "module") ||
                starts_with_word(trimmed, "def") ||
                starts_with_word(trimmed, "if") ||
                starts_with_word(trimmed, "unless") ||
                starts_with_word(trimmed, "case") ||
                starts_with_word(trimmed, "while") ||
                starts_with_word(trimmed, "until") ||
                starts_with_word(trimmed, "for") ||
                (trimmed.size() >= 2 &&
                 trimmed.compare(trimmed.size() - 2, 2, "do") == 0);
      }
      if (opens)
        ++depth;
    } else {
      int opens = 0, endings = 0;
      bool quoted = false, escaped = false;
      char quote = 0;
      for (size_t i = 0; i < content.size(); ++i) {
        char c = content[i];
        if (quoted) {
          if (!escaped && c == quote)
            quoted = false;
          if (!escaped && c == '\\')
            escaped = true;
          else
            escaped = false;
          continue;
        }
        if (c == '\'' || c == '"') {
          quoted = true;
          quote = c;
          continue;
        }
        if (c == '/' && i + 1 < content.size() && content[i + 1] == '/')
          break;
        if (c == '{' || c == '[' || c == '(')
          ++opens;
        else if (c == '}' || c == ']' || c == ')')
          ++endings;
      }
      depth = std::max(0, line_depth + opens - endings + (closes ? 1 : 0));
    }
  }
  return result;
}
