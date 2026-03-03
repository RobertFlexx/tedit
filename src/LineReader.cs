using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace TEdit
{
    public class LineReader
    {
        private readonly List<string> history = new List<string>();
        private const int MaxHistory = 800;

        public List<string> Commands { get; set; } = new List<string>();
        public string ColorInput { get; set; } = "";
        public string ColorReset { get; set; } = Ansi.Reset;

        public void SetThemeColors(ThemePalette palette)
        {
            ColorInput = palette.Input;
            ColorReset = Ansi.Reset;
        }

        public void Remember(string line)
        {
            if (string.IsNullOrEmpty(line)) return;

            if (history.Count == 0 || history[^1] != line)
            {
                if (history.Count >= MaxHistory)
                    history.RemoveAt(0);
                history.Add(line);
            }
        }

        public string? Read(string prompt)
        {
            if (Console.IsInputRedirected)
            {
                Console.Write(prompt);
                return Console.ReadLine();
            }

            Console.Write(prompt);

            var buffer = new StringBuilder();
            int cursor = 0;
            int historyIndex = history.Count;

            void Refresh()
            {
                // clear line and rewrite
                int promptLen = StripAnsi(prompt).Length;
                Console.Write("\r" + new string(' ', Console.WindowWidth - 1) + "\r");
                Console.Write(prompt + ColorInput + buffer.ToString() + ColorReset);

                // move cursor back if needed
                int tail = buffer.Length - cursor;
                if (tail > 0)
                    Console.Write(Ansi.MoveCursorLeft(tail));
            }

            while (true)
            {
                var keyInfo = Console.ReadKey(intercept: true);

                switch (keyInfo.Key)
                {
                    case ConsoleKey.Enter:
                        Console.WriteLine();
                        return buffer.ToString();

                    case ConsoleKey.Escape:
                        buffer.Clear();
                        cursor = 0;
                        Refresh();
                        break;

                    case ConsoleKey.Backspace:
                        if (cursor > 0)
                        {
                            buffer.Remove(cursor - 1, 1);
                            cursor--;
                            Refresh();
                        }
                        break;

                    case ConsoleKey.Delete:
                        if (cursor < buffer.Length)
                        {
                            buffer.Remove(cursor, 1);
                            Refresh();
                        }
                        break;

                    case ConsoleKey.Tab:
                        HandleTabCompletion(buffer, ref cursor);
                        Refresh();
                        break;

                    case ConsoleKey.UpArrow:
                        if (historyIndex > 0)
                        {
                            historyIndex--;
                            buffer.Clear();
                            buffer.Append(history[historyIndex]);
                            cursor = buffer.Length;
                            Refresh();
                        }
                        break;

                    case ConsoleKey.DownArrow:
                        if (historyIndex < history.Count - 1)
                        {
                            historyIndex++;
                            buffer.Clear();
                            buffer.Append(history[historyIndex]);
                            cursor = buffer.Length;
                            Refresh();
                        }
                        else
                        {
                            historyIndex = history.Count;
                            buffer.Clear();
                            cursor = 0;
                            Refresh();
                        }
                        break;

                    case ConsoleKey.LeftArrow:
                        if (cursor > 0)
                        {
                            cursor--;
                            Console.Write(Ansi.MoveCursorLeft(1));
                        }
                        break;

                    case ConsoleKey.RightArrow:
                        if (cursor < buffer.Length)
                        {
                            cursor++;
                            Console.Write(Ansi.MoveCursorRight(1));
                        }
                        break;

                    case ConsoleKey.Home:
                        if (cursor > 0)
                        {
                            Console.Write(Ansi.MoveCursorLeft(cursor));
                            cursor = 0;
                        }
                        break;

                    case ConsoleKey.End:
                        if (cursor < buffer.Length)
                        {
                            Console.Write(Ansi.MoveCursorRight(buffer.Length - cursor));
                            cursor = buffer.Length;
                        }
                        break;

                    default:
                        if (!char.IsControl(keyInfo.KeyChar))
                        {
                            buffer.Insert(cursor, keyInfo.KeyChar);
                            cursor++;
                            Refresh();
                        }
                        break;
                }
            }
        }

        private void HandleTabCompletion(StringBuilder buffer, ref int cursor)
        {
            var completions = GetCompletions(buffer.ToString());

            if (completions.Count == 0)
                return;

            if (completions.Count == 1)
            {
                // single match
                var words = buffer.ToString().Split(' ', StringSplitOptions.RemoveEmptyEntries);
                int lastSpace = buffer.ToString().LastIndexOf(' ');
                string prefix = lastSpace == -1 ? "" : buffer.ToString()[..(lastSpace + 1)];

                buffer.Clear();
                buffer.Append(prefix + completions[0]);
                cursor = buffer.Length;
            }
            else
            {
                // multiple matches
                Console.WriteLine();
                int shown = 0;
                foreach (var opt in completions)
                {
                    Console.Write(opt.PadRight(20));
                    if (++shown % 4 == 0) Console.WriteLine();
                }
                if (shown % 4 != 0) Console.WriteLine();
            }
        }

        private List<string> GetCompletions(string input)
        {
            var words = input.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            bool endsWithSpace = input.Length > 0 && char.IsWhiteSpace(input[^1]);

            // empty input or just starting
            if (words.Length == 0)
                return Commands.ToList();

            // completing first word (command)
            if (words.Length == 1 && !endsWithSpace)
            {
                string prefix = words[0];
                return Commands.Where(c => c.StartsWith(prefix, StringComparison.OrdinalIgnoreCase)).ToList();
            }

            // after command
            string lastToken = endsWithSpace ? "" : words[^1];

            // special handling for cd or chdir
            if (words[0].Equals("cd", StringComparison.OrdinalIgnoreCase))
                return CompleteDirectories(lastToken);

            return CompleteFileSystem(lastToken);
        }

        private List<string> CompleteDirectories(string token)
        {
            var results = new List<string>();
            string expanded = Paths.ExpandHome(token);
            string dir = ".";
            string baseName = expanded;

            int lastSlash = expanded.LastIndexOfAny(new[] { '\\', '/' });
            if (lastSlash != -1)
            {
                dir = expanded[..lastSlash];
                if (string.IsNullOrEmpty(dir)) dir = "\\";
                baseName = expanded[(lastSlash + 1)..];
            }

            try
            {
                foreach (var d in Directory.GetDirectories(dir))
                {
                    string name = Path.GetFileName(d);
                    if (name.StartsWith(baseName, StringComparison.OrdinalIgnoreCase))
                    {
                        string candidate = dir == "." ? name : Path.Combine(dir, name);
                        results.Add(candidate + Path.DirectorySeparatorChar);
                    }
                }
            }
            catch { }

            results.Sort();
            return results;
        }

        private List<string> CompleteFileSystem(string token)
        {
            var results = new List<string>();
            string expanded = Paths.ExpandHome(token);
            string dir = ".";
            string baseName = expanded;

            int lastSlash = expanded.LastIndexOfAny(new[] { '\\', '/' });
            if (lastSlash != -1)
            {
                dir = expanded[..lastSlash];
                if (string.IsNullOrEmpty(dir)) dir = "\\";
                baseName = expanded[(lastSlash + 1)..];
            }

            try
            {
                foreach (var entry in Directory.GetFileSystemEntries(dir))
                {
                    string name = Path.GetFileName(entry);
                    if (name.StartsWith(baseName, StringComparison.OrdinalIgnoreCase))
                    {
                        string candidate = dir == "." ? name : Path.Combine(dir, name);
                        if (Directory.Exists(entry))
                            candidate += Path.DirectorySeparatorChar;
                        results.Add(candidate);
                    }
                }
            }
            catch { }

            results.Sort();
            return results;
        }

        private static string StripAnsi(string s)
        {
            return System.Text.RegularExpressions.Regex.Replace(s, @"\x1b\[[0-9;]*m", "");
        }
    }
}
