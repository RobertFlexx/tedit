using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;

namespace TEdit
{
    public class Editor
    {
        public Buffer Buffer { get; } = new Buffer();
        public LineReader Reader { get; } = new LineReader();
        public ThemePalette Palette { get; private set; }
        public LuaBridge Lua { get; } = new LuaBridge();

        private readonly UndoStack undo = new UndoStack();
        private readonly UndoStack redo = new UndoStack();
        private readonly List<Buffer> otherBuffers = new List<Buffer>();
        private readonly Dictionary<string, string> aliases = new Dictionary<string, string>();

        private ThemeType currentTheme = ThemeType.Default;
        private string activeLuaTheme = "";
        private string lastSearch = "";
        private bool lastSearchIgnoreCase = false;
        private int lastSearchIndex = 0;
        private int autosaveSeconds = 120;
        private DateTime lastAutosave = DateTime.Now;

        public bool WrapLongLines { get; set; } = true;
        public bool TruncateLongLines { get; set; } = false;

        public Editor()
        {
            Palette = ThemePalette.Create(currentTheme);
            InitializeCommands();
            Reader.SetThemeColors(Palette);
            Lua.Initialize(this);
        }

        private void InitializeCommands()
        {
            Reader.Commands = new List<string>
            {
                "help", "h", "?",
                "open", "info", "write", "w", "wq", "saveas", "quit", "q",
                "print", "p", "r", "goto",
                "append", "a", "insert", "i", "edit", "delete", "d", "move", "m", "join",
                "find", "findi", "findre", "n", "N",
                "repl", "replg", "read",
                "undo", "u", "redo",
                "set", "number", "highlight",
                "theme", "lua-themes",
                "alias", "new", "bnext", "bprev", "lsb",
                "diff", "filter",
                "ls", "dir", "pwd", "cd", "clear", "cls",
                "lua", "luafile", "run-plugin", "plugins", "reload-plugins",
                "version", "ver"
            };
        }

        public string PromptStr()
        {
            string dirty = Buffer.Dirty ? "*" : "";
            if (ThemePalette.UseColor)
                return $"{Palette.Prompt}{dirty}tedit> {Ansi.Reset}";
            return $"{dirty}tedit> ";
        }

        public void Status()
        {
            string time = DateTime.Now.ToString("HH:mm:ss");
            string file = string.IsNullOrEmpty(Buffer.Path) ? "(unnamed)" : Buffer.Path;
            string themeName = string.IsNullOrEmpty(activeLuaTheme) 
                ? ThemePalette.GetName(currentTheme) 
                : activeLuaTheme;

            Console.WriteLine($"{Palette.Dim}[{file}] lines={Buffer.LineCount} chars={Buffer.CharCount}" +
                $"{(Buffer.Dirty ? " *" : "")} | {time} | theme:{themeName} | " +
                $"wrap:{BoolStr(WrapLongLines)} | plugin:{(string.IsNullOrEmpty(Lua.CurrentPlugin) ? "none" : Lua.CurrentPlugin)}{Ansi.Reset}");
        }

        public void Banner()
        {
            string bannerPath = Path.Combine(Paths.Home(), ".tedit_banner");
            if (!File.Exists(bannerPath)) return;

            Console.Write(Palette.Accent);
            foreach (var line in File.ReadAllLines(bannerPath))
                Console.WriteLine(line);
            Console.Write(Ansi.Reset);
        }

        public void Tip()
        {
            var tips = new[]
            {
                "Tip: use 'goto <n>' to jump to a line.",
                "Tip: 'n' and 'N' cycle through search results.",
                "Tip: 'theme neon' for a vibrant look.",
                "Tip: 'alias dd \"delete 1-$\"' for quick delete all.",
                "Tip: 'diff' compares buffer with saved file.",
                "Tip: Tab completes commands and paths.",
                "Tip: PowerShell scripts work with 'filter'!"
            };

            var rnd = new Random();
            Console.WriteLine($"{Palette.Dim}{tips[rnd.Next(tips.Length)]}{Ansi.Reset}");
        }

        public void Load(string path)
        {
            path = Paths.ExpandHome(path);
            Buffer.LoadFromFile(path);
            Console.WriteLine($"{Palette.Ok}opened {path}{Ansi.Reset}");

            if (Buffer.TryRecover())
            {
                string recPath = Paths.RecoverFile(path);
                Console.WriteLine($"{Ansi.Yellow}recovery: loaded from {recPath}{Ansi.Reset}");
            }
        }

        public void LoadConfig()
        {
            string path = Paths.ConfigFile();
            if (!File.Exists(path)) return;

            foreach (var line in File.ReadAllLines(path))
            {
                if (string.IsNullOrWhiteSpace(line) || line.StartsWith("#")) continue;

                if (line.StartsWith("alias="))
                {
                    var parts = line[6..].Split(':', 2);
                    if (parts.Length == 2)
                        aliases[parts[0]] = parts[1];
                    continue;
                }

                var kv = line.Split('=', 2);
                if (kv.Length != 2) continue;

                string key = kv[0].Trim().ToLowerInvariant();
                string val = kv[1].Trim();

                switch (key)
                {
                    case "theme":
                        if (ThemePalette.TryParse(val, out var t))
                        {
                            currentTheme = t;
                            activeLuaTheme = "";
                            Palette = ThemePalette.Create(t);
                        }
                        else
                        {
                            var luaPalette = ThemePalette.LoadFromLua(val);
                            if (luaPalette != null)
                            {
                                Palette = luaPalette;
                                activeLuaTheme = val;
                            }
                        }
                        Reader.SetThemeColors(Palette);
                        break;
                    case "highlight":
                        if (val.ParseBool(out bool h)) Buffer.SyntaxHighlight = h;
                        break;
                    case "number":
                        if (val.ParseBool(out bool n)) Buffer.ShowLineNumbers = n;
                        break;
                    case "backup":
                        if (val.ParseBool(out bool b)) Buffer.CreateBackup = b;
                        break;
                    case "autosave":
                        if (int.TryParse(val, out int a)) autosaveSeconds = Math.Max(0, a);
                        break;
                    case "wrap":
                        if (val.ParseBool(out bool w)) WrapLongLines = w;
                        break;
                    case "truncate":
                        if (val.ParseBool(out bool tr)) TruncateLongLines = tr;
                        break;
                }
            }
        }

