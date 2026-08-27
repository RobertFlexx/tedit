enum class Lang {
  Plain,
  Cpp,
  Python,
  Shell,
  Ruby,
  JS,
  HTML,
  CSS,
  JSON,
  Go,
  Rust,
  Java,
  CSharp,
  PHP,
  Markdown,
  YAML,
  TOML,
  SQL,
  XML,
  Swift,
  Kotlin,
  Lua,
  Perl,
  R,
  Make,
  Docker,
  Ini,
  Diff,
  Modula2,
  Modula3,
  ObjectPascal
};

static Lang detect_lang(const string &path) {
  fs::path file(path);
  string name = lower(file.filename().string());
  string ext = lower(file.extension().string());
  if (name == "makefile" || name == "gnumakefile" || name == "bsdmakefile" ||
      ext == ".mk")
    return Lang::Make;
  if (name == "dockerfile" || name.rfind("dockerfile.", 0) == 0 ||
      ext == ".dockerfile")
    return Lang::Docker;
  if (name == ".bashrc" || name == ".zshrc" || name == ".profile")
    return Lang::Shell;
  if (ext == ".c" || ext == ".cc" || ext == ".cpp" || ext == ".cxx" ||
      ext == ".h" || ext == ".hh" || ext == ".hpp" || ext == ".hxx")
    return Lang::Cpp;
  if (ext == ".py" || ext == ".pyi" || ext == ".pyw")
    return Lang::Python;
  if (ext == ".sh" || ext == ".bash" || ext == ".zsh" || ext == ".fish")
    return Lang::Shell;
  if (ext == ".rb" || ext == ".rake" || name == "gemfile")
    return Lang::Ruby;
  if (ext == ".js" || ext == ".mjs" || ext == ".cjs" || ext == ".jsx" ||
      ext == ".ts" || ext == ".tsx" || ext == ".vue" || ext == ".svelte")
    return Lang::JS;
  if (ext == ".html" || ext == ".htm")
    return Lang::HTML;
  if (ext == ".css" || ext == ".scss" || ext == ".sass" || ext == ".less")
    return Lang::CSS;
  if (ext == ".json" || ext == ".jsonc" || ext == ".geojson")
    return Lang::JSON;
  if (ext == ".go")
    return Lang::Go;
  if (ext == ".rs")
    return Lang::Rust;
  if (ext == ".java")
    return Lang::Java;
  if (ext == ".cs")
    return Lang::CSharp;
  if (ext == ".php" || ext == ".phtml")
    return Lang::PHP;
  if (ext == ".md" || ext == ".markdown" || ext == ".mdx")
    return Lang::Markdown;
  if (ext == ".yaml" || ext == ".yml")
    return Lang::YAML;
  if (ext == ".toml")
    return Lang::TOML;
  if (ext == ".sql")
    return Lang::SQL;
  if (ext == ".xml" || ext == ".svg" || ext == ".xsl" || ext == ".xsd")
    return Lang::XML;
  if (ext == ".swift")
    return Lang::Swift;
  if (ext == ".kt" || ext == ".kts")
    return Lang::Kotlin;
  if (ext == ".lua")
    return Lang::Lua;
  if (ext == ".pl" || ext == ".pm")
    return Lang::Perl;
  if (ext == ".r" || ext == ".rmd")
    return Lang::R;
  if (ext == ".ini" || ext == ".cfg" || ext == ".conf" ||
      ext == ".properties" || ext == ".env")
    return Lang::Ini;
  if (ext == ".diff" || ext == ".patch")
    return Lang::Diff;
  if (ext == ".m2" || ext == ".mod" || ext == ".def")
    return Lang::Modula2;
  if (ext == ".m3" || ext == ".mg" || ext == ".ig")
    return Lang::Modula3;
  if (ext == ".pas" || ext == ".lpr" || ext == ".pp" || ext == ".inc")
    return Lang::ObjectPascal;
  return Lang::Plain;
}

enum class SyntaxKind {
  Plain,
  Keyword,
  Control,
  Declaration,
  Type,
  Builtin,
  Function,
  String,
  Number,
  Literal,
  Comment,
  Operator,
  Preprocessor,
  Attribute,
  Key,
  Tag,
  Selector,
  Variable,
  Punctuation
};

struct HighlightState {
  bool block_comment = false;
  string block_end;
  bool multiline_string = false;
  string string_end;
  bool markdown_fence = false;
  Lang embedded_lang = Lang::Plain;
  int css_depth = 0;
};

using WordSet = std::unordered_set<string>;
struct LanguageProfile {
  WordSet control, declaration, type, builtin, literal, keyword;
  bool case_insensitive = false;
};

static WordSet word_set(const char *list) {
  WordSet result;
  std::istringstream in(list);
  string word;
  while (in >> word)
    result.insert(word);
  return result;
}

static LanguageProfile make_profile(const char *control,
                                    const char *declaration, const char *type,
                                    const char *builtin, const char *literal,
                                    const char *keyword,
                                    bool case_insensitive = false) {
  return {word_set(control), word_set(declaration), word_set(type),
          word_set(builtin), word_set(literal),     word_set(keyword),
          case_insensitive};
}

