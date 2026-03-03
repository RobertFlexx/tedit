using System;
using System.Runtime.InteropServices;

namespace TEdit
{
    public static class Ansi
    {
        public static readonly string Reset = "\x1b[0m";
        public static readonly string Dim = "\x1b[2m";
        public static readonly string Bold = "\x1b[1m";
        public static readonly string Underline = "\x1b[4m";

        public static readonly string Black = "\x1b[30m";
        public static readonly string Red = "\x1b[31m";
        public static readonly string Green = "\x1b[32m";
        public static readonly string Yellow = "\x1b[33m";
        public static readonly string Blue = "\x1b[34m";
        public static readonly string Magenta = "\x1b[35m";
        public static readonly string Cyan = "\x1b[36m";
        public static readonly string White = "\x1b[37m";

        public static readonly string BrightBlack = "\x1b[90m";
        public static readonly string BrightRed = "\x1b[91m";
        public static readonly string BrightGreen = "\x1b[92m";
        public static readonly string BrightYellow = "\x1b[93m";
        public static readonly string BrightBlue = "\x1b[94m";
        public static readonly string BrightMagenta = "\x1b[95m";
        public static readonly string BrightCyan = "\x1b[96m";
        public static readonly string BrightWhite = "\x1b[97m";

        public static readonly string ClearScreen = "\x1b[2J\x1b[H";
        public static readonly string ClearLine = "\x1b[2K";

        public static string MoveCursorLeft(int n) => $"\x1b[{n}D";
        public static string MoveCursorRight(int n) => $"\x1b[{n}C";
    }

    public static class AnsiSupport
    {
        private const int STD_OUTPUT_HANDLE = -11;
        private const int STD_INPUT_HANDLE = -10;
        private const uint ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004;
        private const uint ENABLE_VIRTUAL_TERMINAL_INPUT = 0x0200;

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr GetStdHandle(int nStdHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetConsoleMode(IntPtr hConsoleHandle, out uint lpMode);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetConsoleMode(IntPtr hConsoleHandle, uint dwMode);

        public static bool Enable()
        {
            try
            {
                var outHandle = GetStdHandle(STD_OUTPUT_HANDLE);
                if (outHandle == IntPtr.Zero) return false;

                if (!GetConsoleMode(outHandle, out uint outMode)) return false;

                outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                if (!SetConsoleMode(outHandle, outMode)) return false;

                return true;
            }
            catch
            {
                return false;
            }
        }
    }

    public static class Paths
    {
        public static string Home()
        {
            return Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        }

        public static string AppData()
        {
            return Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        }

        public static string ConfigDir()
        {
            string dir = System.IO.Path.Combine(AppData(), "tedit");
            System.IO.Directory.CreateDirectory(dir);
            return dir;
        }

        public static string PluginsDir()
        {
            string dir = System.IO.Path.Combine(ConfigDir(), "plugins");
            System.IO.Directory.CreateDirectory(dir);
            return dir;
        }

        public static string ThemesDir()
        {
            string dir = System.IO.Path.Combine(ConfigDir(), "themes");
            System.IO.Directory.CreateDirectory(dir);
            return dir;
        }

        public static string ConfigFile()
        {
            return System.IO.Path.Combine(ConfigDir(), "config.ini");
        }

        public static string RecoverFile(string bufferPath)
        {
            string name = string.IsNullOrEmpty(bufferPath) ? "unnamed" : bufferPath;
            int hash = name.GetHashCode();
            return System.IO.Path.Combine(ConfigDir(), $"recover-{hash:X8}.tmp");
        }

        public static string ExpandHome(string path)
        {
            if (string.IsNullOrEmpty(path)) return path;
            if (path == "~") return Home();
            if (path.StartsWith("~\\") || path.StartsWith("~/"))
                return System.IO.Path.Combine(Home(), path.Substring(2));
            return path;
        }

        public static string NormalizePath(string path)
        {
            return path.Replace('/', System.IO.Path.DirectorySeparatorChar);
        }
    }

    public static class StringExtensions
    {
        public static string TrimEndNewlines(this string s)
        {
            return s.TrimEnd('\r', '\n');
        }

        public static bool ParseLong(this string s, out long result)
        {
            return long.TryParse(s.Trim(), out result);
        }

        public static bool ParseBool(this string s, out bool result)
        {
            string lower = s.Trim().ToLowerInvariant();
            if (lower == "1" || lower == "on" || lower == "true" || lower == "yes")
            {
                result = true;
                return true;
            }
            if (lower == "0" || lower == "off" || lower == "false" || lower == "no")
            {
                result = false;
                return true;
            }
            result = false;
            return false;
        }
    }
}
