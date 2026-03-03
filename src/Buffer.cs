using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace TEdit
{
    public enum Lang
    {
        Plain, Cpp, CSharp, Python, Shell, PowerShell, Ruby, JS, HTML, CSS, JSON, XML, Markdown
    }

    public class Buffer
    {
        public string Path { get; set; } = "";
        public List<string> Lines { get; set; } = new List<string>();
        public bool Dirty { get; set; } = false;
        public bool ShowLineNumbers { get; set; } = true;
        public bool CreateBackup { get; set; } = true;
        public bool SyntaxHighlight { get; set; } = false;
        public Lang Language { get; set; } = Lang.Plain;

        public long CharCount => Lines.Sum(l => (long)l.Length + 1);
        public int LineCount => Lines.Count;

        public void Clear()
        {
            Lines.Clear();
            Dirty = false;
        }

        public void LoadFromFile(string path)
        {
            Clear();
            Path = path;

            if (!File.Exists(path))
            {
                Dirty = false;
                return;
            }

            foreach (var line in File.ReadAllLines(path))
            {
                Lines.Add(line.TrimEndNewlines());
            }

            Language = DetectLanguage(path);
            Dirty = false;
        }

        public bool SaveToFile(string? targetPath = null)
        {
            string savePath = targetPath ?? Path;
            if (string.IsNullOrEmpty(savePath))
                throw new InvalidOperationException("No filename specified");

            if (CreateBackup && File.Exists(savePath))
            {
                try
                {
                    File.Copy(savePath, savePath + ".bak", overwrite: true);
                }
                catch { /* ignore backup errors */ }
            }

            // atomic write using temp file
            string tempPath = savePath + ".tmp." + Guid.NewGuid().ToString("N")[..8];

            try
            {
                File.WriteAllLines(tempPath, Lines);

                if (File.Exists(savePath))
                    File.Delete(savePath);

                File.Move(tempPath, savePath);

                Path = savePath;
                Dirty = false;

                // remove recovery file
                string recoverPath = Paths.RecoverFile(savePath);
                if (File.Exists(recoverPath))
                    File.Delete(recoverPath);

                return true;
            }
            catch
            {
                if (File.Exists(tempPath))
                    File.Delete(tempPath);
                throw;
            }
        }

        public void AutoSave()
        {
            if (!Dirty || Lines.Count == 0) return;

            try
            {
                string recoverPath = Paths.RecoverFile(Path);
                File.WriteAllLines(recoverPath, Lines);
            }
            catch { /* ignore autosave errors */ }
        }

        public bool TryRecover()
        {
            string recoverPath = Paths.RecoverFile(Path);
            if (!File.Exists(recoverPath)) return false;

            Lines.Clear();
            foreach (var line in File.ReadAllLines(recoverPath))
            {
                Lines.Add(line.TrimEndNewlines());
            }
            Dirty = true;
            return true;
        }

        public static Lang DetectLanguage(string path)
        {
            string ext = System.IO.Path.GetExtension(path).ToLowerInvariant();

            return ext switch
            {
                ".c" or ".cc" or ".cpp" or ".cxx" or ".h" or ".hh" or ".hpp" => Lang.Cpp,
                ".cs" => Lang.CSharp,
                ".py" or ".pyw" => Lang.Python,
                ".sh" or ".bash" or ".zsh" => Lang.Shell,
                ".ps1" or ".psm1" or ".psd1" => Lang.PowerShell,
                ".rb" => Lang.Ruby,
                ".js" or ".mjs" or ".ts" or ".tsx" or ".jsx" => Lang.JS,
                ".html" or ".htm" => Lang.HTML,
                ".css" or ".scss" or ".less" => Lang.CSS,
                ".json" => Lang.JSON,
                ".xml" or ".xaml" or ".csproj" or ".config" => Lang.XML,
                ".md" or ".markdown" => Lang.Markdown,
                _ => Lang.Plain
            };
        }
    }

    public class Snapshot
    {
        public List<string> Lines { get; set; } = new List<string>();

        public Snapshot() { }

        public Snapshot(Buffer buffer)
        {
            Lines = new List<string>(buffer.Lines);
        }
    }

    public class UndoStack
    {
        private const int MaxSize = 200;
        private readonly List<Snapshot> stack = new List<Snapshot>();

        public int Count => stack.Count;

        public void Clear() => stack.Clear();

        public void Push(Buffer buffer)
        {
            if (stack.Count >= MaxSize)
                stack.RemoveAt(0);

            stack.Add(new Snapshot(buffer));
        }

        public bool TryPop(out Snapshot? snapshot)
        {
            snapshot = null;
            if (stack.Count == 0) return false;

            snapshot = stack[^1];
            stack.RemoveAt(stack.Count - 1);
            return true;
        }
    }
}