        public void SaveConfig()
        {
            var lines = new List<string>
            {
                "# TEdit Configuration",
                $"theme={(!string.IsNullOrEmpty(activeLuaTheme) ? activeLuaTheme : ThemePalette.GetName(currentTheme))}",
                $"highlight={BoolStr(Buffer.SyntaxHighlight)}",
                $"number={BoolStr(Buffer.ShowLineNumbers)}",
                $"backup={BoolStr(Buffer.CreateBackup)}",
                $"autosave={autosaveSeconds}",
                $"wrap={BoolStr(WrapLongLines)}",
                $"truncate={BoolStr(TruncateLongLines)}"
            };

            foreach (var kv in aliases)
                lines.Add($"alias={kv.Key}:{kv.Value}");

            File.WriteAllLines(Paths.ConfigFile(), lines);
        }

        private static string BoolStr(bool b) => b ? "on" : "off";

        public void PrintLine(int lineNum)
        {
            if (lineNum < 1 || lineNum > Buffer.LineCount) return;

            string line = Buffer.Lines[lineNum - 1];
            int gutterWidth = GutterWidth();

            if (Buffer.ShowLineNumbers)
            {
                string num = lineNum.ToString().PadLeft(gutterWidth - 3);
                Console.Write($"{Palette.Gutter}{num} | {Ansi.Reset}");
            }

            string colored = Buffer.SyntaxHighlight && ThemePalette.UseColor
                ? Colorize(line)
                : line;

            Console.WriteLine(colored + Ansi.Reset);
        }

        private int GutterWidth()
        {
            if (!Buffer.ShowLineNumbers) return 0;
            int maxLine = Math.Max(1, Buffer.LineCount);
            return maxLine.ToString().Length + 3;
        }

        private string Colorize(string line)
        {
            try
            {
                // string literals
                line = Regex.Replace(line, @"""([^""\\]|\\.)*""",
                    m => $"{Palette.Accent}{m.Value}{Ansi.Reset}");

                // comments (c like)
                line = Regex.Replace(line, @"//.*$",
                    m => $"{Palette.Dim}{m.Value}{Ansi.Reset}");
                
                // comments (python shell powershell, etc)
                line = Regex.Replace(line, @"#.*$",
                    m => $"{Palette.Dim}{m.Value}{Ansi.Reset}");
            }
            catch { }

            return line;
        }

        public bool Handle(string raw)
        {
            // autosave check
            CheckAutosave();

            string input = raw.Trim();
            if (string.IsNullOrEmpty(input)) return true;

            bool hadColon = false;
            if (input[0] == ':')
            {
                hadColon = true;
                input = input[1..].Trim();
                if (string.IsNullOrEmpty(input)) return true;
            }

            // alias expansion
            var firstWord = input.Split(' ', StringSplitOptions.RemoveEmptyEntries).FirstOrDefault() ?? "";
            if (aliases.TryGetValue(firstWord, out string? aliasValue))
            {
                input = aliasValue + input[firstWord.Length..];
            }

            // quick search
            if (input.StartsWith("/"))
            {
                string query = input[1..];
                lastSearch = query;
                lastSearchIgnoreCase = false;
                lastSearchIndex = 0;
                SearchPlain(query, false);
                return true;
            }

            var parts = input.Split(' ', 2, StringSplitOptions.RemoveEmptyEntries);
            string cmd = parts[0].ToLowerInvariant();
            string rest = parts.Length > 1 ? parts[1] : "";

            return ProcessCommand(cmd, rest, hadColon);
        }

        private bool ProcessCommand(string cmd, string rest, bool hadColon)
        {
            switch (cmd)
            {
                case "help":
                case "h":
                case "?":
                    ShowHelp();
                    return true;

                case "quit":
                case "q":
                    return HandleQuit();

                case "write":
                case "w":
                    Save(rest);
                    return true;

                case "wq":
                    if (Save(string.IsNullOrEmpty(rest) ? null : rest))
                    {
                        Console.WriteLine($"{Palette.Dim}bye!{Ansi.Reset}");
                        return false;
                    }
                    return true;

                case "saveas":
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: saveas <path>{Ansi.Reset}");
                        return true;
                    }
                    Save(rest);
                    return true;

                case "open":
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: open <path>{Ansi.Reset}");
                        return true;
                    }
                    if (Buffer.Dirty)
                    {
                        Console.WriteLine($"{Palette.Warn}Unsaved changes. Save first or use :q{Ansi.Reset}");
                        return true;
                    }
                    Load(rest);
                    return true;

                case "info":
                    ShowInfo();
                    return true;

                case "print":
                case "p":
                    ParseRange(rest, out int plo, out int phi);
                    Print(plo, phi);
                    return true;

