using System;
using System.IO;
using System.Collections.Generic;

namespace TEdit
{
    public enum ThemeType
    {
        Default,
        Dark,
        Neon,
        Matrix,
        Paper,
        Yellow,
        Iceberg
    }

    public class ThemePalette
    {
        public string Accent { get; set; } = "";
        public string Ok { get; set; } = "";
        public string Warn { get; set; } = "";
        public string Err { get; set; } = "";
        public string Dim { get; set; } = "";
        public string Prompt { get; set; } = "";
        public string Input { get; set; } = "";
        public string Gutter { get; set; } = "";
        public string Title { get; set; } = "";
        public string HelpCmd { get; set; } = "";
        public string HelpArg { get; set; } = "";
        public string HelpText { get; set; } = "";

        public static bool UseColor => !Console.IsOutputRedirected;

        public static ThemePalette Create(ThemeType theme)
        {
            if (!UseColor)
                return new ThemePalette();

            return theme switch
            {
                ThemeType.Dark => new ThemePalette
                {
                    Accent = Ansi.Cyan,
                    Ok = Ansi.Green,
                    Warn = Ansi.Yellow,
                    Err = Ansi.Red,
                    Dim = Ansi.BrightBlack,
                    Prompt = Ansi.BrightCyan,
                    Input = Ansi.BrightWhite,
                    Gutter = Ansi.BrightBlack,
                    Title = Ansi.Bold + Ansi.Cyan,
                    HelpCmd = Ansi.BrightCyan,
                    HelpArg = Ansi.BrightBlack,
                    HelpText = Ansi.BrightBlack
                },
                ThemeType.Neon => new ThemePalette
                {
                    Accent = Ansi.BrightMagenta,
                    Ok = Ansi.BrightGreen,
                    Warn = Ansi.BrightYellow,
                    Err = Ansi.BrightRed,
                    Dim = Ansi.BrightBlack,
                    Prompt = Ansi.BrightMagenta,
                    Input = Ansi.BrightCyan,
                    Gutter = Ansi.BrightBlack,
                    Title = Ansi.Bold + Ansi.BrightMagenta,
                    HelpCmd = Ansi.BrightMagenta,
                    HelpArg = Ansi.BrightBlack,
                    HelpText = Ansi.BrightBlack
                },
                ThemeType.Matrix => new ThemePalette
                {
                    Accent = Ansi.Green,
                    Ok = Ansi.BrightGreen,
                    Warn = Ansi.Yellow,
                    Err = Ansi.Red,
                    Dim = Ansi.BrightBlack,
                    Prompt = Ansi.BrightGreen,
                    Input = Ansi.BrightGreen,
                    Gutter = Ansi.BrightBlack,
                    Title = Ansi.Bold + Ansi.Green,
                    HelpCmd = Ansi.BrightGreen,
                    HelpArg = Ansi.BrightBlack,
                    HelpText = Ansi.BrightBlack
                },
                ThemeType.Paper => new ThemePalette
                {
                    Accent = Ansi.BrightBlack,
                    Ok = Ansi.Green,
                    Warn = Ansi.Yellow,
                    Err = Ansi.Red,
                    Dim = Ansi.BrightBlack,
                    Prompt = Ansi.BrightBlack,
                    Input = Ansi.BrightBlack,
                    Gutter = Ansi.BrightBlack,
                    Title = Ansi.Bold + Ansi.BrightBlack,
                    HelpCmd = Ansi.BrightBlack,
                    HelpArg = Ansi.BrightBlack,
                    HelpText = Ansi.BrightBlack
                },
                ThemeType.Yellow => new ThemePalette
                {
                    Accent = Ansi.BrightYellow,
                    Ok = Ansi.BrightGreen,
                    Warn = Ansi.Yellow,
                    Err = Ansi.Red,
                    Dim = Ansi.BrightBlack,
                    Prompt = Ansi.BrightYellow,
                    Input = Ansi.BrightWhite,
                    Gutter = Ansi.BrightBlack,
                    Title = Ansi.Bold + Ansi.BrightYellow,
                    HelpCmd = Ansi.BrightYellow,
                    HelpArg = Ansi.BrightBlack,
                    HelpText = Ansi.BrightBlack
                },
                ThemeType.Iceberg => new ThemePalette
                {
                    Accent = Ansi.BrightCyan,
                    Ok = Ansi.BrightGreen,
                    Warn = Ansi.Yellow,
                    Err = Ansi.Red,
                    Dim = Ansi.BrightBlack,
                    Prompt = Ansi.BrightCyan,
                    Input = Ansi.BrightWhite,
                    Gutter = Ansi.BrightBlack,
                    Title = Ansi.Bold + Ansi.BrightCyan,
                    HelpCmd = Ansi.BrightCyan,
                    HelpArg = Ansi.BrightBlack,
                    HelpText = Ansi.BrightBlack
                },
                _ => new ThemePalette
                {
                    Accent = Ansi.Cyan,
                    Ok = Ansi.Green,
                    Warn = Ansi.Yellow,
                    Err = Ansi.Red,
                    Dim = Ansi.Dim,
                    Prompt = Ansi.BrightCyan,
                    Input = Ansi.BrightWhite,
                    Gutter = Ansi.BrightBlack,
                    Title = Ansi.Bold + Ansi.Cyan,
                    HelpCmd = Ansi.Cyan,
                    HelpArg = Ansi.Dim,
                    HelpText = Ansi.Dim
                }
            };
        }

