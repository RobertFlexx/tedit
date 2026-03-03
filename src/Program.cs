using System;

namespace TEdit
{
    class Program
    {
        public const string VERSION = "2.1.0-win";

        static int Main(string[] args)
        {
            try
            {
                Console.OutputEncoding = System.Text.Encoding.UTF8;
                Console.InputEncoding = System.Text.Encoding.UTF8;

                // enable ansi on windows (fuck windows)
                AnsiSupport.Enable();

                var editor = new Editor();
                editor.LoadConfig();

                if (args.Length >= 1)
                {
                    editor.Load(args[0]);
                }

                editor.Banner();
                Console.WriteLine($"{editor.Palette.Accent}tedit — editing " +
                $"{(string.IsNullOrEmpty(editor.Buffer.Path) ? "(unnamed)" : editor.Buffer.Path)} " +
                $"({editor.Buffer.Lines.Count} lines). Type 'help'.{Ansi.Reset}");
                editor.Tip();

                while (true)
                {
                    editor.Status();
                    string? line = editor.Reader.Read(editor.PromptStr());

                    if (line == null)
                    {
                        Console.WriteLine();
                        break;
                    }

                    if (string.IsNullOrEmpty(line)) continue;

                    editor.Reader.Remember(line);
                    bool keepRunning = editor.Handle(line);
                    if (!keepRunning) break;
                }

                return 0;
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"Fatal error: {ex.Message}");
                return 1;
            }
        }
    }
}