static const LanguageProfile &language_profile(Lang lang) {
  static const LanguageProfile plain;
  static const LanguageProfile cpp = make_profile(
      "break case catch continue default do else for goto if return switch "
      "throw try while",
      "class concept enum namespace struct template typedef union using",
      "auto bool char char8_t char16_t char32_t double float int long short "
      "signed size_t unsigned void wchar_t",
      "alignof decltype dynamic_cast malloc free move forward printf scanf "
      "static_cast reinterpret_cast const_cast",
      "false nullptr true NULL",
      "alignas asm const consteval constexpr constinit explicit export extern "
      "final friend inline mutable new noexcept operator override private "
      "protected public register requires static thread_local typename virtual "
      "volatile delete this sizeof");
  static const LanguageProfile python = make_profile(
      "and assert async await break continue elif else except finally for if "
      "in is not or raise return try while with yield",
      "class def import from global lambda nonlocal",
      "bytes bytearray complex dict float frozenset int list memoryview object "
      "range set slice str tuple type",
      "abs all any bin bool breakpoint callable chr compile delattr dir divmod "
      "enumerate eval exec filter format getattr globals hasattr hash help hex "
      "id input isinstance issubclass iter len locals map max min next oct "
      "open ord pow print property repr reversed round setattr sorted sum "
      "super vars zip __name__",
      "False None NotImplemented True Ellipsis", "as del match pass case");
  static const LanguageProfile shell = make_profile(
      "case do done elif else esac fi for if in select then until while",
      "function", "",
      "alias bg bind break builtin caller cd command compgen complete declare "
      "dirs disown echo enable eval exec exit export fc fg getopts hash help "
      "history jobs kill let local logout mapfile popd printf pushd pwd read "
      "readonly return set shift source suspend test times trap type typeset "
      "ulimit umask unalias unset wait",
      "true false", "coproc time");
  static const LanguageProfile ruby = make_profile(
      "begin break case do else elsif end ensure for if next redo rescue retry "
      "return then unless until when while yield",
      "class def module",
      "Array BasicObject Binding Class Dir Encoding Enumerator Exception "
      "FalseClass Fiber File Hash Integer IO MatchData Method Module NilClass "
      "Numeric Object Proc Range Regexp String Struct Symbol Thread Time "
      "TrueClass",
      "attr_accessor attr_reader attr_writer catch exit extend fail format "
      "gets include lambda load loop open p print printf proc putc puts raise "
      "rand read require select sleep sprintf srand system throw warn",
      "false nil self true",
      "alias and defined? not or super undef __FILE__ __LINE__ __ENCODING__");
  static const LanguageProfile js = make_profile(
      "break case catch continue default do else finally for if return switch "
      "throw try while with yield await",
      "class const export function import interface let namespace type var "
      "enum implements package private protected public",
      "any bigint boolean never number object string symbol unknown void Array "
      "Promise Record Partial Readonly",
      "Array BigInt Boolean Date Error Function JSON Map Math Number Object "
      "Promise Proxy Reflect RegExp Set String Symbol WeakMap WeakSet console "
      "document globalThis process window parseFloat parseInt setInterval "
      "setTimeout clearInterval clearTimeout",
      "false Infinity NaN null true undefined",
      "as async delete extends from get in instanceof keyof new of readonly "
      "set static super this typeof");
  static const LanguageProfile go =
      make_profile("break case continue default defer else fallthrough for go "
                   "goto if range return select switch",
                   "const func import interface map package struct type var",
                   "any bool byte comparable complex128 complex64 error "
                   "float32 float64 int int16 int32 int64 int8 rune string "
                   "uint uint16 uint32 uint64 uint8 uintptr",
                   "append cap clear close complex copy delete imag len make "
                   "max min new panic print println real recover",
                   "false iota nil true", "chan");
  static const LanguageProfile rust = make_profile(
      "break continue else for if in loop match return while await",
      "const enum extern fn impl let mod static struct trait type union use",
      "bool char f32 f64 i8 i16 i32 i64 i128 isize str u8 u16 u32 u64 u128 "
      "usize Self String Vec Option Result Box",
      "assert dbg drop format include_bytes include_str matches panic print "
      "println todo unreachable vec",
      "false None self Self true",
      "as async crate dyn move mut pub ref super unsafe where abstract become "
      "box do final macro offsetof override priv typeof unsized virtual yield");
  static const LanguageProfile java = make_profile(
      "break case catch continue default do else finally for if return switch "
      "throw try while yield",
      "class enum import interface package record",
      "boolean byte char double float int long short void Boolean Byte "
      "Character Double Float Integer Long Object Short String Void",
      "Math Objects System Arrays Collections List Map Optional Set Stream",
      "false null true",
      "abstract assert const extends final implements instanceof native new "
      "non-sealed permits private protected public sealed static strictfp "
      "super synchronized this throws transient var volatile");
  static const LanguageProfile csharp = make_profile(
      "break case catch continue default do else finally for foreach if return "
      "switch throw try while yield await",
      "class delegate enum event interface namespace record struct",
      "bool byte char decimal double dynamic float int long nint nuint object "
      "sbyte short string uint ulong ushort void",
      "Console Convert DateTime Dictionary Enumerable Environment Exception "
      "Guid List Math String Task Tuple",
      "false null true",
      "abstract as async base checked const explicit extern fixed implicit in "
      "internal is lock new operator out override params partial private "
      "protected public readonly ref required scoped sealed sizeof stackalloc "
      "static this typeof unchecked unsafe using virtual volatile when where");
  static const LanguageProfile php = make_profile(
      "break case catch continue default do else elseif endfor endforeach "
      "endif endswitch endwhile finally for foreach if match return switch "
      "throw try while yield",
      "class enum function interface namespace trait",
      "array bool callable float int iterable mixed never object string void",
      "array_count_values array_filter array_map array_merge array_pop "
      "array_push count date define empty explode implode in_array is_array "
      "isset json_decode json_encode print_r sprintf strlen strpos trim "
      "var_dump",
      "false null true",
      "abstract and as clone const declare echo extends final fn global "
      "implements include include_once instanceof new or private protected "
      "public readonly require require_once static use var xor");
  static const LanguageProfile swift = make_profile(
      "break case catch continue default defer do else fallthrough for guard "
      "if in repeat return switch throw try where while",
      "associatedtype class deinit enum extension func import init operator "
      "precedencegroup protocol struct subscript typealias",
      "Any AnyObject Array Bool Character Dictionary Double Error Float Int "
      "Never Optional Result Set String UInt Void",
      "abs assert debugPrint fatalError max min precondition print readLine "
      "stride type",
      "false nil self Self super true",
      "as assignment associativity convenience didSet dynamic fileprivate "
      "final get indirect infix inout internal is lazy left let mutating "
      "nonmutating open optional override postfix precedence prefix private "
      "public required rethrows right set some static unowned var weak "
      "willSet");
  static const LanguageProfile kotlin = make_profile(
      "break catch continue do else finally for if return throw try when while",
      "class enum fun import interface object package typealias",
      "Any Boolean Byte Char Double Float Int Long Nothing Short String UInt "
      "ULong Unit",
      "arrayOf check error lazy listOf mapOf mutableListOf mutableMapOf print "
      "println require run setOf sequenceOf",
      "false null this super true",
      "as by companion const constructor crossinline data delegate dynamic "
      "expect external field file final get in infix init inline inner "
      "internal is lateinit noinline open operator out override private "
      "protected public reified sealed set suspend tailrec val var vararg "
      "where");
  static const LanguageProfile lua = make_profile(
      "break do else elseif end for goto if in repeat return then until while",
      "function local", "",
      "assert collectgarbage coroutine debug dofile error getmetatable ipairs "
      "io load loadfile math next os package pairs pcall print rawequal rawget "
      "rawlen rawset require select setmetatable string table tonumber "
      "tostring type utf8 xpcall",
      "false nil true", "and const not or self");
  static const LanguageProfile perl = make_profile(
      "continue do else elsif for foreach given goto if last next redo return "
      "unless until when while",
      "format package sub use require", "",
      "chdir chmod chomp chop close die each eof eval exec exists exit grep "
      "join keys kill length map open pop print printf push read readdir "
      "readline reverse scalar shift sort splice split sprintf system tell "
      "time unlink unshift values warn",
      "undef", "and cmp eq ge gt le lt ne no not or state our local my xor");
  static const LanguageProfile r = make_profile(
      "break else for if in next repeat return while",
      "function library require",
      "character complex double integer list logical matrix numeric raw",
      "abs c cat data.frame dim length library max mean min names ncol nrow "
      "paste print read.csv rep require sd seq sort source str subset summary "
      "table typeof",
      "FALSE Inf NA NaN NULL TRUE", "function");
  static const LanguageProfile sql = make_profile(
      "begin case commit else end except having if loop rollback then when "
      "where while",
      "alter create delete drop insert merge replace select truncate update",
      "bigint binary bit blob boolean char date datetime decimal double enum "
      "float int integer json numeric real serial smallint text time timestamp "
      "uuid varchar",
      "avg cast coalesce concat count current_date current_time "
      "current_timestamp extract greatest least lower max min nullif round "
      "row_number sum trim upper",
      "false null true unknown",
      "all and any as asc between by constraint cross desc distinct exists "
      "foreign from full group in index inner into is join left like limit not "
      "offset on or order outer primary references right table union unique "
      "values view with",
      true);
  static const LanguageProfile make = make_profile(
      "else endif ifdef ifeq ifndef ifneq",
      "define endef export include override private undefine unexport vpath",
      "",
      "addprefix addsuffix basename call dir error eval file filter filter-out "
      "findstring firstword flavor foreach guile if info join lastword notdir "
      "origin patsubst realpath shell sort strip subst suffix value warning "
      "wildcard word wordlist words",
      "", "phony");
  static const LanguageProfile docker = make_profile(
      "",
      "add arg cmd copy entrypoint env expose from healthcheck label "
      "maintainer onbuild run shell stopsignal user volume workdir",
      "", "", "", "as", true);
  static const LanguageProfile modula2 = make_profile(
      "case else elsif end exit for if loop repeat return then until while",
      "definition export forward from import implementation module procedure "
      "qualified unqualified",
      "address array bitset boolean cardinal char comp complex "
      "longcomplex longint longreal pointer process proctype real "
      "signal word",
      "abs assert cap chr create dispose eval halt high inc dec incl "
      "excl int len lfloat long max min new odd ord read readChar "
      "readInt readLongReal readCard readReal readString size "
      "terminate trunc val wait write writeChar writeInt writeLongInt "
      "writeLongReal writeCard writeReal writeString writeLn "
      "open close _done _exit _offset _length",
      "false true nil zero first last firstBit lastBit maxCard minCard "
      "maxInt minInt maxReal minReal maxLongReal minLongReal",
      "and array begin by const div do export in mod not of or "
      "pointer record set to type var with packedset wordsize",
      true);
  static const LanguageProfile modula3 = make_profile(
      "case else elsif end exit for if loop repeat return then try "
      "except finally lock while",
      "export from import interface methods module overrides procedure "
      "reveal",
      "any array bits branded byte cardinal char extended integer "
      "longcard longreal mutex object packedset procedure range ref "
      "refany root set subrange text thread untraced word",
      "abs address adr adrsize bitsize card ceiling dec dispose "
      "float floor inc istype loophole max min mod narrow new "
      "number ord round subarray trunc typecode val",
      "false true nil zero first last maxCard maxInt maxReal "
      "maxLongReal unknown",
      "and array begin by const div do eval for in mod not of or "
      "readonly record set to type typecase value var with",
      true);
  static const LanguageProfile objectpascal = make_profile(
      "case do else except finally for if repeat then until while",
      "class constructor destructor interface library program "
      "procedure record unit uses var",
      "boolean byte cardinal char double extended integer int64 longint "
      "longword pointer pchar real shortint single string sustained "
      "text widechar widestring word",
      "abs addr alignof arc4random assertTrue chr close concat copy "
      "count decode dim encode eof eoln exp false fileExists format "
      "frac getMem heapSize high index initInstance isInList length "
      "ln low maxInt maxSize min mod now null pos power pred random "
      "read readLn release round setLength setSize showGUIObject "
      "sizeof squeeze str subString succ true trunc typedFill val "
      "write writeLn",
      "false nil true", "and array asm begin cdecl class const constructor "
      "destructor destructor div do downto dynamic else end except exported "
      "external far file finalization finalize finally function goto if "
      "implementation inherited initialization inline interface is label "
      "mod near nil not object of on or packed pascal private procedure "
      "program public raise record registered repeat resourcestring set "
      "shl shr stdcall string then threadvar to try type unit until uses "
      "var while with xor",
      true);

  switch (lang) {
  case Lang::Cpp:
    return cpp;
  case Lang::Python:
    return python;
  case Lang::Shell:
    return shell;
  case Lang::Ruby:
    return ruby;
  case Lang::JS:
    return js;
  case Lang::Go:
    return go;
  case Lang::Rust:
    return rust;
  case Lang::Java:
    return java;
  case Lang::CSharp:
    return csharp;
  case Lang::PHP:
    return php;
  case Lang::Swift:
    return swift;
  case Lang::Kotlin:
    return kotlin;
  case Lang::Lua:
    return lua;
  case Lang::Perl:
    return perl;
  case Lang::R:
    return r;
  case Lang::SQL:
    return sql;
  case Lang::Make:
    return make;
  case Lang::Docker:
    return docker;
  case Lang::Modula2:
    return modula2;
  case Lang::Modula3:
    return modula3;
  case Lang::ObjectPascal:
    return objectpascal;
  default:
    return plain;
  }
}

