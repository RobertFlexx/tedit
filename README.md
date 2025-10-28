# tedit — A Minimal Yet Mighty Command-Line Text Editor

`tedit` is a lightweight, command-line text editor inspired by *ed*/*ex*—with modern safety, syntax highlighting, themes, and quality-of-life extras in one fast, portable binary. Check out [this](https://github.com/Kokonico/medit)! :D

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
* **Themes** `default`, `dark`, `neon`, `matrix`, `paper` (`theme <name>`).
* **Buffers (multi-file)** `new`, `bnext`, `bprev`, `lsb` to hop between files.
* **Diff on demand** `diff` shows changes vs on-disk file.
* **Shell filters** Pipe ranges through any command: `filter 1-20 !sort`.
* **Range-aware write & read** `write [range] <path>`, `read <path> [n]`.
* **Display controls** `set wrap on|off`, `set truncate on|off`, line numbers toggle via `set number on|off` (or `number`).
* **Hooks & flair**

  * Run `~/.tedit/hooks/on_save` and `~/.tedit/hooks/on_quit` if present.
  * Optional banner from `~/.tedit_banner`.
  * Confetti on successful saves (because… joy).
* **Privileged saves (doas)** If a save fails for permissions, `tedit` can write to a temp file and `doas mv` it into place (configure `doas` as needed).
* **New Tools (Utility Scripts)**

  * **install.sh** — Installs dependencies, builds, and installs `tedit` automatically with a progress bar and package manager detection.
  * **uninstall.sh** — Cleanly removes `tedit` from your system, including PATH entries added by the installer.
  * **update.sh** — Checks for git updates. If no updates are found, prints *“Tedit is up to date.”*; otherwise pulls changes, rebuilds, and reinstalls automatically.
  * **man page (`tedit.1`)** — Installed automatically with `make install` or `install.sh`, viewable with `man tedit`.

---

## Clone & Use the Tools (with sudo)

```bash
git clone https://github.com/RobertFlexx/tedit
cd tedit
sh init.sh            # one-time: installs PATH wrappers (tedit-install/update/uninstall)
sudo tedit-install    # build + install to /usr/local/bin
sudo tedit-update     # fetch latest updates, rebuild, reinstall
sudo tedit-uninstall  # remove binary + man page + PATH entry
```

### What `init.sh` does (one-time)

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

## Getting Started (Beginner-Friendly)

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

| Command                           | Description                                            |      |             |                |
| --------------------------------- | ------------------------------------------------------ | ---- | ----------- | -------------- |
| `open <file>`                     | Open a file                                            |      |             |                |
| `w` / `write`                     | Save                                                   |      |             |                |
| `wq`                              | Save & quit                                            |      |             |                |
| `q`                               | Quit (prompts if unsaved)                              |      |             |                |
| `p [range]` / `r <n>`             | Print lines / show one line                            |      |             |                |
| `a` / `i <n>`                     | Append / insert before line *n* (`.` alone to finish)  |      |             |                |
| `d [range]` / `m <from> <to>`     | Delete range / move a line                             |      |             |                |
| `join [range]`                    | Join lines into one                                    |      |             |                |
| `find` / `findi` / `findre`       | Search (plain / case-insensitive / regex)              |      |             |                |
| `n` / `N`                         | Next / previous search hit                             |      |             |                |
| `repl old new` / `replg …`        | Replace first / replace globally per line              |      |             |                |
| `undo` / `redo`                   | History navigation                                     |      |             |                |
| `goto <n>`                        | Jump to line *n*                                       |      |             |                |
| `read <path> [n]`                 | Insert file after line *n* (default: end)              |      |             |                |
| `write [range] <path>`            | Write range out to a new path                          |      |             |                |
| `filter <range> !cmd`             | Pipe range through shell and replace                   |      |             |                |
| `theme <name>`                    | `default`, `dark`, `neon`, `matrix`, `paper`           |      |             |                |
| `highlight on/off`                | Toggle syntax highlighting                             |      |             |                |
| `set number                       | backup                                                 | wrap | truncate …` | Editor toggles |
| `set autosave <sec>`              | Autosave interval for crash recovery snapshots         |      |             |                |
| `set lang <name>`                 | Force a syntax (`cpp, python, sh, rb, js, html, css…`) |      |             |                |
| `alias <from> <to…>`              | Define command aliases                                 |      |             |                |
| `new` / `bnext` / `bprev` / `lsb` | Multi-buffer workflow                                  |      |             |                |
| `diff`                            | Show changes vs on-disk                                |      |             |                |
| `ls [-a] [-l] [path]` / `pwd`     | Directory helpers                                      |      |             |                |
| `cd <dir>`                        | Change directory (use `~`, `.`, `..`)                  |      |             |                |
| `clear`                           | Clear screen + scrollback                              |      |             |                |

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

`tedit` follows the Unix idea of **doing one thing well**: editing text with predictable, script-friendly commands—while still giving you modern comforts like highlighting, undo, themes, and safer saves.

---

## License

BSD-3-Clause. Have fun, keep the notice, and ship great things. :P
