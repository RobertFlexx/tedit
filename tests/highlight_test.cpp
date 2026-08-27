#define main tedit_program_main
#include "../src/tedit.cpp"
#undef main

static void require_token(const string &output, const string &token) {
  if (output.find(token) == string::npos) {
    cerr << "missing highlight token: " << token << "\noutput: " << output
         << "\n";
    std::exit(1);
  }
}

static ThemePalette test_palette() {
  ThemePalette P;
  P.syntax_keyword = "<keyword>";
  P.syntax_control = "<control>";
  P.syntax_declaration = "<declaration>";
  P.syntax_type = "<type>";
  P.syntax_builtin = "<builtin>";
  P.syntax_function = "<function>";
  P.syntax_string = "<string>";
  P.syntax_number = "<number>";
  P.syntax_literal = "<literal>";
  P.syntax_comment = "<comment>";
  P.syntax_operator = "<operator>";
  P.syntax_preprocessor = "<preprocessor>";
  P.syntax_attribute = "<attribute>";
  P.syntax_key = "<key>";
  P.syntax_tag = "<tag>";
  P.syntax_selector = "<selector>";
  P.syntax_variable = "<variable>";
  P.syntax_punctuation = "<punctuation>";
  return P;
}

int main() {
  ThemePalette P = test_palette();
  HighlightState state;

  string cpp =
      highlight_line("if (Widget value = make(42)) return true; // note", P,
                     Lang::Cpp, state, true);
  require_token(cpp, "<control>if");
  require_token(cpp, "<type>Widget");
  require_token(cpp, "<function>make");
  require_token(cpp, "<number>42");
  require_token(cpp, "<literal>true");
  require_token(cpp, "<comment>// note");

  state = {};
  string python = highlight_line("@cache", P, Lang::Python, state, true);
  require_token(python, "<attribute>@cache");
  python =
      highlight_line("def render(value: str):", P, Lang::Python, state, true);
  require_token(python, "<declaration>def");
  require_token(python, "<function>render");
  require_token(python, "<type>str");
  highlight_line("\"\"\"first", P, Lang::Python, state, true);
  python = highlight_line("second\"\"\"", P, Lang::Python, state, true);
  require_token(python, "<string>second\"\"\"");

  state = {};
  string rust = highlight_line("#[derive(Debug)]", P, Lang::Rust, state, true);
  require_token(rust, "<attribute>#[derive(Debug)]");
  rust = highlight_line("let item: Option<u8> = vec![1];", P, Lang::Rust, state,
                        true);
  require_token(rust, "<declaration>let");
  require_token(rust, "<type>Option");
  require_token(rust, "<preprocessor>vec");
  rust = highlight_line("let letter: char = 'x';", P, Lang::Rust, state, true);
  require_token(rust, "<string>'x'");

  state = {};
  string sql = highlight_line("SELECT count(id) FROM users WHERE active = TRUE",
                              P, Lang::SQL, state, true);
  require_token(sql, "<declaration>SELECT");
  require_token(sql, "<builtin>count");
  require_token(sql, "<keyword>FROM");
  require_token(sql, "<control>WHERE");
  require_token(sql, "<literal>TRUE");

  state = {};
  string json = highlight_line("{\"enabled\": true, \"count\": 12}", P,
                               Lang::JSON, state, true);
  require_token(json, "<key>\"enabled\"");
  require_token(json, "<literal>true");
  require_token(json, "<number>12");

  state = {};
  string css = highlight_line("main.card { color: #abc; width: 10rem; }", P,
                              Lang::CSS, state, true);
  require_token(css, "<selector>main");
  require_token(css, "<key>color");
  require_token(css, "<literal>#abc");
  require_token(css, "<number>10rem");
  state = {};
  css = highlight_line("#abc:hover {", P, Lang::CSS, state, true);
  require_token(css, "<selector>abc");

  state = {};
  string html = highlight_line("<article class=\"card\">&amp;</article>", P,
                               Lang::HTML, state, true);
  require_token(html, "<tag>article");
  require_token(html, "<attribute>class");
  require_token(html, "<string>\"card\"");
  require_token(html, "<literal>&amp;");

  state = {};
  highlight_line("```python", P, Lang::Markdown, state, true);
  string fenced =
      highlight_line("def inside():", P, Lang::Markdown, state, true);
  require_token(fenced, "<declaration>def");
  require_token(fenced, "<function>inside");

  if (detect_lang("Dockerfile.dev") != Lang::Docker ||
      detect_lang("build.mk") != Lang::Make ||
      detect_lang("module.lua") != Lang::Lua ||
      detect_lang("change.patch") != Lang::Diff) {
    cerr << "extended language detection failed\n";
    return 1;
  }

  cout << "highlight tests passed\n";
  return 0;
}
