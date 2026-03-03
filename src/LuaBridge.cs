using System;
using System.Collections.Generic;
using System.IO;
using NLua;

namespace TEdit
{
    public class LuaBridge : IDisposable
    {
        private Lua? lua;
        private Editor? editor;
        private readonly Dictionary<string, string> pluginFiles = new Dictionary<string, string>();
        private readonly List<string> pluginNames = new List<string>();

        public string CurrentPlugin { get; private set; } = "";
        public IReadOnlyList<string> PluginNames => pluginNames;
        public IReadOnlyDictionary<string, string> PluginFiles => pluginFiles;

        public bool IsAvailable => lua != null;

        public void Initialize(Editor ed)
        {
            editor = ed;

            try
            {
                lua = new Lua();
                lua.LoadCLRPackage();

                // Register bridge functions
                lua.RegisterFunction("tedit_command", this, GetType().GetMethod(nameof(LuaCommand)));
                lua.RegisterFunction("tedit_echo", this, GetType().GetMethod(nameof(LuaEcho)));
                lua.RegisterFunction("tedit_print", this, GetType().GetMethod(nameof(LuaPrint)));
                lua.RegisterFunction("tedit_get_line", this, GetType().GetMethod(nameof(LuaGetLine)));
                lua.RegisterFunction("tedit_set_line", this, GetType().GetMethod(nameof(LuaSetLine)));
                lua.RegisterFunction("tedit_line_count", this, GetType().GetMethod(nameof(LuaLineCount)));
                lua.RegisterFunction("tedit_insert_line", this, GetType().GetMethod(nameof(LuaInsertLine)));
                lua.RegisterFunction("tedit_delete_line", this, GetType().GetMethod(nameof(LuaDeleteLine)));

                LoadPlugins();
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{Ansi.Red}lua: initialization failed: {ex.Message}{Ansi.Reset}");
                lua?.Dispose();
                lua = null;
            }
        }

        public void LoadPlugins()
        {
            pluginNames.Clear();
            pluginFiles.Clear();

            if (lua == null) return;

            string dir = Paths.PluginsDir();
            if (!Directory.Exists(dir)) return;

            foreach (var file in Directory.GetFiles(dir, "*.lua"))
            {
                string name = Path.GetFileNameWithoutExtension(file);
                pluginNames.Add(name);
                pluginFiles[name] = file;
            }

            pluginNames.Sort();
        }

        public bool RunPlugin(string nameOrPath)
        {
            if (lua == null)
            {
                Console.WriteLine($"{editor?.Palette.Err}lua: not available{Ansi.Reset}");
                return false;
            }

            string path;
            string displayName;

            if (pluginFiles.TryGetValue(nameOrPath, out string? pluginPath))
            {
                path = pluginPath;
                displayName = nameOrPath;
            }
            else
            {
                path = Paths.ExpandHome(nameOrPath);
                if (!File.Exists(path))
                {
                    Console.WriteLine($"{editor?.Palette.Err}run-plugin: not found: {nameOrPath}{Ansi.Reset}");
                    return false;
                }
                displayName = Path.GetFileNameWithoutExtension(path);
            }

            // security scan
            string content = File.ReadAllText(path);
            var dangerousPatterns = new[] { "os.execute", "io.popen", "loadfile", "dofile", "package.loadlib" };
            var found = new List<string>();

            foreach (var pattern in dangerousPatterns)
            {
                if (content.Contains(pattern))
                    found.Add(pattern);
            }

            if (found.Count > 0)
            {
                Console.WriteLine($"{editor?.Palette.Warn}Warning: Plugin uses potentially dangerous functions: {string.Join(", ", found)}{Ansi.Reset}");
                Console.Write($"{editor?.Palette.Warn}Run anyway? [y/N] {Ansi.Reset}");
                var key = Console.ReadKey();
                Console.WriteLine();
                if (key.KeyChar != 'y' && key.KeyChar != 'Y')
                {
                    Console.WriteLine($"{editor?.Palette.Dim}Aborted.{Ansi.Reset}");
                    return false;
                }
            }

            try
            {
                lua.DoFile(path);
                CurrentPlugin = displayName;
                Console.WriteLine($"{editor?.Palette.Ok}run-plugin: loaded {displayName}{Ansi.Reset}");
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{editor?.Palette.Err}run-plugin: {ex.Message}{Ansi.Reset}");
                return false;
            }
        }

        public void Execute(string code)
        {
            if (lua == null)
            {
                Console.WriteLine($"{editor?.Palette.Err}lua: not available{Ansi.Reset}");
                return;
            }

            try
            {
                lua.DoString(code);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{editor?.Palette.Err}lua: {ex.Message}{Ansi.Reset}");
            }
        }

        public void ExecuteFile(string path)
        {
            if (lua == null)
            {
                Console.WriteLine($"{editor?.Palette.Err}lua: not available{Ansi.Reset}");
                return;
            }

            path = Paths.ExpandHome(path);
            if (!File.Exists(path))
            {
                Console.WriteLine($"{editor?.Palette.Err}luafile: not found: {path}{Ansi.Reset}");
                return;
            }

            try
            {
                lua.DoFile(path);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"{editor?.Palette.Err}luafile: {ex.Message}{Ansi.Reset}");
            }
        }

        // lua bridge functions
        public void LuaCommand(string cmd)
        {
            editor?.Handle(cmd);
        }

        public void LuaEcho(string msg)
        {
            Console.WriteLine($"{editor?.Palette.Accent}{msg}{Ansi.Reset}");
        }

        public void LuaPrint(long lineNum)
        {
            if (editor == null) return;
            if (lineNum >= 1 && lineNum <= editor.Buffer.LineCount)
                editor.PrintLine((int)lineNum);
        }

        public string? LuaGetLine(long lineNum)
        {
            if (editor == null) return null;
            if (lineNum >= 1 && lineNum <= editor.Buffer.LineCount)
                return editor.Buffer.Lines[(int)lineNum - 1];
            return null;
        }

        public void LuaSetLine(long lineNum, string text)
        {
            if (editor == null) return;
            if (lineNum >= 1 && lineNum <= editor.Buffer.LineCount)
            {
                editor.Buffer.Lines[(int)lineNum - 1] = text;
                editor.Buffer.Dirty = true;
            }
        }

        public long LuaLineCount()
        {
            return editor?.Buffer.LineCount ?? 0;
        }

        public void LuaInsertLine(long afterLine, string text)
        {
            if (editor == null) return;
            int pos = Math.Clamp((int)afterLine, 0, editor.Buffer.LineCount);
            editor.Buffer.Lines.Insert(pos, text);
            editor.Buffer.Dirty = true;
        }

        public void LuaDeleteLine(long lineNum)
        {
            if (editor == null) return;
            if (lineNum >= 1 && lineNum <= editor.Buffer.LineCount)
            {
                editor.Buffer.Lines.RemoveAt((int)lineNum - 1);
                editor.Buffer.Dirty = true;
            }
        }

        public void Dispose()
        {
            lua?.Dispose();
            lua = null;
        }
    }
}
