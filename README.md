# tedit command-line text editor

`tedit` is a lightweight command-line text editor for Linux, BSD, and macOS. Its command model follows `ed` and `ex`, with atomic saves, recovery, syntax highlighting, multiple buffers, Lua plugins, and optional language-server formatting.

> If you are on windows, go [here](https://github.com/RobertFlexx/tedit/tree/tedit-windows)

---

## Highlights (What’s New & Notable)

* **Small C++17 executable** with embedded Lua scripting.

* **Modern safety**

  * Atomic saves (write to `.tmp` → `rename`).
  * Optional backups (`filename~`).
  * Undo/Redo stack (up to 200 ops).
  * **Crash recovery & autosave snapshots** (periodic save to `~/.tedit-recover-*`).

* **Smart CLI** Command history, quoted paths, tab completion (commands first-word, filesystem after), and directory-only completion for `cd`.

* **Comfortable line editing** UTF-8-aware cursor movement, Home/End, Delete, Ctrl+Left/Right, Alt+B/F, Ctrl+U/K/W/L, history arrows, and safe Ctrl+C/Ctrl+D handling.

* **Semantic syntax highlighting** Language-specific rules distinguish control flow, declarations, types, built-ins, function calls, strings, numbers, literals, comments, operators, attributes, keys, tags, and selectors. Auto-detection covers C/C++, Python, Shell, Ruby, JavaScript/TypeScript, HTML, CSS, JSON, Go, Rust, Java, C#, PHP, Markdown, YAML, TOML, SQL, XML/SVG, Swift, Kotlin, Lua, Perl, R, Makefiles, Dockerfiles, INI files, and diffs. Markdown fences use the lexer named after the opening fence. Toggle highlighting with `highlight on/off` or override detection with `set lang <name>`.

* **Indentation and language servers** `indent [range]` applies built-in language rules. `lsp start <server...>` connects to a stdio language server, and `lsp format` applies server formatting edits through undo.

* **Ed compatibility** `set edmode on` enables a quiet prompt, current-line tracking, classic addresses, `p`/`n`/`d`, append/insert input, `s///`, and ed-style modified-buffer quit warnings.

* **Themes** built-in: `default`, `dark`, `neon`, `matrix`, `paper`, `yellow`, `iceberg`, `ocean`, `solarized`, `gruvbox`, `rose` (`theme <name>`). Plus sandboxed **Lua themes** from `~/tedit-config/themes` (list them with `lua-themes`, load with `theme <name>`).

* **Buffers (multi-file)** `new`, `bnext`, `bprev`, `lsb` to hop between files.

* **Diff on demand** `diff` shows changes vs on-disk file.

* **Shell filters** Pipe ranges through any command: `filter 1-20 !sort`.

* **Range-aware write & read** `write [range] <path>`, `read <path> [n]`.

* **Display controls** `set wrap on/off`, `set truncate on/off`, line numbers toggle via `set number on/off` (or `number`).

* **Hooks & flair**

  * Run `~/.tedit/hooks/on_save` and `~/.tedit/hooks/on_quit` if present.
  * Optional banner from `~/.tedit_banner`.
  * Optional color output and a compact save confirmation.

* **Privileged saves (doas)** If a save fails for permissions, `tedit` can write to a temp file and `doas mv` it into place (configure `doas` as needed).

* **Lua scripting & plugins**

  * Embedded **Lua 5.4/5.3/5.2 or LuaJIT** runtime (selected from available development libraries at build time).
  * Lua helpers exposed: `tedit_command(cmd)`, `tedit_echo(text)`, `tedit_print(line_number)`.
  * A namespaced API is also available: `tedit.line_count()`, `get_line`, `set_line`, `insert_line`, `delete_line`, `append_line`, `path`, and `is_dirty`.
  * Scans `*.lua` files from `~/tedit-config/plugins`; execution is explicit via `:run-plugin <name>`.
  * `:plugins` shows available plugins; `:reload-plugins` rescans the directory.
  * `:lua <code>` runs inline Lua; `:luafile <path>` runs a Lua script file.
  * **Lua themes**: drop `*.lua` files into `~/tedit-config/themes`, list them with `lua-themes`, and apply via `theme <name>`.

For more information about Plugins & Themes for Tedit, see [here](https://github.com/RobertFlexx/tedit/blob/main/How%20Plugins%20Work.md).

---

## Install from source

```sh
curl -fsSL https://raw.githubusercontent.com/RobertFlexx/tedit/main/scripts/install.sh | sh
```

No clone required: the script fetches and builds tedit from source, installs the
binary and man page, then cleans up. Regular users install into `~/.local`;
root or `--system` uses `/usr/local`. Missing dependencies use the native package
manager through sudo/doas when needed. To pass options, use `sh -s -- --system`
(or `--prefix PATH`, `--ref TAG`) at the end of the curl command.

## Clone & Use the Tools

```bash
git clone https://github.com/RobertFlexx/tedit
cd tedit
sh scripts/init.sh        # install optional wrappers into ~/.local/bin
tedit-install             # build + install to ~/.local
tedit-update              # fetch, fast-forward, rebuild and reinstall
tedit-uninstall           # remove this installation
```

### What `scripts/init.sh` does (one-time, must run in git directory)

* Creates small wrapper commands in your **PATH** so you can run the project tools **from anywhere**:

  * `tedit-install`, `tedit-update`, `tedit-uninstall`
  * By default they’re placed in `~/.local/bin` (or in `/usr/local/bin` if you run `sh scripts/init.sh --system`).
* Records the path to **this clone** so wrappers know where to run. Each wrapper will:

  * `cd` into the recorded repo directory before executing.
  * For updates, call the underlying scripts (e.g., `scripts/update.sh`) which can `git fetch/pull` and then rebuild.
* Ensures `~/.local/bin` is on your `PATH` (appends to your shell profile if needed).
* Idempotent: safe to re-run. If you reclone somewhere else later, run `sh scripts/init.sh` again in the new clone to repoint the wrappers.
* Use `sudo` with the wrappers when you want system-wide installs/updates.

> After running `scripts/init.sh` once, you can use **`tedit-install`**, **`tedit-update`**, and **`tedit-uninstall`** from **any directory**.

---

## Why a “cd-first” workflow?

Short answer: **safety, atomicity, and predictability.** Longer answer:

1. **Atomic saves need same-filesystem renames.** `rename(2)` is only atomic within the same mount. Working **inside the target directory** guarantees your save stays atomic (no partial files if the power blips).

2. **Tools expect the project's CWD.** Filters (`filter ... !cmd`), hooks, and many build scripts assume they run from the project root. `cd` keeps that assumption true and your workflow friction-free.

3. **Cleaner mental model & completion.** You get directory-only tab completion for `cd`, sensible relative paths, and fewer “oops I saved to the wrong place” moments.

> Can you still open by path? **Yes.** `tedit /etc/hosts` works; `open path/to/file` works. The **recommended** workflow is `cd project && tedit`, but it’s *not* a hard requirement.

---

## Building and installing

### Install

```bash
sh scripts/install.sh          # per-user install from this checkout
sh scripts/install.sh --system # install into /usr/local using sudo/doas as needed
```

This script will:

1. Detect your package manager (apt, dnf, pacman, eopkg, etc.).
2. Verify GNU make, C++17 filesystem support, and Lua headers/libraries; install build dependencies if needed.
3. Rebuild `tedit` from source, including when local build outputs already exist.
4. Install it to `~/.local/bin` for a regular user, or `/usr/local/bin` for root/`--system`.
5. Add it to your PATH if needed.

### Update

```bash
tedit-update     # or: sh scripts/update.sh
```

This checks for git updates. If `tedit` is already current, it will say:

```
Up to date.
```

Otherwise, it fast-forwards the configured upstream branch and uses the same
installer to rebuild and reinstall. Incoming updates require a clean working
tree; dirty or diverged trees are left intact with an error. Use `--no-pull` to
rebuild local edits.

### Uninstall

```bash
tedit-uninstall  # or: sh scripts/uninstall.sh
```

This cleanly removes `tedit` from your system and any PATH entries added by the installer.

### Manual Install

If you prefer to do it yourself:

```bash
make
sudo make install
```

Or with Meson:

```bash
meson setup build
meson compile -C build
sudo meson install -C build
```

> Requires GNU make, a C++17 compiler with filesystem support, and Lua development
> headers/libraries. Meson also requires Meson and Ninja. Works on Linux/macOS/BSD.
> Windows users: use WSL.

### Open a file

```bash
tedit notes.txt              # open one file
tedit a.txt b.txt c.txt      # open extra files as buffers
tedit --version              # print version
tedit --help                 # print CLI usage
tedit --no-banner --no-status notes.txt
tedit                        # start empty, open later
```

### View the Man Page

`tedit` installs a proper manual page with the rest of the system manpages. Once installed, simply run:

```bash
man tedit
```

to view full documentation, command references, and usage examples.

### Five-Minute Tutorial

```text
:help                 # overview of commands (':' optional)
:set number on        # show line numbers
:open src/main.cpp    # open a file (if you started empty)
:find main            # search (plain); try :findre for regex
:repl old new         # replace first occurrence per line
:replg old new        # replace all occurrences per line
:write                # save (atomic, backup optional)
:wq                   # save & quit
```

**Pro tip:** Work from the project folder.

```bash
cd my-project
tedit
tedit> open src/app.py
tedit> diff
tedit> wq
```

---

## Basic Commands (Cheat Sheet)

| Command | Description |
| --- | --- |
| `help <command>` | Show focused help for a command |
| `open <file>` | Open a file, including `~` paths |
| `w` / `write` / `w!` | Save, with `w!` skipping backup |
| `write [range] <path>` | Write selected lines to a new path |
| `wq` | Save and quit |
| `q` / `q!` | Quit with prompt, or force quit without saving |
| `p [range]` / `r <n>` | Print lines or show one line |
| `a` / `i <n>` / `edit <n>` | Append, insert, or edit lines |
| `d [range]` / `m <from> <to>` / `join [range]` | Delete, move, or join lines |
| `find` / `findi` / `findre [-i]` / `findrei` | Search plain text or regex |
| `n` / `N` | Next or previous search hit |
| `repl old new` / `replg old new` | Replace first or all matches per line |
| `undo` / `redo` | History navigation |
| `goto <n>` | Jump to line *n* |
| `read <path> [n]` | Insert file after line *n* |
| `filter <range> !cmd` | Pipe range through shell and replace |
| `set` / `set <name> <value>` | Show or change settings |
| `indent [range]` | Reindent with built-in language rules |
| `lsp start ...` / `lsp format` / `lsp stop` | Use stdio language-server formatting |
| `edmode on/off` | Toggle classic ed-compatible behavior |
| `syntax <name>` | Alias for `set lang <name>` |
| `theme <name>` / `theme preview` | Apply or preview themes |
| `highlight on/off` | Toggle syntax highlighting |
| `alias <from> <to...>` | Define command aliases |
| `new` / `bnext` / `bprev` / `lsb` / `buffer <n>` / `close` | Multi-buffer workflow |
| `config` / `recent` / `messages` | Show paths, recent files, or message log |
| `history` / `aliases` | Show session command history or configured aliases |
| `diff` | Show changes vs on-disk |
| `ls [-a] [-l] [path]` / `pwd` / `cd <dir>` | Directory helpers |
| `clear` | Clear screen and scrollback |
| `lua <code>` / `luafile <path>` | Run Lua code or a Lua script file |
| `run-plugin <name>` | Run Lua plugin, invoked as `:run-plugin <name>` |
| `plugin trust <name|path>` / `plugin trusted` | Manage trusted plugin warning sources |
| `plugins` / `reload-plugins` | List or rescan Lua plugins |
| `lua-themes` | List Lua themes from `~/tedit-config/themes` |

---

## Configuration

`tedit` saves preferences in `~/.teditrc`. Example:

```ini
theme=neon
highlight=on
number=on
backup=on
autosave=120
wrap=on
truncate=off
status=on
alias    dd      delete 1-$
alias    wq!     wq
```

**Hooks** (optional):

* `~/.tedit/hooks/on_save`
* `~/.tedit/hooks/on_quit`

**Banner** (optional):

* `~/.tedit_banner` (printed at startup)

**Lua plugins** (optional):

* Stored under `~/tedit-config/plugins`.
* Any `*.lua` file in that directory is discovered on startup but is not executed automatically.
* Use `:plugins` to list them, `:reload-plugins` to rescan, and `:run-plugin <name>` to execute one.

### Lua plugin example

Create `~/tedit-config/plugins/hello.lua` with:

```lua
-- Print a greeting when this plugin is run.
if tedit_echo then
  tedit_echo("hello from lua plugin!")
end
```

Run it with `:run-plugin hello`.

Within Lua you can also:

```lua
-- run an editor command
if tedit_command then
  tedit_command("set number on")
end

-- print a specific line (1-based)
if tedit_print then
  tedit_print(1)
end
```

**Lua themes** (optional):

* Stored under `~/tedit-config/themes`.
* Any `*.lua` file there can be selected as a theme with `theme <name>`.
* Use `lua-themes` inside `tedit` to list Lua themes that are available.

### Lua theme example

1. Create the folder if it doesn’t exist:

   ```bash
   mkdir -p ~/tedit-config/themes
   ```

2. Create `~/tedit-config/themes/pink.lua` with this content:

   ```lua
   -- ~/.tedit-config/themes/pink.lua
   -- Theme fields accept ANSI SGR color sequences.
   -- Save, then in tedit run:  lua-themes   and   theme pink

   return {
     accent     = "\27[95m", -- magenta (used for prompts/titles)
     ok         = "\27[92m", -- green (success messages)
     warn       = "\27[93m", -- yellow (warnings)
     err        = "\27[91m", -- red (errors)
     dim        = "\27[90m", -- dim text / hints
     prompt     = "\27[95m",
     input      = "\27[97m",
     gutter     = "\27[90m",
     title      = "\27[95m",
     help_cmd   = "\27[95m",
     help_arg   = "\27[96m",
     help_text  = "\27[90m",
     syntax_control  = "\27[95m",
     syntax_type     = "\27[96m",
     syntax_function = "\27[94m",
     syntax_string   = "\27[92m",
     syntax_comment  = "\27[90m",
   }
   ```

3. Start `tedit`, then run:

   ```text
   lua-themes      # to see 'pink' listed
   theme pink      # to activate it
   ```

You don’t have to understand the Lua syntax here; the important part is the `return { ... }` table. You can experiment later by changing one color at a time and re-running `theme pink`.

### ⚠️ Plugin & theme safety (WARNING / DISCLAIMER)

Lua plugins and Lua themes are just **Lua scripts**. That means they can, in principle:

* Read, modify, or delete files you have access to.
* Run shell commands indirectly (e.g., via helpers you expose, or other Lua APIs if you add them).
* Interact with your system in ways that are not obvious from a quick glance.

**Treat any third-party plugin or theme like you would a shell script from the internet.**

* Only install plugins/themes from sources you genuinely trust.
* Prefer small, readable scripts you can skim yourself.
* Be especially careful with plugins that claim to "optimize", "clean up", or "manage" files or system config.
* Never give random people root/doas/sudo access just because a plugin suggests it.

If in doubt, **don’t run it**. Stick to simple, local tweaks (like the examples above) or review the code with someone you trust.

---

## Privileged Saves (Editing System Files)

If saving fails with `EACCES`, `tedit` can:

1. Write a temp file in `/tmp`, then
2. Run `doas mv /tmp/... <target>`.

Set up your `doas` policy accordingly, or run `tedit` as root when editing system files.

---

## Example Session

```bash
$ tedit main.cpp
tedit - editing main.cpp (120 lines). Type 'help'.

tedit> find main
match at 4: int main() {
match at 97: // main loop
tedit> diff
--- main.cpp
+++ /tmp/tedit_diff_XXXXXX
@@ ...
tedit> wq
```

---

## Philosophy

`tedit` follows the Unix idea of **doing one thing well**: editing text with predictable, script-friendly commands - while still giving you modern comforts like highlighting, undo, themes, Lua scripting, and safer saves.

---

## License

BSD-3-Clause. Have fun, keep the notice, and ship great things. :P

---

## Tooling (Installer/Updater/Uninstaller)

### Supported Package Managers (auto-detected)

* **Debian/Ubuntu and derivatives** (`apt-get`)
* **Fedora/RHEL-compatible systems** (`dnf`/`yum`)
* **Arch/Manjaro/Endeavour/Artix** (`pacman`)
* **openSUSE/SLES** (`zypper`)
* **Alpine/postmarketOS/Chimera** (`apk`)
* **Void** (`xbps-install`)
* **Solus** (`eopkg`)
* **Gentoo** (`emerge`)
* **FreeBSD/DragonFly, OpenBSD, NetBSD** (`pkg`, `pkg_add`, `pkgin`)
* **macOS** (`brew`/`port`) - requires Xcode Command Line Tools

Dependencies are GNU make, a C++17 compiler with filesystem support, and Lua
development headers/libraries. The installer checks these by compiling and
linking, installs native packages if needed, and checks again before continuing.
Distro derivatives use `ID_LIKE`. On NixOS or Guix, enter a build environment
with make, a C++ compiler, pkg-config, Lua and Git, then use `--no-deps`.

### Common options

```sh
sh scripts/install.sh --deps-only                  # dependencies only
sh scripts/install.sh --no-deps                    # use existing dependencies
sh scripts/install.sh --prefix "$HOME/apps/tedit" # custom install
sh scripts/install.sh --no-man --no-strip --no-path
sh scripts/update.sh --no-pull                     # rebuild local edits
sh scripts/update.sh --no-install                  # fetch/build only
sh scripts/uninstall.sh --dry-run                  # preview removal
```

`PREFIX` also selects a prefix. `TEDIT_REPO` selects an existing checkout;
`TEDIT_REPO_URL` and `TEDIT_REF` select the source URL and branch/tag when cloning.
`CXX`, `MAKE`, `PKG_CONFIG`, `LUA_CFLAGS` and `LUA_LIBS` can override detection.
Use `--help` on any script for its full options.

### Wrappers and updates

Run `scripts/init.sh` once to install wrappers. Use `--system`, `--user`, or
`--bin-dir PATH` to select their destination. `--show` prints paths without
installing; `--force` replaces existing commands not generated by tedit.
User wrappers record the checkout in `${XDG_DATA_HOME:-$HOME/.local/share}/tedit/repo`;
system wrappers use `/usr/local/share/tedit/repo`. Rerun init after moving a clone,
or set `TEDIT_REPO=/absolute/path/to/tedit` for a single invocation.

Updates use the configured Git upstream and accept only fast-forwards. Incoming
changes require a clean tree; local work and diverged history are preserved.
`--no-pull` builds local edits. `Up to date.` requires a successful installation
of the current revision. Fetch, dependency, build and install failures return
an error and leave details in the log.

### Uninstall and purge

Uninstall removes the selected prefix's binary, manual, generated wrappers,
metadata and matching PATH entries. Custom wrapper directories use `--bin-dir`.
`--dry-run` leaves files, profiles and data untouched. Package-owned files and
unrecognized wrappers require `--force`. Local build directories are retained.

```sh
sh scripts/uninstall.sh --purge-user-data  # config, plugins, themes and recovery files
sh scripts/uninstall.sh --purge-repo -y    # permanently delete the source checkout
sh scripts/uninstall.sh --purge --yes      # both; cannot be undone
```

Repository deletion requires confirmation, or `--yes` without a terminal.

### Verbose mode & Logs

Use `--verbose` or `VERBOSE=1` to show successful command output. Errors are
always shown. Logs are kept at `/tmp/tedit-<action>.log.<random>` (or `$TMPDIR`);
set `LOG_FILE=/path/to/log` to choose a location. If something fails, share the
last ~60 lines of that log. Run `make test-scripts` for the tooling regression
suite, or `make test` for all tests (Python 3 required).
