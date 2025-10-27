# tedit — A Minimal Yet Mighty Command-Line Text Editor

`tedit` is a lightweight, command-line text editor designed to bring the power of traditional editors like *ed* and *ex* — but with modern touches, safety features, syntax highlighting, and theming, all in one fast and portable binary.

---

## Features

* **Fast, simple, and dependency-free** — written in pure C++17.
* **Modern safety:**

  * Atomic saves (`.tmp` → rename)
  * Optional automatic backups (`filename~`)
  * Undo/Redo stack (up to 200 operations)
* **Smart command-line interface** with autocompletion, command history, and tab suggestions.
* **Syntax highlighting** for multiple languages (C/C++, Python, Ruby, Shell, etc.).
* **Themes** for customizable aesthetics (`:theme neon`, `:theme retro`, etc.).
* **Confetti mode** for fun events such as successful saves.
* **Line numbering** toggle (`:set number on|off` or just `:number`).
* **Find/Replace** (`:find`, `:findre`, `:repl`, `:replg`).
* **Shell filters** — pipe code through any shell command:

  ```sh
  :filter 1-20 !sort
  ```
* **Directory tools** — built-in `:ls` and `:pwd`.
* **Cross-platform** (Linux, macOS, BSD; Windows via WSL).

---

## Getting Started

### Installation

```bash
make
doas make install
```

### Running

```bash
tedit filename.txt
```

or start an empty session:

```bash
tedit
```

---

## Basic Commands

| Command                     | Description                                  |
| --------------------------- | -------------------------------------------- |
| `open <file>`               | Open a file                                  |
| `w` / `write`               | Save file                                    |
| `wq`                        | Save and quit                                |
| `q`                         | Quit (warns if unsaved)                      |
| `p [range]`                 | Print lines                                  |
| `a`                         | Append lines (`.` on its own ends)           |
| `i <n>`                     | Insert before line *n*                       |
| `d [range]`                 | Delete lines                                 |
| `m <from> <to>`             | Move line                                    |
| `join [range]`              | Join lines into one                          |
| `find` / `findi` / `findre` | Search text (plain, case-insensitive, regex) |
| `repl` / `replg`            | Replace text (first / global)                |
| `undo` / `redo`             | History navigation                           |
| `set number on/off`         | Toggle line numbering                        |
| `theme <name>`              | Change color theme                           |
| `highlight on/off`          | Toggle syntax highlighting                   |
| `ls` / `pwd`                | File browser tools                           |

---

## Example

```bash
$ tedit main.cpp
tedit — editing main.cpp (120 lines). Type 'help'.

tedit> :find main
match at 4: int main() {
match at 97: // main loop
```

---

## Advantages Over Traditional TUI Editors

* **Zero terminal control dependencies:** no ncurses or GUI libraries required.
* **Safer editing:** atomic saves prevent corruption during crashes or power loss.
* **Script-friendly:** works perfectly in pipes and automation.
* **Faster startup:** loads instantly, even for large files.
* **Portable:** one small binary, no runtime bloat.
* **Accessible:** no cursor traps or redraw flickers.

---

## Philosophy

`tedit` follows the Unix philosophy of doing one thing well: editing text. It provides a clean, command-oriented experience while maintaining modern conveniences like highlighting, undo, and themes.

---

## License

`tedit` is licensed under the **BSD 3-Clause License**. You are free to use, modify, and redistribute it, provided that you retain the copyright notice.