        public static bool TryParse(string name, out ThemeType theme)
        {
            return Enum.TryParse(name, true, out theme);
        }

        public static string GetName(ThemeType theme)
        {
            return theme.ToString().ToLowerInvariant();
        }

        public static ThemePalette? LoadFromLua(string name)
        {
            string path = Path.Combine(Paths.ThemesDir(), name + ".lua");
            if (!File.Exists(path)) return null;

            try
            {
                using var lua = new NLua.Lua();
                lua.DoFile(path);

                var theme = lua["theme"] as NLua.LuaTable;
                if (theme == null) return null;

                var basePalette = Create(ThemeType.Default);
                var palette = new ThemePalette
                {
                    Accent = GetLuaString(theme, "accent") ?? basePalette.Accent,
                    Ok = GetLuaString(theme, "ok") ?? basePalette.Ok,
                    Warn = GetLuaString(theme, "warn") ?? basePalette.Warn,
                    Err = GetLuaString(theme, "err") ?? basePalette.Err,
                    Dim = GetLuaString(theme, "dim") ?? basePalette.Dim,
                    Prompt = GetLuaString(theme, "prompt") ?? basePalette.Prompt,
                    Input = GetLuaString(theme, "input") ?? basePalette.Input,
                    Gutter = GetLuaString(theme, "gutter") ?? basePalette.Gutter,
                    Title = GetLuaString(theme, "title") ?? basePalette.Title,
                    HelpCmd = GetLuaString(theme, "help_cmd") ?? basePalette.HelpCmd,
                    HelpArg = GetLuaString(theme, "help_arg") ?? basePalette.HelpArg,
                    HelpText = GetLuaString(theme, "help_text") ?? basePalette.HelpText
                };

                return palette;
            }
            catch
            {
                return null;
            }
        }

        private static string? GetLuaString(NLua.LuaTable table, string key)
        {
            var val = table[key];
            return val?.ToString();
        }

        public static List<string> ListLuaThemes()
        {
            var themes = new List<string>();
            string dir = Paths.ThemesDir();

            if (!Directory.Exists(dir)) return themes;

            foreach (var file in Directory.GetFiles(dir, "*.lua"))
            {
                themes.Add(Path.GetFileNameWithoutExtension(file));
            }

            themes.Sort();
            return themes;
        }
    }
}