static bool identifier_start(unsigned char c) {
  return std::isalpha(c) || c == '_';
}
static bool identifier_continue(unsigned char c) {
  return std::isalnum(c) || c == '_';
}

static const string &syntax_color(const ThemePalette &P, SyntaxKind kind) {
  switch (kind) {
  case SyntaxKind::Keyword:
    return P.syntax_keyword;
  case SyntaxKind::Control:
    return P.syntax_control;
  case SyntaxKind::Declaration:
    return P.syntax_declaration;
  case SyntaxKind::Type:
    return P.syntax_type;
  case SyntaxKind::Builtin:
    return P.syntax_builtin;
  case SyntaxKind::Function:
    return P.syntax_function;
  case SyntaxKind::String:
    return P.syntax_string;
  case SyntaxKind::Number:
    return P.syntax_number;
  case SyntaxKind::Literal:
    return P.syntax_literal;
  case SyntaxKind::Comment:
    return P.syntax_comment;
  case SyntaxKind::Operator:
    return P.syntax_operator;
  case SyntaxKind::Preprocessor:
    return P.syntax_preprocessor;
  case SyntaxKind::Attribute:
    return P.syntax_attribute;
  case SyntaxKind::Key:
    return P.syntax_key;
  case SyntaxKind::Tag:
    return P.syntax_tag;
  case SyntaxKind::Selector:
    return P.syntax_selector;
  case SyntaxKind::Variable:
    return P.syntax_variable;
  case SyntaxKind::Punctuation:
    return P.syntax_punctuation;
  default:
    return P.help_text;
  }
}

