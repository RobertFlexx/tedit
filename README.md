# tedit — A Minimal Yet Mighty Command-Line Text Editor (this is not a shell! it has shell-like behavior.)

`tedit` is a mac-os safe, lightweight, command-line text editor inspired by *ed*/*ex*—with modern safety, syntax highlighting, themes, and quality-of-life extras in one fast, portable binary. Check out [this](https://github.com/Kokonico/medit)! :D

> psst.. bored? have no computer power for a physics game? dont want gigabytes of bloat? want to soothe your nerves? oh boy well i got the game for you! check out my [PowderSandbox](https://github.com/RobertFlexx/Powder-Sandbox-GameHub) Family, a tree of one powder sandbox written in multiple languages! pick your fire!
---

## Highlights (What’s New & Notable)

* **Fast, simple, dependency-free** — pure C++17 single binary.

* **Modern safety**

  * Atomic saves (write to `.tmp` → `rename`).
  * Optional backups (`filename~`).
  * Undo/Redo stack (up to 200 ops).
  * **Crash recovery & autosave snapshots** (periodic save to `~/.tedit-recover-*`).

* **Smart CLI** Command history, tab completion (commands first-word, filesystem after), and directory-only completion for `cd`.

* **Syntax highlighting (auto-detect)** C/C++, Python, Shell, Ruby, JS/TS, HTML, CSS, JSON (toggle with `highlight on/off`, override via `set lang <name>`).

* **Themes** built-in: `default`, `dark`, `neon`, `matrix`, `paper`, `yellow`, `iceberg` (`theme <name>`). Plus **Lua themes** from `~/tedit-config/themes` (list them with `lua-themes`, load with `theme <name>`).

* **Buffers (multi-file)** `new`, `bnext`, `bprev`, `lsb` to hop between files.

* **Diff on demand** `diff` shows changes vs on-disk file.

* **Shell filters** Pipe ranges through any command: `filter 1-20 !sort`.

* **Range-aware write & read** `write [range] <path>`, `read <path> [n]`.

* **Display controls** `set wrap on/off`, `set truncate on/off`, line numbers toggle via `set number on/off` (or `number`).

* **Hooks & flair**

  * Run `~/.tedit/hooks/on_save` and `~/.tedit/hooks/on_quit` if present.
  * Optional banner from `~/.tedit_banner`.
  * Confetti on successful saves (because… joy).

* **Privileged saves (doas)** If a save fails for permissions, `tedit` can write to a temp file and `doas mv` it into place (configure `doas` as needed).

* **Lua scripting & plugins**

  * Embedded **Lua 5.4** runtime (if built with Lua dev headers/libs).
  * Lua helpers exposed: `tedit_command(cmd)`, `tedit_echo(text)`, `tedit_print(line_number)`.
  * Auto-loads `*.lua` files from `~/tedit-config/plugins` at startup.
  * `:plugins` shows loaded plugins; `:reload-plugins` reloads from disk.
  * `:lua <code>` runs inline Lua; `:luafile <path>` runs a Lua script file.
  * **Lua themes**: drop `*.lua` files into `~/tedit-config/themes`, list them with `lua-themes`, and apply via `theme <name>`.

For more information about Plugins & Themes for Tedit, see [here](https://github.com/RobertFlexx/tedit/blob/main/How%20Plugins%20Work.md).

---

## Clone & Use the Tools (with sudo, git clone project first)

```bash
git clone https://github.com/RobertFlexx/tedit
cd tedit
sudo sh init.sh            # one-time: installs PATH wrappers (tedit-install/update/uninstall)
sudo tedit-install    # build + install to /usr/local/bin
sudo tedit-update     # fetch latest updates, rebuild, reinstall
sudo tedit-uninstall  # remove binary + man page + PATH entry
```

### What `init.sh` does (one-time, must run in git directory)

* Creates small wrapper commands in your **PATH** so you can run the project tools **from anywhere**:

  * `tedit-install`, `tedit-update`, `tedit-uninstall`
  * By default they’re placed in `~/.local/bin` (or in `/usr/local/bin` if you run `sh init.sh --system`).
* Records the path to **this clone** so wrappers know where to run. Each wrapper will:

  * `cd` into the recorded repo directory before executing.
  * For updates, call the underlying scripts (e.g., `update.sh`) which can `git fetch/pull` and then rebuild.
* Ensures `~/.local/bin` is on your `PATH` (appends to your shell profile if needed).
* Idempotent: safe to re-run. If you reclone somewhere else later, run `sh init.sh` again in the new clone to repoint the wrappers.
* Use `sudo` with the wrappers when you want system-wide installs/updates.

> After running `init.sh` once, you can use **`tedit-install`**, **`tedit-update`**, and **`tedit-uninstall`** from **any directory**.

---

## Why a “cd-first” workflow?

Short answer: **safety, atomicity, and predictability.** Longer answer:

1. **Atomic saves need same-filesystem renames.** `rename(2)` is only atomic within the same mount. Working **inside the target directory** guarantees your save stays atomic (no partial files if the power blips).

2. **Tools expect the project’s CWD.** Filters (`filter … !cmd`), hooks, and many build scripts assume they run from the project root. `cd` keeps that assumption true and your workflow friction-free.

3. **Cleaner mental model & completion.** You get directory-only tab completion for `cd`, sensible relative paths, and fewer “oops I saved to the wrong place” moments.

> Can you still open by path? **Yes.** `tedit /etc/hosts` works; `open path/to/file` works. The **recommended** workflow is `cd project && tedit`, but it’s *not* a hard requirement.

---

## Getting Started (Beginner-Friendly Source Build Guide)

### Install

```bash
sudo tedit-install    # or: sudo sh install.sh
```

This script will:

1. Detect your package manager (apt, dnf, pacman, eopkg, etc.).
2. Install build dependencies if missing (`make`, `g++`, etc.).
3. Build `tedit`.
4. Install it to `/usr/local/bin` (or `~/.local/bin` if you don’t have sudo/doas).
5. Add it to your PATH if needed.

### Update

```bash
sudo tedit-update     # or: sudo sh update.sh
```

This checks for git updates. If `tedit` is already current, it will say:

```
Tedit is up to date.
```

Otherwise, it will pull the latest changes, rebuild, and reinstall automatically.

### Uninstall

```bash
sudo tedit-uninstall  # or: sudo sh uninstall.sh
```

This cleanly removes `tedit` from your system and any PATH entries added by the installer.

### Manual Install

If you prefer to do it yourself:

```bash
make
sudo make install
```

> Requires a C++17 compiler (e.g., `g++`). Works on Linux/macOS/BSD. Windows users: use WSL.

### Open a file

```bash
tedit notes.txt     # or just: tedit   (start empty, open later)
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

| Command                           | Description                                                                     |      |             |                |
| --------------------------------- | ------------------------------------------------------------------------------- | ---- | ----------- | -------------- |
| `open <file>`                     | Open a file                                                                     |      |             |                |
| `w` / `write`                     | Save                                                                            |      |             |                |
| `wq`                              | Save & quit                                                                     |      |             |                |
| `q`                               | Quit (prompts if unsaved)                                                       |      |             |                |
| `p [range]` / `r <n>`             | Print lines / show one line                                                     |      |             |                |
| `a` / `i <n>`                     | Append / insert before line *n* (`.` alone to finish)                           |      |             |                |
| `d [range]` / `m <from> <to>`     | Delete range / move a line                                                      |      |             |                |
| `join [range]`                    | Join lines into one                                                             |      |             |                |
| `find` / `findi` / `findre`       | Search (plain / case-insensitive / regex)                                       |      |             |                |
| `n` / `N`                         | Next / previous search hit                                                      |      |             |                |
| `repl old new` / `replg …`        | Replace first / replace globally per line                                       |      |             |                |
| `undo` / `redo`                   | History navigation                                                              |      |             |                |
| `goto <n>`                        | Jump to line *n*                                                                |      |             |                |
| `read <path> [n]`                 | Insert file after line *n* (default: end)                                       |      |             |                |
| `write [range] <path>`            | Write range out to a new path                                                   |      |             |                |
| `filter <range> !cmd`             | Pipe range through shell and replace                                            |      |             |                |
| `theme <name>`                    | `default`, `dark`, `neon`, `matrix`, `paper`, `yellow`, `iceberg` or Lua themes |      |             |                |
| `highlight on/off`                | Toggle syntax highlighting                                                      |      |             |                |
| `set number                       | backup                                                                          | wrap | truncate …` | Editor toggles |
| `set autosave <sec>`              | Autosave interval for crash recovery snapshots                                  |      |             |                |
| `set lang <name>`                 | Force a syntax (`cpp, python, sh, rb, js, html, css…`)                          |      |             |                |
| `alias <from> <to…>`              | Define command aliases                                                          |      |             |                |
| `new` / `bnext` / `bprev` / `lsb` | Multi-buffer workflow                                                           |      |             |                |
| `diff`                            | Show changes vs on-disk                                                         |      |             |                |
| `ls [-a] [-l] [path]` / `pwd`     | Directory helpers                                                               |      |             |                |
| `cd <dir>`                        | Change directory (use `~`, `.`, `..`)                                           |      |             |                |
| `clear`                           | Clear screen + scrollback                                                       |      |             |                |
| `lua <code>`                      | Run inline Lua code                                                             |      |             |                |
| `luafile <path>`                  | Run a Lua script file                                                           |      |             |                |
| `plugins` / `reload-plugins`      | List or reload Lua plugins from `~/tedit-config/plugins`                        |      |             |                |
| `lua-themes`                      | List Lua themes from `~/tedit-config/themes`                                    |      |             |                |

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
* Any `*.lua` file in that directory is auto-loaded on startup.
* Use `:plugins` to list them, `:reload-plugins` to re-load without restarting.

### Simple plugin example (even if you don't know Lua)

Create `~/tedit-config/plugins/hello.lua` with:

```lua
-- Print a greeting when tedit starts
if tedit_echo then
  tedit_echo("hello from lua plugin!")
end
```

That’s it. You don’t need to know Lua to use this: just copy the file, change the message text if you like, and restart `tedit`. On startup you should see the greeting.

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

### Simple Lua theme example (copy-paste friendly)

1. Create the folder if it doesn’t exist:

   ```bash
   mkdir -p ~/tedit-config/themes
   ```

2. Create `~/tedit-config/themes/pink.lua` with this content:

   ```lua
   -- ~/.tedit-config/themes/pink.lua
   -- You can copy‑paste this even if you don't know Lua.
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
2. Run `doas mv /tmp/… <target>`.

Set up your `doas` policy accordingly, or run `tedit` as root when editing system files.

---

## Example Session

```bash
$ tedit main.cpp
tedit — editing main.cpp (120 lines). Type 'help'.

tedit> find main
match at 4: int main() {
match at 97: // main loop
tedit> diff
--- main.cpp
+++ /tmp/tedit_diff_XXXXXX
@@ …
tedit> wq
```

---

## Philosophy

`tedit` follows the Unix idea of **doing one thing well**: editing text with predictable, script-friendly commands—while still giving you modern comforts like highlighting, undo, themes, Lua scripting, and safer saves.

---

## License

BSD-3-Clause. Have fun, keep the notice, and ship great things. :P

---

## Tooling UX (Installer/Updater/Uninstaller)

### What changed (TL;DR)

* **Cinematic UI:** one clean progress bar + subtle spinner for long steps (TTY-aware).
* **Smarter deps:** detects your package manager and installs only what’s missing.
* **Gentoo note:** clearly shown in **yellow** when emerging toolchains (may be interactive).
* **Quiet git:** updater keeps git chatter in a log; your terminal stays neat.
* **“Up to date.”** Updater checks the remote; if nothing to pull, it prints exactly that.
* **Repo auto-discovery:** wrappers/update can find your clone even when run from anywhere.

---

### Using the Wrappers (recommended)

After you run `init.sh` once (in the repo), you can do everything from anywhere:

```bash
# Build + install (system-wide)
sudo tedit-install        # or: doas tedit-install

# Update (pull, rebuild, reinstall)
sudo tedit-update         # prints "Up to date." if there’s nothing new

# Uninstall (clean removal)
sudo tedit-uninstall
```

> No sudo/doas? The tools auto-fallback to **user installs** under `~/.local` when possible.

---

### Install (cinematic)

```bash
# From inside the clone:
sudo sh install.sh         # uses a cinematic bar + spinner
# or with wrappers after init.sh:
sudo tedit-install
```

What it does:

1. Detects your **package manager** (apt, dnf/yum, pacman, zypper, apk, xbps-install, eopkg, emerge, brew).
2. Installs **missing deps only** (`make`, a C++17 compiler, etc.).
3. Builds and installs to `/usr/local/bin` (or `~/.local/bin` without sudo/doas).
4. Installs the **man page** (if present) and refreshes the man DB (best-effort).
5. Adds the install bin dir to your **PATH** if needed.

**Gentoo:** you’ll see `Gentoo detected — emerging toolchain (may be interactive).` in yellow.

---

### Update (smart + quiet)

```bash
# From anywhere (after init.sh):
sudo tedit-update
# Or directly in the repo:
sudo sh update.sh
```

What it does:

* **Scans dependencies** first and installs what’s missing.
* Warms up sudo/doas once to avoid surprise prompts mid-spinner.
* **Quietly fetches** remote changes, compares your HEAD to upstream, and:

  * Prints **`Up to date.`** if there’s nothing to pull.
  * Otherwise, pulls/rebases, **auto-stashes** dirty trees and restores them after.
* Rebuilds and reinstalls with the same cinematic UI.
* Strips the binary (if `strip` exists).

> Tip: if your wrappers ever lose track of the repo path, set it once:

```bash
sudo mkdir -p /usr/local/share/tedit
printf '%s\n' "/absolute/path/to/your/tedit" | sudo tee /usr/local/share/tedit/repo >/dev/null
```

Or export per-call:

```bash
TEDIT_REPO="/absolute/path/to/tedit" sudo tedit-update
```

---

### Uninstall (now with purge options)

```bash
# Simple:
sudo tedit-uninstall

# Advanced (from repo or wrapper):
sudo sh uninstall.sh --purge-user-data             # remove ~/.tedit* (rc, banner, hooks, recover)
sudo sh uninstall.sh --purge-repo -y               # also remove THIS git repo directory (no prompt)
sudo sh uninstall.sh --purge --yes                 # both of the above, non-interactive
```

The uninstaller:

* Removes the binary from common bin dirs and anywhere on `PATH`.
* Removes wrappers (`tedit-install/update/uninstall`).
* Cleans manpages, PATH lines added by the installer, and local build droppings.
* Refreshes the man database (best-effort).
* Supports **purge flags** (see above).

---

### Verbose mode & Logs

* Set `VERBOSE=1` to stream command output instead of the spinner:

  ```bash
  VERBOSE=1 sudo sh install.sh
  VERBOSE=1 sudo sh update.sh
  ```
* All runs write detailed logs:

  * Install: `/tmp/tedit-install.<XXXXXX>.log`
  * Update:  `/tmp/tedit-update.<XXXXXX>.log`
  * Uninstall: `/tmp/tedit-uninstall.<XXXXXX>.log`

If something acts cursed, share the **last ~60 lines** of the relevant log.

---

### Supported Package Managers (auto-detected)

* **Debian/Ubuntu** (`apt`)
* **Fedora/RHEL/CentOS/Rocky/Alma** (`dnf`/`yum`)
* **Arch/Manjaro/Endeavour/Artix** (`pacman`)
* **openSUSE/SLES** (`zypper`)
* **Alpine/Chimera** (`apk`)
* **Void** (`xbps-install`)
* **Solus** (`eopkg`)
* **Gentoo** (`emerge`) — **yellow note**; may be interactive
* **macOS** (`brew`) — may require **Xcode Command Line Tools**

---

### Do I need root?

* **System-wide** install/update/uninstall: use `sudo` **or** `doas`.
* **Per-user** install when you don’t have privileges: the scripts automatically target `~/.local/bin` and ensure it’s on your `PATH`.

---

### Wrapper Internals (FYI)

* `init.sh` records the repo path so wrappers can **cd into the clone** before running tools.
* `update.sh` can also discover the repo via:

  * `TEDIT_REPO=/path/to/tedit`
  * Marker file: `/usr/local/share/tedit/repo`
  * Common fallback: `$HOME/tedit`

This keeps `tedit-update` working even if you call it from a random directory.

---

### Common Tasks (tooling quickies)

```bash
# Fresh setup
git clone https://github.com/RobertFlexx/tedit
cd tedit
sudo sh init.sh
sudo tedit-install

# Update later
sudo tedit-update

# Local-only install (no sudo)
sh install.sh

# Nuke everything (careful)
sudo sh uninstall.sh --purge --purge-repo -y
```

---

### Troubleshooting

* **`Up to date.` but your local changes aren’t present?** You’re already on the latest commit. If you expected different code, check your remote/branch or run `git remote -v && git status`.
* **Dirty tree errors?** The updater **auto-stashes** and pops after pulling. If it can’t auto-resolve, it’ll leave a stash and note it in the log.
* **PATH still missing?** Re-open your shell or run:

  ```bash
  export PATH="/usr/local/bin:$PATH"         # system
  export PATH="$HOME/.local/bin:$PATH"       # user
  ```
* **Man page not found?** Some distros don’t compress/update man DB automatically. Reinstall and verify `man -w tedit`, or run `mandb`/`makewhatis` (root may be required).
