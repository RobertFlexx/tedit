# TEdit for Windows — A Minimal Yet Mighty Command-Line Text Editor

`tedit` is a lightweight, fast terminal text editor for Windows—inspired by classic *ed*/*ex* with modern safety, syntax highlighting, themes, and quality-of-life extras in one portable executable.


If you are on linux, go [here](https://github.com/RobertFlexx/tedit)
---

## Highlights

* **Fast, simple, portable** — single `.exe`, no external dependencies at runtime
* **Modern safety**

  * Atomic saves (write to `.tmp` → rename)
  * Optional backups (`filename.bak`)
  * Undo/Redo stack (up to 200 ops)
  * Crash recovery & autosave snapshots
* **Smart CLI** — Command history, tab completion (commands + paths), directory-only completion for `cd`
* **Syntax highlighting** — C/C++, C#, Python, PowerShell, JavaScript, HTML, CSS, JSON (toggle with `highlight on/off`)
* **Themes** — Built-in: `default`, `dark`, `neon`, `matrix`, `paper`, `yellow`, `iceberg`. Plus **Lua themes** from config folder
* **Multiple buffers** — `new`, `bnext`, `bprev`, `lsb` to work with multiple files
* **Shell filters** — Pipe ranges through PowerShell: `filter 1-20 !Sort-Object`
* **Diff on demand** — `diff` shows changes vs on-disk file
* **Lua plugins** — Extend functionality with simple Lua scripts

---

## Quick Start

### Requirements

* Windows 10 or later (for ANSI color support)
* .NET 8.0 SDK (for building only)

### Build & Install

```powershell
# Clone the repo
git clone https://github.com/YourUsername/tedit-windows
cd tedit-windows

# Build
.\scripts\build.ps1 -Release

# Install (adds to PATH)
.\scripts\install.ps1 -AddToPath

# Restart your terminal, then:
tedit myfile.txt
```

### Update

```powershell
cd tedit-windows
git pull
.\scripts\build.ps1 -Release
.\scripts\install.ps1 -AddToPath
```

### Uninstall

```powershell
.\scripts\uninstall.ps1
```

---

## Five-Minute Tutorial

```text
help                  # overview of commands (':' optional)
set number on         # show line numbers
open src\main.cs      # open a file
find Main             # search
repl old new          # replace first occurrence per line
replg old new         # replace all occurrences
write                 # save (atomic, backup optional)
wq                    # save & quit
```

**Recommended workflow:**

```powershell
cd C:\Projects\MyApp
tedit
tedit> open Program.cs
tedit> diff
tedit> wq
```

---

## Commands (Cheat Sheet)

| Command                           | Description                                     |
| --------------------------------- | ----------------------------------------------- |
| `open <file>`                     | Open a file                                     |
| `w` / `write`                     | Save                                            |
| `wq`                              | Save & quit                                     |
| `q` / `quit`                      | Quit (prompts if unsaved)                       |
| `p [range]` / `r <n>`             | Print lines / show one line                     |
| `a` / `i <n>`                     | Append / insert before line *n* (`.` to finish) |
| `edit <n> [text]`                 | Edit line *n*                                   |
| `d [range]`                       | Delete lines                                    |
| `m <from> <to>`                   | Move a line                                     |
| `join [range]`                    | Join lines into one                             |
| `find` / `findi` / `findre`       | Search (plain / case-insensitive / regex)       |
| `n` / `N`                         | Next / previous search hit                      |
| `repl old new`                    | Replace first per line                          |
| `replg old new`                   | Replace all per line                            |
| `undo` / `redo`                   | History navigation                              |
| `goto <n>`                        | Jump to line                                    |
| `read <path> [n]`                 | Insert file after line *n*                      |
| `write [range] <path>`            | Write range to file                             |
| `filter <range> !cmd`             | Pipe through PowerShell                         |
| `theme <name>`                    | Change theme                                    |
| `highlight on/off`                | Toggle syntax highlighting                      |
| `set number on/off`               | Toggle line numbers                             |
| `set backup on/off`               | Toggle backup files                             |
| `set wrap on/off`                 | Toggle line wrapping                            |
| `alias <from> <to>`               | Define command alias                            |
| `new` / `bnext` / `bprev` / `lsb` | Multi-buffer workflow                           |
| `diff`                            | Show changes vs saved file                      |
| `ls` / `dir` / `pwd` / `cd`       | Directory navigation                            |
| `clear` / `cls`                   | Clear screen                                    |
| `lua <code>`                      | Run Lua code                                    |
| `:run-plugin <name>`              | Run a Lua plugin                                |
| `plugins`                         | List available plugins                          |
| `lua-themes`                      | List Lua themes                                 |
| `version`                         | Show version                                    |

**Ranges:** Use `1-10`, `5-$` ($ = last line), or just a line number.

---

## Configuration

Settings are saved in `%APPDATA%\tedit\config.ini`:

```ini
theme=neon
highlight=on
number=on
backup=on
autosave=120
wrap=on
```

### Aliases

Define shortcuts in the config or at runtime:

```text
alias dd delete 1-$
alias wq! wq
```

---

## Themes

**Built-in:** `default`, `dark`, `neon`, `matrix`, `paper`, `yellow`, `iceberg`

**Lua themes:** Drop `.lua` files in `%APPDATA%\tedit\themes\`

Example theme (`%APPDATA%\tedit\themes\pink.lua`):

```lua
theme = {
    accent     = "\027[95m",
    ok         = "\027[92m",
    warn       = "\027[93m",
    err        = "\027[91m",
    dim        = "\027[90m",
    prompt     = "\027[95m",
    input      = "\027[97m",
    gutter     = "\027[90m",
    title      = "\027[1m\027[95m",
    help_cmd   = "\027[95m",
    help_arg   = "\027[96m",
    help_text  = "\027[90m",
}
```

Then in tedit: `lua-themes` to see it, `theme pink` to use it.

---

## Plugins

Lua plugins go in `%APPDATA%\tedit\plugins\`

**Simple example** (`%APPDATA%\tedit\plugins\hello.lua`):

```lua
tedit_echo("Hello from plugin!")
```

**Available Lua functions:**

* `tedit_echo(text)` — Print colored message
* `tedit_command(cmd)` — Run an editor command
* `tedit_print(line)` — Print a specific line
* `tedit_get_line(n)` — Get line content
* `tedit_set_line(n, text)` — Set line content
* `tedit_line_count()` — Get total lines
* `tedit_insert_line(after, text)` — Insert a line
* `tedit_delete_line(n)` — Delete a line

**Run plugins with:** `:run-plugin <name>` (colon required for safety)

### ⚠️ Plugin Safety Warning

Plugins are Lua scripts that can read/write files and run commands. **Only use plugins from sources you trust.**

---

## Directory Structure

```
%APPDATA%\tedit\
├── config.ini          # Settings
├── plugins\            # Lua plugins
│   └── example.lua
├── themes\             # Lua themes
│   └── cyberpunk.lua
└── recover-*.tmp       # Auto-recovery files
```

---

## Example Session

```powershell
PS C:\> tedit main.cs
tedit — editing main.cs (45 lines). Type 'help'.

*tedit> find Main
match at 12: static void Main(string[] args)
*tedit> goto 12
  12 | static void Main(string[] args)
*tedit> repl args argv
replaced 1 line(s)
*tedit> diff
Comparing files main.cs and C:\Users\...\Temp\tmp...
*tedit> wq
 *  .  *   . *
.  *  *  .    *
   *  .   *  .
saved to main.cs
bye!
```

---

## Script Reference

| Script                                | Description                       |
| ------------------------------------- | --------------------------------- |
| `scripts\build.ps1`                   | Build the project                 |
| `scripts\build.ps1 -Release`          | Build optimized single-file exe   |
| `scripts\build.ps1 -Clean`            | Clean and rebuild                 |
| `scripts\install.ps1`                 | Install to `%LOCALAPPDATA%\tedit` |
| `scripts\install.ps1 -AddToPath`      | Install and add to user PATH      |
| `scripts\install.ps1 -CreateShortcut` | Create desktop shortcut           |
| `scripts\uninstall.ps1`               | Remove installation               |

You can also use the `.bat` wrappers if PowerShell isn't your default.

---

## Building from Source

### Prerequisites

1. Install [.NET 8.0 SDK](https://dotnet.microsoft.com/download)
2. Clone this repo

### Build Debug

```powershell
.\scripts\build.ps1
```

Output: `build\tedit.exe` (requires .NET runtime)

### Build Release (Single File)

```powershell
.\scripts\build.ps1 -Release
```

Output: `build\tedit.exe` (self-contained, ~15MB)

### Manual Build

```powershell
cd src
dotnet publish -c Release -r win-x64 --self-contained -o ..\build
```

---

## Philosophy

TEdit follows the Unix idea of **doing one thing well**: editing text with predictable, script-friendly commands—while giving you modern comforts like highlighting, undo, themes, Lua scripting, and safe atomic saves.

It's not a shell. It's not a TUI. It's a **command-line text editor** that stays out of your way.

---

## Troubleshooting

**Colors not working?**

* Make sure you're on Windows 10 or later
* Use Windows Terminal, PowerShell 7, or cmd.exe (not ancient terminals)

**Can't find tedit after install?**

* Restart your terminal after adding to PATH
* Or run: `$env:Path = [Environment]::GetEnvironmentVariable("Path", "User")`

**Plugin won't run?**

* Use the colon: `:run-plugin name` not just `run-plugin name`

**Build fails?**

* Make sure .NET 8.0 SDK is installed: `dotnet --version`

---

## License

BSD-3 License. Have fun, keep the notice, ship great things (pls dont screw stuff up lol)

---

## Links

* [NLua](https://github.com/NLua/NLua) — Lua for .NET
* [.NET SDK](https://dotnet.microsoft.com/download)
* [Windows Terminal](https://aka.ms/terminal) — Recommended terminal