static void emit_syntax(string &out, const string &text, SyntaxKind kind,
                        const ThemePalette &P, bool paint) {
  if (!paint || kind == SyntaxKind::Plain)
    out += text;
  else
    out += syntax_color(P, kind) + text + C_RESET;
}

static size_t next_nonspace(const string &line, size_t from) {
  while (from < line.size() && std::isspace((unsigned char)line[from]))
    ++from;
  return from;
}
static size_t previous_nonspace(const string &line, size_t from) {
  while (from > 0 && std::isspace((unsigned char)line[from - 1]))
    --from;
  return from;
}
static bool has_word(const WordSet &set, const string &word) {
  return set.find(word) != set.end();
}

static SyntaxKind classify_word(const string &word, const string &lookup,
                                const LanguageProfile &profile, Lang lang,
                                const string &line, size_t start, size_t end,
                                bool expect_name) {
  if (expect_name)
    return SyntaxKind::Function;
  if (has_word(profile.control, lookup))
    return SyntaxKind::Control;
  if (has_word(profile.declaration, lookup))
    return SyntaxKind::Declaration;
  if (has_word(profile.type, lookup))
    return SyntaxKind::Type;
  if (has_word(profile.builtin, lookup))
    return SyntaxKind::Builtin;
  if (has_word(profile.literal, lookup))
    return SyntaxKind::Literal;
  if (has_word(profile.keyword, lookup))
    return SyntaxKind::Keyword;
  size_t next = next_nonspace(line, end), prev = previous_nonspace(line, start);
  if (next < line.size() && line[next] == '(')
    return SyntaxKind::Function;
  if (prev > 0 &&
      (line[prev - 1] == '.' || (prev > 1 && line.substr(prev - 2, 2) == "->")))
    return SyntaxKind::Attribute;
  if (!word.empty() && std::isupper((unsigned char)word[0]) &&
      (lang == Lang::Cpp || lang == Lang::JS || lang == Lang::Rust ||
       lang == Lang::Java || lang == Lang::CSharp || lang == Lang::PHP ||
       lang == Lang::Swift || lang == Lang::Kotlin ||
       lang == Lang::Modula2 || lang == Lang::Modula3 ||
       lang == Lang::ObjectPascal)) {
    bool all_upper = true;
    for (unsigned char c : word)
      if (std::isalpha(c) && !std::isupper(c)) {
        all_upper = false;
        break;
      }
    return all_upper ? SyntaxKind::Literal : SyntaxKind::Type;
  }
  return SyntaxKind::Plain;
}

static bool line_comment_at(const string &line, size_t i, Lang lang) {
  if ((lang == Lang::Cpp || lang == Lang::JS || lang == Lang::Go ||
       lang == Lang::Rust || lang == Lang::Java || lang == Lang::CSharp ||
       lang == Lang::PHP || lang == Lang::Swift || lang == Lang::Kotlin ||
       lang == Lang::ObjectPascal) &&
      line.compare(i, 2, "//") == 0)
    return true;
  if ((lang == Lang::Python || lang == Lang::Shell || lang == Lang::Ruby ||
       lang == Lang::Perl || lang == Lang::R || lang == Lang::Make) &&
      line[i] == '#')
    return true;
  if ((lang == Lang::SQL || lang == Lang::Lua) && line.compare(i, 2, "--") == 0)
    return true;
  return lang == Lang::PHP && line[i] == '#';
}

static bool starts_block_comment(const string &line, size_t i, Lang lang,
                                 string &end) {
  if ((lang == Lang::Cpp || lang == Lang::JS || lang == Lang::Go ||
       lang == Lang::Rust || lang == Lang::Java || lang == Lang::CSharp ||
       lang == Lang::PHP || lang == Lang::Swift || lang == Lang::Kotlin ||
       lang == Lang::CSS) &&
      line.compare(i, 2, "/*") == 0) {
    end = "*/";
    return true;
  }
  if ((lang == Lang::Modula2 || lang == Lang::Modula3 ||
       lang == Lang::ObjectPascal) &&
      line.compare(i, 2, "(*") == 0) {
    end = "*)";
    return true;
  }
  if (lang == Lang::Lua && line.compare(i, 4, "--[[") == 0) {
    end = "]]";
    return true;
  }
  return false;
}

static size_t scan_quoted(const string &line, size_t start, char quote) {
  size_t i = start + 1;
  bool escaped = false;
  while (i < line.size()) {
    char c = line[i++];
    if (!escaped && c == quote)
      break;
    if (!escaped && c == '\\')
      escaped = true;
    else
      escaped = false;
  }
  return i;
}