                case "r":
                    if (!rest.ParseLong(out long rn) || rn < 1 || rn > Buffer.LineCount)
                    {
                        Console.WriteLine($"{Palette.Warn}usage: r <n>{Ansi.Reset}");
                        return true;
                    }
                    PrintLine((int)rn);
                    return true;

                case "goto":
                    if (!rest.ParseLong(out long gn) || gn < 1 || gn > Buffer.LineCount)
                    {
                        Console.WriteLine($"{Palette.Warn}usage: goto <n>{Ansi.Reset}");
                        return true;
                    }
                    PrintLine((int)gn);
                    return true;

                case "append":
                case "a":
                    PushUndo();
                    AppendMode();
                    return true;

                case "insert":
                case "i":
                    if (!rest.ParseLong(out long iln))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: insert <n>{Ansi.Reset}");
                        return true;
                    }
                    PushUndo();
                    InsertMode((int)iln);
                    return true;

                case "edit":
                    HandleEdit(rest);
                    return true;

                case "delete":
                case "d":
                    HandleDelete(rest);
                    return true;

                case "move":
                case "m":
                    HandleMove(rest);
                    return true;

                case "join":
                    HandleJoin(rest);
                    return true;

                case "find":
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: find <text>{Ansi.Reset}");
                        return true;
                    }
                    lastSearch = rest;
                    lastSearchIgnoreCase = false;
                    lastSearchIndex = 0;
                    SearchPlain(rest, false);
                    return true;

                case "findi":
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: findi <text>{Ansi.Reset}");
                        return true;
                    }
                    lastSearch = rest;
                    lastSearchIgnoreCase = true;
                    lastSearchIndex = 0;
                    SearchPlain(rest, true);
                    return true;

                case "findre":
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: findre <regex>{Ansi.Reset}");
                        return true;
                    }
                    SearchRegex(rest);
                    return true;

                case "n":
                    NextMatch(false);
                    return true;

                case "N":
                    NextMatch(true);
                    return true;

                case "repl":
                    HandleReplace(rest, false);
                    return true;

                case "replg":
                    HandleReplace(rest, true);
                    return true;

                case "read":
                    HandleRead(rest);
                    return true;

                case "undo":
                case "u":
                    HandleUndo(rest);
                    return true;

                case "redo":
                    HandleRedo();
                    return true;

                case "set":
                    HandleSet(rest);
                    return true;

                case "number":
                    Buffer.ShowLineNumbers = !Buffer.ShowLineNumbers;
                    Console.WriteLine($"number: {BoolStr(Buffer.ShowLineNumbers)}");
                    SaveConfig();
                    return true;

                case "highlight":
                    if (rest.ParseBool(out bool hl))
                    {
                        Buffer.SyntaxHighlight = hl;
                        Console.WriteLine($"highlight: {BoolStr(hl)}");
                        SaveConfig();
                    }
                    else
                    {
                        Console.WriteLine($"{Palette.Warn}usage: highlight on|off{Ansi.Reset}");
                    }
                    return true;

                case "theme":
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: theme <name>{Ansi.Reset}");
                        return true;
                    }
                    SetTheme(rest);
                    return true;

                case "lua-themes":
                    ListLuaThemes();
                    return true;

                case "alias":
                    HandleAlias(rest);
                    return true;

                case "new":
                    OpenNewBuffer(rest);
                    return true;

                case "bnext":
                    BufferNext();
                    return true;

                case "bprev":
                    BufferPrev();
                    return true;

                case "lsb":
                    ListBuffers();
                    return true;

                case "diff":
                    ShowDiff();
                    return true;

                case "filter":
                    HandleFilter(rest);
                    return true;

                case "ls":
                case "dir":
                    HandleLs(rest);
                    return true;

                case "pwd":
                    Console.WriteLine(Directory.GetCurrentDirectory());
                    return true;

                case "cd":
                    HandleCd(rest);
                    return true;

                case "clear":
                case "cls":
                    Console.Clear();
                    return true;

                case "lua":
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: lua <code>{Ansi.Reset}");
                        return true;
                    }
                    Lua.Execute(rest);
                    return true;

                case "luafile":
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: luafile <path>{Ansi.Reset}");
                        return true;
                    }
                    Lua.ExecuteFile(rest);
                    return true;

                case "run-plugin":
                    if (!hadColon)
                    {
                        Console.WriteLine($"{Palette.Warn}run-plugin must be called as :run-plugin{Ansi.Reset}");
                        return true;
                    }
                    if (string.IsNullOrEmpty(rest))
                    {
                        Console.WriteLine($"{Palette.Warn}usage: run-plugin <name|path>{Ansi.Reset}");
                        return true;
                    }
                    Lua.RunPlugin(rest);
                    return true;

                case "plugins":
                    ListPlugins();
                    return true;

                case "reload-plugins":
                    Lua.LoadPlugins();
                    Console.WriteLine("plugins reloaded");
                    return true;

                case "version":
                case "ver":
                    Console.WriteLine($"{Palette.Title}tedit {Program.VERSION}{Ansi.Reset}");
                    return true;

                default:
                    Console.WriteLine($"{Palette.Warn}unknown command — type 'help'{Ansi.Reset}");
                    return true;
            }
        }

        private void ShowHelp()
        {
            Console.WriteLine($"{Palette.Title}Commands (':' optional, except where noted){Ansi.Reset}");

            void Cmd(string name, string args, string desc)
            {
                Console.WriteLine($"{Palette.HelpCmd}{name,-20}{Ansi.Reset} {Palette.HelpArg}{args,-15}{Ansi.Reset} — {Palette.HelpText}{desc}{Ansi.Reset}");
            }

            Cmd("open <path>", "", "open file");
            Cmd("info", "", "buffer info");
            Cmd("w|write [path]", "", "save (atomic)");
            Cmd("wq", "", "save & quit");
            Cmd("q|quit", "", "quit");
            Cmd("p|print [range]", "", "print lines");
            Cmd("r <n>", "", "show line n");
            Cmd("goto <n>", "", "jump to line");
            Cmd("a|append", "", "append lines");
            Cmd("i|insert <n>", "", "insert before n");
            Cmd("edit <n> [text]", "", "edit line");
            Cmd("d|delete [range]", "", "delete lines");
            Cmd("m|move <from> <to>", "", "move line");
            Cmd("join <range>", "", "join lines");
            Cmd("/text | find[i]", "", "search");
            Cmd("findre <regex>", "", "regex search");
            Cmd("n | N", "", "next/prev match");
            Cmd("repl[g] old new", "", "replace");
            Cmd("read <path> [n]", "", "insert file");
            Cmd("undo [k] | redo", "", "undo/redo");
            Cmd("set <opt> <val>", "", "settings");
            Cmd("filter <range> !cmd", "", "shell filter");
            Cmd("theme <name>", "", "change theme");
            Cmd("lua-themes", "", "list Lua themes");
            Cmd("alias <from> <to>", "", "create alias");
            Cmd("new [path]", "", "new buffer");
            Cmd("bnext|bprev|lsb", "", "buffer nav");
            Cmd("diff", "", "show changes");
            Cmd("ls|dir [-l] [path]", "", "list files");
            Cmd("cd <dir>", "", "change directory");
            Cmd("clear|cls", "", "clear screen");
            Cmd("lua <code>", "", "run Lua");
            Cmd(":run-plugin <name>", "", "run plugin");
            Cmd("version", "", "show version");
        }

        private bool HandleQuit()
        {
            if (Buffer.Dirty)
            {
                Console.Write($"{Palette.Warn}Unsaved changes. Save? [y/N] {Ansi.Reset}");
                var key = Console.ReadKey();
                Console.WriteLine();
                if (key.KeyChar == 'y' || key.KeyChar == 'Y')
                {
                    if (!Save(null)) return true;
                }
            }
            Console.WriteLine($"{Palette.Dim}bye!{Ansi.Reset}");
            return false;
        }

        private bool Save(string? path)
        {
            string target = path ?? Buffer.Path;
            if (string.IsNullOrEmpty(target))
            {
                Console.WriteLine($"{Palette.Warn}save: no filename{Ansi.Reset}");
                return false;
            }

            try
            {
                target = Paths.ExpandHome(target);
                Buffer.SaveToFile(target);
                Console.WriteLine($"{Palette.Ok}saved to {target}{Ansi.Reset}");
                Confetti();
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{Palette.Err}save: {ex.Message}{Ansi.Reset}");
                return false;
            }
        }

        private void Confetti()
        {
            if (!ThemePalette.UseColor) return;
            var art = new[] { " *  .  *   . *", ".  *  *  .    *", "   *  .   *  . " };
            Console.WriteLine($"{Palette.Accent}{art[0]}{Ansi.Reset}");
            Console.WriteLine($"{Palette.Ok}{art[1]}{Ansi.Reset}");
            Console.WriteLine($"{Palette.Warn}{art[2]}{Ansi.Reset}");
        }

        private void ShowInfo()
        {
            string file = string.IsNullOrEmpty(Buffer.Path) ? "(unnamed)" : Buffer.Path;
            Console.WriteLine($"file: {file}{(Buffer.Dirty ? " *" : "")}");
            Console.WriteLine($"  lines: {Buffer.LineCount}, chars: {Buffer.CharCount}");

            if (!string.IsNullOrEmpty(Buffer.Path) && File.Exists(Buffer.Path))
            {
                var fi = new FileInfo(Buffer.Path);
                Console.WriteLine($"  size: {fi.Length} bytes, modified: {fi.LastWriteTime}");
            }
        }

        private void Print(int lo, int hi)
        {
            if (Buffer.LineCount == 0)
            {
                Console.WriteLine("(empty)");
                return;
            }

            for (int i = lo; i <= hi && i <= Buffer.LineCount; i++)
                PrintLine(i);
        }

        private void ParseRange(string rangeStr, out int lo, out int hi)
        {
            lo = 1;
            hi = Math.Max(1, Buffer.LineCount);

            if (string.IsNullOrWhiteSpace(rangeStr)) return;

            rangeStr = rangeStr.Replace("$", Buffer.LineCount.ToString());

            if (rangeStr.Contains('-'))
            {
                var parts = rangeStr.Split('-');
                if (int.TryParse(parts[0].Trim(), out int l)) lo = l;
                if (parts.Length > 1 && int.TryParse(parts[1].Trim(), out int h)) hi = h;
            }
            else if (int.TryParse(rangeStr.Trim(), out int single))
            {
                lo = hi = single;
            }

            lo = Math.Max(1, lo);
            hi = Math.Min(Buffer.LineCount, hi);
        }

        private void PushUndo()
        {
            undo.Push(Buffer);
            redo.Clear();
        }

        private void AppendMode()
        {
            Console.WriteLine("enter text; '.' alone ends (use \".\" for a literal '.')");
            int added = 0;
            while (true)
            {
                Console.Write("> ");
                string? line = Console.ReadLine();
                if (line == null) break;
                if (line == ".\"") line = ".";
                else if (line == ".") break;

                Buffer.Lines.Add(line);
                added++;
            }
            if (added > 0)
            {
                Buffer.Dirty = true;
                Console.WriteLine($"appended {added} line(s)");
            }
        }

        private void InsertMode(int before)
        {
            before = Math.Clamp(before, 1, Buffer.LineCount + 1);
            Console.WriteLine("enter text; '.' alone ends");
            int added = 0;
            while (true)
            {
                Console.Write("> ");
                string? line = Console.ReadLine();
                if (line == null) break;
                if (line == ".\"") line = ".";
                else if (line == ".") break;

                int insertPos = before - 1 + added;
                if (insertPos > Buffer.LineCount)
                    Buffer.Lines.Add(line);
                else
                    Buffer.Lines.Insert(insertPos, line);
                added++;
            }
            if (added > 0)
            {
                Buffer.Dirty = true;
                Console.WriteLine($"inserted {added} line(s)");
            }
        }

        private void HandleEdit(string rest)
        {
            var parts = rest.Split(' ', 2);
            if (!parts[0].ParseLong(out long ln) || ln < 1 || ln > Buffer.LineCount)
            {
                Console.WriteLine($"{Palette.Warn}usage: edit <n> [text]{Ansi.Reset}");
                return;
            }

            PushUndo();
            int lineNum = (int)ln;

            if (parts.Length == 1)
            {
                Console.WriteLine($"old: {Buffer.Lines[lineNum - 1]}");
                Console.Write("new> ");
                string? newLine = Console.ReadLine();
                if (newLine != null)
                {
                    Buffer.Lines[lineNum - 1] = newLine;
                    Buffer.Dirty = true;
                    Console.WriteLine($"edited line {lineNum}");
                }
            }
            else
            {
                Buffer.Lines[lineNum - 1] = parts[1];
                Buffer.Dirty = true;
                Console.WriteLine($"edited line {lineNum}");
            }
        }

        private void HandleDelete(string rest)
        {
            if (Buffer.LineCount == 0)
            {
                Console.WriteLine("(empty)");
                return;
            }

            ParseRange(rest, out int lo, out int hi);
            PushUndo();
            int count = hi - lo + 1;
            Buffer.Lines.RemoveRange(lo - 1, count);
            Buffer.Dirty = true;
            Console.WriteLine($"deleted {count} line(s)");
        }

        private void HandleMove(string rest)
        {
            var parts = rest.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 2 ||
                !parts[0].ParseLong(out long fromL) ||
                !parts[1].ParseLong(out long toL))
            {
                Console.WriteLine($"{Palette.Warn}usage: move <from> <to>{Ansi.Reset}");
                return;
            }

            int from = (int)fromL, to = (int)toL;
            if (from < 1 || from > Buffer.LineCount || to < 0 || to > Buffer.LineCount)
            {
                Console.WriteLine($"{Palette.Warn}invalid line numbers{Ansi.Reset}");
                return;
            }

            PushUndo();
            string line = Buffer.Lines[from - 1];
            Buffer.Lines.RemoveAt(from - 1);
            if (to > from) to--;
            to = Math.Min(to, Buffer.LineCount);
            Buffer.Lines.Insert(to, line);
            Buffer.Dirty = true;
            Console.WriteLine($"moved line {fromL} to {to}");
        }

        private void HandleJoin(string rest)
        {
            ParseRange(rest, out int lo, out int hi);
            if (hi <= lo)
            {
                Console.WriteLine($"{Palette.Warn}need at least 2 lines to join{Ansi.Reset}");
                return;
            }

            PushUndo();
            var joined = string.Join(" ", Buffer.Lines.Skip(lo - 1).Take(hi - lo + 1));
            Buffer.Lines.RemoveRange(lo - 1, hi - lo + 1);
            Buffer.Lines.Insert(lo - 1, joined);
            Buffer.Dirty = true;
            Console.WriteLine("joined");
        }

        private void SearchPlain(string query, bool ignoreCase)
        {
            var comparison = ignoreCase
                ? StringComparison.OrdinalIgnoreCase
                : StringComparison.Ordinal;

            var hits = new List<int>();
            for (int i = 0; i < Buffer.LineCount; i++)
            {
                if (Buffer.Lines[i].Contains(query, comparison))
                    hits.Add(i + 1);
            }

            if (hits.Count == 0)
            {
                Console.WriteLine("no matches");
                return;
            }

            foreach (var ln in hits)
                Console.WriteLine($"match at {ln}: {Buffer.Lines[ln - 1]}");
        }

        private void SearchRegex(string pattern)
        {
            try
            {
                var regex = new Regex(pattern);
                int hits = 0;
                for (int i = 0; i < Buffer.LineCount; i++)
                {
                    if (regex.IsMatch(Buffer.Lines[i]))
                    {
                        Console.WriteLine($"match at {i + 1}: {Buffer.Lines[i]}");
                        hits++;
                    }
                }
                if (hits == 0) Console.WriteLine("no matches");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{Palette.Err}regex: {ex.Message}{Ansi.Reset}");
            }
        }

        private void NextMatch(bool reverse)
        {
            if (string.IsNullOrEmpty(lastSearch))
            {
                Console.WriteLine("(no previous search)");
                return;
            }

            var comparison = lastSearchIgnoreCase
                ? StringComparison.OrdinalIgnoreCase
                : StringComparison.Ordinal;

            var hits = new List<int>();
            for (int i = 0; i < Buffer.LineCount; i++)
            {
                if (Buffer.Lines[i].Contains(lastSearch, comparison))
                    hits.Add(i + 1);
            }

            if (hits.Count == 0)
            {
                Console.WriteLine("no matches");
                return;
            }

            if (!reverse)
            {
                var next = hits.FirstOrDefault(h => h > lastSearchIndex);
                lastSearchIndex = next != 0 ? next : hits[0];
            }
            else
            {
                var prev = hits.LastOrDefault(h => h < lastSearchIndex);
                lastSearchIndex = prev != 0 ? prev : hits[^1];
            }

            PrintLine(lastSearchIndex);
        }

        private void HandleReplace(string rest, bool global)
        {
            var parts = rest.Split(' ', 2);
            if (parts.Length < 2)
            {
                Console.WriteLine($"{Palette.Warn}usage: repl[g] <old> <new>{Ansi.Reset}");
                return;
            }

            string oldText = parts[0];
            string newText = parts[1];

            PushUndo();
            int total = 0;

            for (int i = 0; i < Buffer.LineCount; i++)
            {
                string line = Buffer.Lines[i];
                string modified = global
                    ? line.Replace(oldText, newText)
                    : ReplaceFirst(line, oldText, newText);

                if (modified != line)
                {
                    Buffer.Lines[i] = modified;
                    total++;
                }
            }

            if (total > 0)
            {
                Buffer.Dirty = true;
                Console.WriteLine($"replaced {total} line(s)");
            }
            else
            {
                Console.WriteLine("no occurrences found");
            }
        }

        private static string ReplaceFirst(string text, string oldValue, string newValue)
        {
            int pos = text.IndexOf(oldValue);
            if (pos < 0) return text;
            return text[..pos] + newValue + text[(pos + oldValue.Length)..];
        }

        private void HandleRead(string rest)
        {
            var parts = rest.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 0)
            {
                Console.WriteLine($"{Palette.Warn}usage: read <path> [n]{Ansi.Reset}");
                return;
            }

            string path = Paths.ExpandHome(parts[0]);
            int insertAfter = Buffer.LineCount;
            if (parts.Length > 1 && int.TryParse(parts[1], out int n))
                insertAfter = Math.Clamp(n, 0, Buffer.LineCount);

            if (!File.Exists(path))
            {
                Console.WriteLine($"{Palette.Err}read: file not found{Ansi.Reset}");
                return;
            }

            PushUndo();
            var lines = File.ReadAllLines(path).Select(l => l.TrimEndNewlines()).ToList();
            Buffer.Lines.InsertRange(insertAfter, lines);
            Buffer.Dirty = true;
            Console.WriteLine($"read {lines.Count} line(s) from {path}");
        }

        private void HandleUndo(string rest)
        {
            int steps = 1;
            if (!string.IsNullOrEmpty(rest) && int.TryParse(rest, out int k))
                steps = Math.Max(1, k);

            bool any = false;
            while (steps-- > 0 && undo.TryPop(out var snap))
            {
                redo.Push(Buffer);
                Buffer.Lines = new List<string>(snap!.Lines);
                Buffer.Dirty = true;
                any = true;
            }

            Console.WriteLine(any ? "undo" : "nothing to undo");
        }

        private void HandleRedo()
        {
            if (!redo.TryPop(out var snap))
            {
                Console.WriteLine("nothing to redo");
                return;
            }

            undo.Push(Buffer);
            Buffer.Lines = new List<string>(snap!.Lines);
            Buffer.Dirty = true;
            Console.WriteLine("redo");
        }

        private void HandleSet(string rest)
        {
            var parts = rest.Split(' ', 2, StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length < 2)
            {
                Console.WriteLine($"{Palette.Warn}usage: set <option> <value>{Ansi.Reset}");
                return;
            }

            string key = parts[0].ToLowerInvariant();
            string val = parts[1];

            switch (key)
            {
                case "number":
                    if (val.ParseBool(out bool n))
                    {
                        Buffer.ShowLineNumbers = n;
                        Console.WriteLine($"number: {BoolStr(n)}");
                        SaveConfig();
                    }
                    break;
                case "backup":
                    if (val.ParseBool(out bool b))
                    {
                        Buffer.CreateBackup = b;
                        Console.WriteLine($"backup: {BoolStr(b)}");
                        SaveConfig();
                    }
                    break;
                case "autosave":
                    if (int.TryParse(val, out int a))
                    {
                        autosaveSeconds = Math.Max(0, a);
                        Console.WriteLine($"autosave: {autosaveSeconds}s");
                        SaveConfig();
                    }
                    break;
                case "wrap":
                    if (val.ParseBool(out bool w))
                    {
                        WrapLongLines = w;
                        Console.WriteLine($"wrap: {BoolStr(w)}");
                        SaveConfig();
                    }
                    break;
                case "truncate":
                    if (val.ParseBool(out bool t))
                    {
                        TruncateLongLines = t;
                        Console.WriteLine($"truncate: {BoolStr(t)}");
                        SaveConfig();
                    }
                    break;
                case "lang":
                    Buffer.Language = Buffer.DetectLanguage("." + val) != Lang.Plain
                        ? Buffer.DetectLanguage("." + val)
                        : Lang.Plain;
                    Console.WriteLine("lang: set");
                    break;
                default:
                    Console.WriteLine($"{Palette.Warn}unknown setting: {key}{Ansi.Reset}");
                    break;
            }
        }

        private void SetTheme(string name)
        {
            if (ThemePalette.TryParse(name, out var t))
            {
                currentTheme = t;
                activeLuaTheme = "";
                Palette = ThemePalette.Create(t);
                Reader.SetThemeColors(Palette);
                Console.WriteLine($"{Palette.Ok}theme set{Ansi.Reset}");
                SaveConfig();
                return;
            }

            var luaPalette = ThemePalette.LoadFromLua(name);
            if (luaPalette != null)
            {
                Palette = luaPalette;
                activeLuaTheme = name;
                Reader.SetThemeColors(Palette);
                Console.WriteLine($"{Palette.Ok}theme set (lua: {name}){Ansi.Reset}");
                SaveConfig();
                return;
            }

            Console.WriteLine($"{Palette.Err}theme not found{Ansi.Reset}");
        }

        private void ListLuaThemes()
        {
            var themes = ThemePalette.ListLuaThemes();
            if (themes.Count == 0)
            {
                Console.WriteLine("no lua themes found");
                return;
            }

            Console.WriteLine("lua themes:");
            foreach (var name in themes)
            {
                bool current = string.Equals(name, activeLuaTheme, StringComparison.OrdinalIgnoreCase);
                Console.WriteLine($"- {name}{(current ? " *" : "")}");
            }
        }

        private void HandleAlias(string rest)
        {
            var parts = rest.Split(' ', 2);
            if (parts.Length < 2)
            {
                Console.WriteLine($"{Palette.Warn}usage: alias <from> <to>{Ansi.Reset}");
                return;
            }

            aliases[parts[0]] = parts[1];
            Console.WriteLine($"alias: {parts[0]} -> {parts[1]}");
            SaveConfig();
        }

        private void OpenNewBuffer(string path)
        {
            otherBuffers.Add(Buffer);
            var newBuffer = new Buffer
            {
                ShowLineNumbers = Buffer.ShowLineNumbers,
                CreateBackup = Buffer.CreateBackup,
                SyntaxHighlight = Buffer.SyntaxHighlight
            };

            if (!string.IsNullOrEmpty(path))
                newBuffer.LoadFromFile(Paths.ExpandHome(path));

            // cant reassign buffer since its init only
            // for simplicity, copy the lines
            Buffer.Lines.Clear();
            Buffer.Lines.AddRange(newBuffer.Lines);
            Buffer.Path = newBuffer.Path;
            Buffer.Dirty = newBuffer.Dirty;
            Buffer.Language = newBuffer.Language;

            Console.WriteLine($"{Palette.Ok}(new buffer) {(string.IsNullOrEmpty(path) ? "(unnamed)" : path)}{Ansi.Reset}");
        }

        private void BufferNext()
        {
            if (otherBuffers.Count == 0)
            {
                Console.WriteLine("(only one buffer)");
                return;
            }

            // store current as ssnapshot
            otherBuffers.Insert(0, new Buffer
            {
                Path = Buffer.Path,
                Lines = new List<string>(Buffer.Lines),
                Dirty = Buffer.Dirty,
                ShowLineNumbers = Buffer.ShowLineNumbers,
                CreateBackup = Buffer.CreateBackup,
                SyntaxHighlight = Buffer.SyntaxHighlight,
                Language = Buffer.Language
            });

            var next = otherBuffers[^1];
            otherBuffers.RemoveAt(otherBuffers.Count - 1);

            Buffer.Path = next.Path;
            Buffer.Lines = next.Lines;
            Buffer.Dirty = next.Dirty;
            Buffer.Language = next.Language;

            Console.WriteLine($"[bnext] {(string.IsNullOrEmpty(Buffer.Path) ? "(unnamed)" : Buffer.Path)}");
        }

        private void BufferPrev()
        {
            if (otherBuffers.Count == 0)
            {
                Console.WriteLine("(only one buffer)");
                return;
            }

            otherBuffers.Add(new Buffer
            {
                Path = Buffer.Path,
                Lines = new List<string>(Buffer.Lines),
                Dirty = Buffer.Dirty,
                ShowLineNumbers = Buffer.ShowLineNumbers,
                CreateBackup = Buffer.CreateBackup,
                SyntaxHighlight = Buffer.SyntaxHighlight,
                Language = Buffer.Language
            });

            var prev = otherBuffers[0];
            otherBuffers.RemoveAt(0);

            Buffer.Path = prev.Path;
            Buffer.Lines = prev.Lines;
            Buffer.Dirty = prev.Dirty;
            Buffer.Language = prev.Language;

            Console.WriteLine($"[bprev] {(string.IsNullOrEmpty(Buffer.Path) ? "(unnamed)" : Buffer.Path)}");
        }

        private void ListBuffers()
        {
            Console.WriteLine($"{Ansi.Bold}* 0 {(string.IsNullOrEmpty(Buffer.Path) ? "(unnamed)" : Buffer.Path)}{(Buffer.Dirty ? " *" : "")}{Ansi.Reset}");
            for (int i = 0; i < otherBuffers.Count; i++)
            {
                var b = otherBuffers[i];
                Console.WriteLine($"  {i + 1} {(string.IsNullOrEmpty(b.Path) ? "(unnamed)" : b.Path)}{(b.Dirty ? " *" : "")}");
            }
        }

        private void ShowDiff()
        {
            if (string.IsNullOrEmpty(Buffer.Path) || !File.Exists(Buffer.Path))
            {
                Console.WriteLine("diff: no on-disk version");
                return;
            }

            try
            {
                // create temp file with current buffer
                string tempPath = Path.GetTempFileName();
                File.WriteAllLines(tempPath, Buffer.Lines);

                // use FC on windows
                var psi = new ProcessStartInfo
                {
                    FileName = "fc.exe",
                    Arguments = $"/N \"{Buffer.Path}\" \"{tempPath}\"",
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    CreateNoWindow = true
                };

                using var process = Process.Start(psi);
                if (process != null)
                {
                    Console.WriteLine(process.StandardOutput.ReadToEnd());
                    process.WaitForExit();
                }

                File.Delete(tempPath);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{Palette.Err}diff: {ex.Message}{Ansi.Reset}");
            }
        }

        private void HandleFilter(string rest)
        {
            // parse
            int bangPos = rest.IndexOf('!');
            if (bangPos < 0)
            {
                Console.WriteLine($"{Palette.Warn}usage: filter <range> !command{Ansi.Reset}");
                return;
            }

            string rangeStr = rest[..bangPos].Trim();
            string command = rest[(bangPos + 1)..].Trim();

            ParseRange(rangeStr, out int lo, out int hi);

            if (lo < 1 || hi < lo || hi > Buffer.LineCount)
            {
                Console.WriteLine($"{Palette.Warn}invalid range{Ansi.Reset}");
                return;
            }

            try
            {
                // write selected lines to temp file
                string inPath = Path.GetTempFileName();
                string outPath = Path.GetTempFileName();

                var selectedLines = Buffer.Lines.Skip(lo - 1).Take(hi - lo + 1);
                File.WriteAllLines(inPath, selectedLines);

                // run command (using powershell for flexibility)
                var psi = new ProcessStartInfo
                {
                    FileName = "powershell.exe",
                    Arguments = $"-NoProfile -Command \"Get-Content '{inPath}' | {command} | Set-Content '{outPath}'\"",
                    UseShellExecute = false,
                    CreateNoWindow = true
                };

                using (var process = Process.Start(psi))
                {
                    process?.WaitForExit();
                    if (process?.ExitCode != 0)
                    {
                        Console.WriteLine($"{Palette.Err}filter failed{Ansi.Reset}");
                        return;
                    }
                }

                // read output
                var outputLines = File.ReadAllLines(outPath).ToList();

                // replace lines
                PushUndo();
                Buffer.Lines.RemoveRange(lo - 1, hi - lo + 1);
                Buffer.Lines.InsertRange(lo - 1, outputLines);
                Buffer.Dirty = true;

                Console.WriteLine("filtered");

                // cleanup
                File.Delete(inPath);
                File.Delete(outPath);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{Palette.Err}filter: {ex.Message}{Ansi.Reset}");
            }
        }

        private void HandleLs(string rest)
        {
            bool longFormat = false;
            bool showAll = false;
            string path = ".";

            foreach (var arg in rest.Split(' ', StringSplitOptions.RemoveEmptyEntries))
            {
                if (arg == "-l") longFormat = true;
                else if (arg == "-a") showAll = true;
                else path = Paths.ExpandHome(arg);
            }

            try
            {
                var entries = Directory.GetFileSystemEntries(path)
                    .Select(e => new { Path = e, Name = Path.GetFileName(e), IsDir = Directory.Exists(e) })
                    .Where(e => showAll || !e.Name.StartsWith("."))
                    .OrderBy(e => e.Name)
                    .ToList();

                foreach (var entry in entries)
                {
                    if (longFormat)
                    {
                        var fi = entry.IsDir ? null : new FileInfo(entry.Path);
                        var di = entry.IsDir ? new DirectoryInfo(entry.Path) : null;
                        string size = fi != null ? fi.Length.ToString().PadLeft(12) : "<DIR>".PadLeft(12);
                        string date = (fi?.LastWriteTime ?? di?.LastWriteTime)?.ToString("yyyy-MM-dd HH:mm") ?? "";
                        string name = entry.Name + (entry.IsDir ? "\\" : "");
                        Console.WriteLine($"{size}  {date}  {name}");
                    }
                    else
                    {
                        string name = entry.Name + (entry.IsDir ? "\\" : "");
                        Console.WriteLine(name);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{Palette.Err}ls: {ex.Message}{Ansi.Reset}");
            }
        }

        private void HandleCd(string rest)
        {
            if (string.IsNullOrEmpty(rest))
            {
                Console.WriteLine($"{Palette.Warn}usage: cd <path>{Ansi.Reset}");
                return;
            }

            string path = Paths.ExpandHome(rest);

            try
            {
                Directory.SetCurrentDirectory(path);
                Console.WriteLine($"{Palette.Ok}cd: {Directory.GetCurrentDirectory()}{Ansi.Reset}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{Palette.Err}cd: {ex.Message}{Ansi.Reset}");
            }
        }

        private void ListPlugins()
        {
            if (Lua.PluginNames.Count == 0)
            {
                Console.WriteLine("no plugins found (use reload-plugins to rescan)");
                return;
            }

            Console.WriteLine("available plugins:");
            foreach (var name in Lua.PluginNames)
            {
                bool current = name == Lua.CurrentPlugin;
                if (Lua.PluginFiles.TryGetValue(name, out string? path))
                    Console.WriteLine($"- {name} ({path}){(current ? " *" : "")}");
                else
                    Console.WriteLine($"- {name}{(current ? " *" : "")}");
            }
        }

        private void CheckAutosave()
        {
            if (autosaveSeconds <= 0) return;
            if ((DateTime.Now - lastAutosave).TotalSeconds < autosaveSeconds) return;

            Buffer.AutoSave();
            lastAutosave = DateTime.Now;
        }
    }
}