static size_t scan_number(const string &line, size_t start) {
  size_t i = start;
  if (line[i] == '.')
    ++i;
  while (i < line.size()) {
    unsigned char c = (unsigned char)line[i];
    if (std::isalnum(c) || line[i] == '_' || line[i] == '.' ||
        line[i] == '\'') {
      ++i;
      continue;
    }
    if ((line[i] == '+' || line[i] == '-') && i > start &&
        (line[i - 1] == 'e' || line[i - 1] == 'E' || line[i - 1] == 'p' ||
         line[i - 1] == 'P')) {
      ++i;
      continue;
    }
    break;
  }
  return i;
}

static Lang lang_from_fence(string name) {
  name = lower(trim_copy(name));
  size_t end = name.find_first_of(" \t{,");
  if (end != string::npos)
    name.resize(end);
  if (name == "c" || name == "cc" || name == "cpp" || name == "c++")
    return Lang::Cpp;
  if (name == "py" || name == "python")
    return Lang::Python;
  if (name == "sh" || name == "bash" || name == "zsh")
    return Lang::Shell;
  if (name == "rb" || name == "ruby")
    return Lang::Ruby;
  if (name == "js" || name == "javascript" || name == "ts" ||
      name == "typescript" || name == "jsx" || name == "tsx")
    return Lang::JS;
  if (name == "html")
    return Lang::HTML;
  if (name == "css" || name == "scss")
    return Lang::CSS;
  if (name == "json")
    return Lang::JSON;
  if (name == "go")
    return Lang::Go;
  if (name == "rs" || name == "rust")
    return Lang::Rust;
  if (name == "java")
    return Lang::Java;
  if (name == "cs" || name == "csharp")
    return Lang::CSharp;
  if (name == "php")
    return Lang::PHP;
  if (name == "yaml" || name == "yml")
    return Lang::YAML;
  if (name == "toml")
    return Lang::TOML;
  if (name == "sql")
    return Lang::SQL;
  if (name == "xml" || name == "svg")
    return Lang::XML;
  if (name == "swift")
    return Lang::Swift;
  if (name == "kt" || name == "kotlin")
    return Lang::Kotlin;
  if (name == "lua")
    return Lang::Lua;
  if (name == "pl" || name == "perl")
    return Lang::Perl;
  if (name == "r")
    return Lang::R;
  if (name == "make" || name == "makefile")
    return Lang::Make;
  if (name == "docker" || name == "dockerfile")
    return Lang::Docker;
  if (name == "modula2" || name == "mod2" || name == "m2")
    return Lang::Modula2;
  if (name == "modula3" || name == "mod3" || name == "m3")
    return Lang::Modula3;
  if (name == "pascal" || name == "delphi" || name == "objectpascal")
    return Lang::ObjectPascal;
  return Lang::Plain;
}

static string highlight_code(const string &, const ThemePalette &, Lang,
                             HighlightState &, bool);

static string highlight_markdown(const string &line, const ThemePalette &P,
                                 HighlightState &state, bool paint) {
  string trimmed = trim_copy(line);
  if (trimmed.rfind("```", 0) == 0 || trimmed.rfind("~~~", 0) == 0) {
    if (state.markdown_fence) {
      state.markdown_fence = false;
      state.block_comment = false;
      state.multiline_string = false;
    } else {
      state.markdown_fence = true;
      state.embedded_lang = lang_from_fence(trimmed.substr(3));
    }
    string out;
    emit_syntax(out, line, SyntaxKind::Preprocessor, P, paint);
    return out;
  }
  if (state.markdown_fence) {
    if (state.embedded_lang != Lang::Plain)
      return highlight_code(line, P, state.embedded_lang, state, paint);
    string out;
    emit_syntax(out, line, SyntaxKind::String, P, paint);
    return out;
  }
  string out;
  size_t i = 0, first = next_nonspace(line, 0);
  if (first < line.size() && line[first] == '#') {
    size_t end = first;
    while (end < line.size() && line[end] == '#')
      ++end;
    if (end < line.size() && std::isspace((unsigned char)line[end])) {
      emit_syntax(out, line.substr(0, first), SyntaxKind::Plain, P, paint);
      emit_syntax(out, line.substr(first), SyntaxKind::Tag, P, paint);
      return out;
    }
  }
  while (i < line.size()) {
    if (line.compare(i, 4, "<!--") == 0) {
      size_t end = line.find("-->", i + 4);
      end = end == string::npos ? line.size() : end + 3;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Comment, P, paint);
      i = end;
      continue;
    }
    if (line[i] == '`') {
      size_t ticks = 1;
      while (i + ticks < line.size() && line[i + ticks] == '`')
        ++ticks;
      string marker(ticks, '`');
      size_t end = line.find(marker, i + ticks);
      end = end == string::npos ? line.size() : end + ticks;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::String, P, paint);
      i = end;
      continue;
    }
    if (line[i] == '[') {
      size_t mid = line.find("](", i + 1),
             end = mid == string::npos ? string::npos : line.find(')', mid + 2);
      if (end != string::npos) {
        emit_syntax(out, line.substr(i, end - i + 1), SyntaxKind::Attribute, P,
                    paint);
        i = end + 1;
        continue;
      }
    }
    if ((line[i] == '*' || line[i] == '_') && i + 1 < line.size()) {
      char mark = line[i];
      size_t count = line[i + 1] == mark ? 2 : 1;
      string marker(count, mark);
      size_t end = line.find(marker, i + count);
      if (end != string::npos) {
        end += count;
        emit_syntax(out, line.substr(i, end - i), SyntaxKind::Keyword, P,
                    paint);
        i = end;
        continue;
      }
    }
    if (i == first && (line[i] == '>' || line[i] == '-' || line[i] == '*' ||
                       line[i] == '+')) {
      size_t end = i + 1;
      if (end < line.size() && std::isspace((unsigned char)line[end])) {
        emit_syntax(out, line.substr(i, end - i), SyntaxKind::Punctuation, P,
                    paint);
        i = end;
        continue;
      }
    }
    out.push_back(line[i++]);
  }
  return out;
}

static string highlight_markup(const string &line, const ThemePalette &P,
                               HighlightState &state, bool paint) {
  string out;
  size_t i = 0;
  if (state.block_comment) {
    size_t end = line.find("-->");
    if (end == string::npos) {
      emit_syntax(out, line, SyntaxKind::Comment, P, paint);
      return out;
    }
    emit_syntax(out, line.substr(0, end + 3), SyntaxKind::Comment, P, paint);
    i = end + 3;
    state.block_comment = false;
  }
  while (i < line.size()) {
    if (line.compare(i, 4, "<!--") == 0) {
      size_t end = line.find("-->", i + 4);
      if (end == string::npos) {
        emit_syntax(out, line.substr(i), SyntaxKind::Comment, P, paint);
        state.block_comment = true;
        break;
      }
      end += 3;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Comment, P, paint);
      i = end;
      continue;
    }
    if (line[i] == '&') {
      size_t end = line.find(';', i + 1);
      if (end != string::npos && end - i < 20) {
        emit_syntax(out, line.substr(i, end - i + 1), SyntaxKind::Literal, P,
                    paint);
        i = end + 1;
        continue;
      }
    }
    if (line[i] != '<') {
      out.push_back(line[i++]);
      continue;
    }
    if (line.compare(i, 2, "<!") == 0 || line.compare(i, 2, "<?") == 0) {
      size_t end = line.find('>', i + 2);
      end = end == string::npos ? line.size() : end + 1;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Preprocessor, P,
                  paint);
      i = end;
      continue;
    }
    emit_syntax(out, "<", SyntaxKind::Punctuation, P, paint);
    ++i;
    if (i < line.size() && line[i] == '/') {
      emit_syntax(out, "/", SyntaxKind::Punctuation, P, paint);
      ++i;
    }
    size_t tag = i;
    while (i < line.size() && (identifier_continue((unsigned char)line[i]) ||
                               line[i] == '-' || line[i] == ':'))
      ++i;
    emit_syntax(out, line.substr(tag, i - tag), SyntaxKind::Tag, P, paint);
    while (i < line.size() && line[i] != '>') {
      if (std::isspace((unsigned char)line[i])) {
        out.push_back(line[i++]);
        continue;
      }
      if (line[i] == '/') {
        emit_syntax(out, "/", SyntaxKind::Punctuation, P, paint);
        ++i;
        continue;
      }
      size_t key = i;
      while (i < line.size() &&
             (identifier_continue((unsigned char)line[i]) || line[i] == '-' ||
              line[i] == ':' || line[i] == '.'))
        ++i;
      if (i > key) {
        emit_syntax(out, line.substr(key, i - key), SyntaxKind::Attribute, P,
                    paint);
        continue;
      }
      if (line[i] == '=') {
        emit_syntax(out, "=", SyntaxKind::Operator, P, paint);
        ++i;
        continue;
      }
      if (line[i] == '\'' || line[i] == '"') {
        size_t end = scan_quoted(line, i, line[i]);
        emit_syntax(out, line.substr(i, end - i), SyntaxKind::String, P, paint);
        i = end;
        continue;
      }
      out.push_back(line[i++]);
    }
    if (i < line.size()) {
      emit_syntax(out, ">", SyntaxKind::Punctuation, P, paint);
      ++i;
    }
  }
  return out;
}

static string highlight_config(const string &line, const ThemePalette &P,
                               Lang lang, bool paint) {
  string out;
  size_t i = 0, first = next_nonspace(line, 0);
  if ((lang == Lang::TOML || lang == Lang::Ini) && first < line.size() &&
      line[first] == '[') {
    emit_syntax(out, line.substr(0, first), SyntaxKind::Plain, P, paint);
    emit_syntax(out, line.substr(first), SyntaxKind::Tag, P, paint);
    return out;
  }
  while (i < line.size()) {
    if ((lang == Lang::JSON && line.compare(i, 2, "//") == 0) ||
        ((lang == Lang::YAML || lang == Lang::TOML || lang == Lang::Ini) &&
         (line[i] == '#' || line[i] == ';'))) {
      emit_syntax(out, line.substr(i), SyntaxKind::Comment, P, paint);
      break;
    }
    if (line[i] == '"' || line[i] == '\'') {
      size_t end = scan_quoted(line, i, line[i]),
             next = next_nonspace(line, end);
      SyntaxKind kind =
          (next < line.size() && (line[next] == ':' || line[next] == '='))
              ? SyntaxKind::Key
              : SyntaxKind::String;
      emit_syntax(out, line.substr(i, end - i), kind, P, paint);
      i = end;
      continue;
    }
    if (identifier_start((unsigned char)line[i])) {
      size_t end = i + 1;
      while (end < line.size() &&
             (identifier_continue((unsigned char)line[end]) ||
              line[end] == '-' || line[end] == '.'))
        ++end;
      string word = line.substr(i, end - i), low = lower(word);
      size_t next = next_nonspace(line, end);
      SyntaxKind kind = SyntaxKind::Plain;
      if (low == "true" || low == "false" || low == "null" || low == "yes" ||
          low == "no" || low == "on" || low == "off")
        kind = SyntaxKind::Literal;
      else if (next < line.size() && (line[next] == ':' || line[next] == '='))
        kind = SyntaxKind::Key;
      emit_syntax(out, word, kind, P, paint);
      i = end;
      continue;
    }
    if (std::isdigit((unsigned char)line[i]) ||
        (line[i] == '-' && i + 1 < line.size() &&
         std::isdigit((unsigned char)line[i + 1]))) {
      size_t end = i + 1;
      while (end < line.size() &&
             (std::isalnum((unsigned char)line[end]) ||
              string("._:+-").find(line[end]) != string::npos))
        ++end;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Number, P, paint);
      i = end;
      continue;
    }
    if (lang == Lang::YAML &&
        (line[i] == '&' || line[i] == '*' || line[i] == '!')) {
      size_t end = i + 1;
      while (end < line.size() && !std::isspace((unsigned char)line[end]) &&
             string(",]}:").find(line[end]) == string::npos)
        ++end;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Attribute, P,
                  paint);
      i = end;
      continue;
    }
    if (string("{}[]:,=").find(line[i]) != string::npos)
      emit_syntax(out, string(1, line[i]), SyntaxKind::Punctuation, P, paint);
    else
      out.push_back(line[i]);
    ++i;
  }
  return out;
}

static string highlight_css(const string &line, const ThemePalette &P,
                            HighlightState &state, bool paint) {
  string out;
  size_t i = 0;
  while (i < line.size()) {
    if (state.block_comment) {
      size_t end = line.find("*/", i);
      if (end == string::npos) {
        emit_syntax(out, line.substr(i), SyntaxKind::Comment, P, paint);
        return out;
      }
      end += 2;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Comment, P, paint);
      i = end;
      state.block_comment = false;
      continue;
    }
    if (line.compare(i, 2, "/*") == 0) {
      size_t end = line.find("*/", i + 2);
      if (end == string::npos) {
        state.block_comment = true;
        emit_syntax(out, line.substr(i), SyntaxKind::Comment, P, paint);
        return out;
      }
      end += 2;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Comment, P, paint);
      i = end;
      continue;
    }
    if (line[i] == '\'' || line[i] == '"') {
      size_t end = scan_quoted(line, i, line[i]);
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::String, P, paint);
      i = end;
      continue;
    }
    if (state.css_depth > 0 && line[i] == '#' && i + 1 < line.size() &&
        std::isxdigit((unsigned char)line[i + 1])) {
      size_t end = i + 1;
      while (end < line.size() && std::isxdigit((unsigned char)line[end]))
        ++end;
      if (end - i == 4 || end - i == 5 || end - i == 7 || end - i == 9) {
        emit_syntax(out, line.substr(i, end - i), SyntaxKind::Literal, P,
                    paint);
        i = end;
        continue;
      }
    }
    if (line[i] == '@') {
      size_t end = i + 1;
      while (
          end < line.size() &&
          (identifier_continue((unsigned char)line[end]) || line[end] == '-'))
        ++end;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Preprocessor, P,
                  paint);
      i = end;
      continue;
    }
    if (identifier_start((unsigned char)line[i]) ||
        line.compare(i, 2, "--") == 0) {
      size_t end = i + 1;
      while (
          end < line.size() &&
          (identifier_continue((unsigned char)line[end]) || line[end] == '-'))
        ++end;
      size_t next = next_nonspace(line, end);
      SyntaxKind kind = SyntaxKind::Selector;
      if (state.css_depth > 0 && next < line.size() && line[next] == ':')
        kind = SyntaxKind::Key;
      else if (state.css_depth > 0 && next < line.size() && line[next] == '(')
        kind = SyntaxKind::Function;
      emit_syntax(out, line.substr(i, end - i), kind, P, paint);
      i = end;
      continue;
    }
    if (std::isdigit((unsigned char)line[i]) ||
        (line[i] == '.' && i + 1 < line.size() &&
         std::isdigit((unsigned char)line[i + 1]))) {
      size_t end = i + 1;
      while (end < line.size() && (std::isalnum((unsigned char)line[end]) ||
                                   line[end] == '.' || line[end] == '%'))
        ++end;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Number, P, paint);
      i = end;
      continue;
    }
    if (line[i] == '{')
      ++state.css_depth;
    else if (line[i] == '}' && state.css_depth > 0)
      --state.css_depth;
    SyntaxKind kind = string("{}[]():;,").find(line[i]) != string::npos
                          ? SyntaxKind::Punctuation
                      : string("+>~=|*").find(line[i]) != string::npos
                          ? SyntaxKind::Operator
                          : SyntaxKind::Plain;
    emit_syntax(out, string(1, line[i]), kind, P, paint);
    ++i;
  }
  return out;
}

static string highlight_diff(const string &line, const ThemePalette &P,
                             bool paint) {
  string out;
  if (line.rfind("+++", 0) == 0 || line.rfind("---", 0) == 0 ||
      line.rfind("diff ", 0) == 0 || line.rfind("index ", 0) == 0)
    emit_syntax(out, line, SyntaxKind::Preprocessor, P, paint);
  else if (line.rfind("@@", 0) == 0)
    emit_syntax(out, line, SyntaxKind::Attribute, P, paint);
  else if (!line.empty() && line[0] == '+')
    emit_syntax(out, line, SyntaxKind::String, P, paint);
  else if (!line.empty() && line[0] == '-')
    emit_syntax(out, line, SyntaxKind::Comment, P, paint);
  else
    out = line;
  return out;
}

static string highlight_code(const string &line, const ThemePalette &P,
                             Lang lang, HighlightState &state, bool paint) {
  const LanguageProfile &profile = language_profile(lang);
  string out;
  size_t i = 0;
  bool expect_name = false;
  size_t first = next_nonspace(line, 0);
  if (lang == Lang::Cpp && first < line.size() && line[first] == '#') {
    emit_syntax(out, line.substr(0, first), SyntaxKind::Plain, P, paint);
    size_t end = first + 1;
    while (end < line.size() && identifier_continue((unsigned char)line[end]))
      ++end;
    emit_syntax(out, line.substr(first, end - first), SyntaxKind::Preprocessor,
                P, paint);
    i = end;
  } else if ((lang == Lang::Python || lang == Lang::Ruby ||
              lang == Lang::Java || lang == Lang::CSharp) &&
             first < line.size() && line[first] == '@') {
    emit_syntax(out, line.substr(0, first), SyntaxKind::Plain, P, paint);
    size_t end = first + 1;
    while (end < line.size() &&
           (identifier_continue((unsigned char)line[end]) || line[end] == '.'))
      ++end;
    emit_syntax(out, line.substr(first, end - first), SyntaxKind::Attribute, P,
                paint);
    i = end;
  } else if (lang == Lang::Rust && first < line.size() &&
             line.compare(first, 2, "#[") == 0) {
    emit_syntax(out, line.substr(0, first), SyntaxKind::Plain, P, paint);
    size_t end = line.find(']', first + 2);
    end = end == string::npos ? line.size() : end + 1;
    emit_syntax(out, line.substr(first, end - first), SyntaxKind::Attribute, P,
                paint);
    i = end;
  }
  while (i < line.size()) {
    if (state.multiline_string) {
      size_t end = line.find(state.string_end, i);
      if (end == string::npos) {
        emit_syntax(out, line.substr(i), SyntaxKind::String, P, paint);
        return out;
      }
      end += state.string_end.size();
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::String, P, paint);
      i = end;
      state.multiline_string = false;
      continue;
    }
    if (state.block_comment) {
      size_t end = line.find(state.block_end, i);
      if (end == string::npos) {
        emit_syntax(out, line.substr(i), SyntaxKind::Comment, P, paint);
        return out;
      }
      end += state.block_end.size();
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Comment, P, paint);
      i = end;
      state.block_comment = false;
      continue;
    }
    if (line_comment_at(line, i, lang)) {
      emit_syntax(out, line.substr(i), SyntaxKind::Comment, P, paint);
      break;
    }
    string block_end;
    if (starts_block_comment(line, i, lang, block_end)) {
      size_t end = line.find(block_end, i + (lang == Lang::Lua ? 4 : 2));
      if (end == string::npos) {
        emit_syntax(out, line.substr(i), SyntaxKind::Comment, P, paint);
        state.block_comment = true;
        state.block_end = block_end;
        break;
      }
      end += block_end.size();
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Comment, P, paint);
      i = end;
      continue;
    }
    if ((lang == Lang::Python && (line.compare(i, 3, "\"\"\"") == 0 ||
                                  line.compare(i, 3, "'''") == 0)) ||
        (lang == Lang::JS && line[i] == '`')) {
      string delimiter = lang == Lang::JS ? "`" : line.substr(i, 3);
      size_t end = line.find(delimiter, i + delimiter.size());
      if (end == string::npos) {
        emit_syntax(out, line.substr(i), SyntaxKind::String, P, paint);
        state.multiline_string = true;
        state.string_end = delimiter;
        break;
      }
      end += delimiter.size();
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::String, P, paint);
      i = end;
      continue;
    }
    if (lang == Lang::Rust && line[i] == '\'' && i + 1 < line.size() &&
        identifier_start((unsigned char)line[i + 1])) {
      size_t end = i + 2;
      while (end < line.size() && identifier_continue((unsigned char)line[end]))
        ++end;
      if (end >= line.size() || line[end] != '\'') {
        emit_syntax(out, line.substr(i, end - i), SyntaxKind::Attribute, P,
                    paint);
        i = end;
        continue;
      }
    }
    if (line[i] == '\'' || line[i] == '"') {
      size_t end = scan_quoted(line, i, line[i]);
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::String, P, paint);
      i = end;
      continue;
    }
    if ((line[i] == '$' ||
         (line[i] == '@' &&
          (lang == Lang::Ruby || lang == Lang::Perl || lang == Lang::PHP))) &&
        i + 1 < line.size()) {
      size_t end = i + 1;
      if (line[end] == '{' || line[end] == '(') {
        char close = line[end] == '{' ? '}' : ')';
        ++end;
        while (end < line.size() && line[end] != close)
          ++end;
        if (end < line.size())
          ++end;
      } else
        while (end < line.size() &&
               (identifier_continue((unsigned char)line[end]) ||
                line[end] == '@' || line[end] == '#' || line[end] == '?' ||
                line[end] == '!'))
          ++end;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Variable, P, paint);
      i = end;
      continue;
    }
    if (std::isdigit((unsigned char)line[i]) ||
        (line[i] == '.' && i + 1 < line.size() &&
         std::isdigit((unsigned char)line[i + 1]))) {
      size_t end = scan_number(line, i);
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Number, P, paint);
      i = end;
      continue;
    }
    if (identifier_start((unsigned char)line[i])) {
      size_t end = i + 1;
      while (end < line.size() && identifier_continue((unsigned char)line[end]))
        ++end;
      string word = line.substr(i, end - i),
             lookup = profile.case_insensitive ? lower(word) : word;
      SyntaxKind kind =
          classify_word(word, lookup, profile, lang, line, i, end, expect_name);
      size_t next = next_nonspace(line, end);
      if (lang == Lang::Rust && next < line.size() && line[next] == '!')
        kind = SyntaxKind::Preprocessor;
      emit_syntax(out, word, kind, P, paint);
      expect_name =
          has_word(profile.declaration, lookup) &&
          (lookup == "class" || lookup == "def" || lookup == "function" ||
           lookup == "func" || lookup == "fn" || lookup == "struct" ||
           lookup == "enum" || lookup == "interface" || lookup == "trait" ||
           lookup == "type" || lookup == "module" || lookup == "namespace" ||
           lookup == "sub" || lookup == "record");
      i = end;
      continue;
    }
    if (string("+-*/%=&|!<>^~?:").find(line[i]) != string::npos) {
      size_t end = i + 1;
      while (end < line.size() &&
             string("+-*/%=&|!<>^~?:").find(line[end]) != string::npos)
        ++end;
      emit_syntax(out, line.substr(i, end - i), SyntaxKind::Operator, P, paint);
      i = end;
      continue;
    }
    if (string("{}[]();,.").find(line[i]) != string::npos)
      emit_syntax(out, string(1, line[i]), SyntaxKind::Punctuation, P, paint);
    else
      out.push_back(line[i]);
    ++i;
  }
  return out;
}

static string highlight_line(const string &line, const ThemePalette &P,
                             Lang lang, HighlightState &state, bool paint) {
  if (lang == Lang::Markdown)
    return highlight_markdown(line, P, state, paint);
  if (lang == Lang::HTML || lang == Lang::XML)
    return highlight_markup(line, P, state, paint);
  if (lang == Lang::CSS)
    return highlight_css(line, P, state, paint);
  if (lang == Lang::JSON || lang == Lang::YAML || lang == Lang::TOML ||
      lang == Lang::Ini)
    return highlight_config(line, P, lang, paint);
  if (lang == Lang::Diff)
    return highlight_diff(line, P, paint);
  return highlight_code(line, P, lang, state, paint);
}

static void advance_highlight_state(const string &line, const ThemePalette &P,
                                    Lang lang, HighlightState &state) {
  (void)highlight_line(line, P, lang, state, false);
}

static string colorize_lang(const string &line, const Buffer &b,
                            const ThemePalette &P, Lang lang,
                            HighlightState &state) {
  if (!use_color() || !b.highlight)
    return line;
  return highlight_line(line, P, lang, state, true);
}
