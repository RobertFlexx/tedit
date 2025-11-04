# tedit Lua Plugins & Themes

*(Beginner-friendly guide with examples & safety notes)*

`tedit` embeds a small **Lua 5.4** runtime so you can customize behavior, automate editing tasks, and define your own color themes — all without recompiling the editor.

This document walks through:

* Where plugins/themes live
* How `tedit` loads and runs them
* **Beginner-friendly, copy-paste examples**
* A **warning/disclaimer** about malicious plugins
* What the “suspicious” Lua APIs actually do (`os.execute`, `io.popen`, etc.)

> You don’t need to be a Lua expert. If you know basic scripting or can read simple code, you can follow along.

---

## 1. How tedit Finds Plugins & Themes

### Plugin directory

`tedit` looks for Lua plugins here:

```text
~/tedit-config/plugins
```

* Any file ending in `.lua` in that directory is treated as a **plugin**.
* On startup, `tedit` **scans** this directory and remembers the plugin names.
* Plugins are not automatically executed just by existing; you run them with `:run-plugin` (details below).

### Theme directory (Lua themes)

Lua themes live here:

```text
~/tedit-config/themes
```

* Each theme is a file: `NAME.lua` (e.g. `pink.lua`).

* You can apply a Lua theme with:

  ```text
  theme NAME
  ```

  where `NAME` matches the filename without `.lua` (e.g. `theme pink` for `pink.lua`).

* If you’ve added the `lua-themes` command in your build, you can list all Lua themes:

  ```text
  lua-themes
  ```

---

## 2. Running Plugins in tedit

Once you’ve compiled `tedit` with Lua support:

* **List plugins**:

  ```text
  plugins
  ```

* **Reload plugin list** (after adding/removing files):

  ```text
  reload-plugins
  ```

* **Run a plugin by name** (from `~/tedit-config/plugins`):

  ```text
  :run-plugin myplugin
  ```

  If there is a `myplugin.lua` in the plugins directory, `tedit` will load and execute it.

* **Run a plugin from an explicit path**:

  ```text
  :run-plugin /absolute/path/to/some.lua
  ```

* **Run a snippet of Lua directly**:

  ```text
  lua tedit_echo("hello from inline lua")
  ```

* **Run a Lua script file directly**:

  ```text
  luafile /path/to/script.lua
  ```

---

## 3. What Lua Can Do Inside tedit

`tedit` exposes some helper functions to Lua:

* `tedit_echo(text)`

  * Prints a message in the **accent** color in the status/output area.
* `tedit_command(cmd)`

  * Runs a normal **tedit command** as if you typed it at the prompt.
  * Example: `tedit_command("set number on")`, `tedit_command("find main")`.
* `tedit_print(line_number)`

  * Prints a specific line (1-based) from the current buffer.

These are **safe** high-level helpers – they don’t bypass tedit’s safety mechanisms; they just drive the editor.

---

## 4. Your First Plugin (Beginner Example)

Create the plugin directory if it doesn’t exist:

```bash
mkdir -p ~/tedit-config/plugins
```

Then create a file:

```bash
$ tedit ~/tedit-config/plugins/hello.lua
```

Put this inside:

```lua
-- hello.lua - a tiny tedit plugin example

-- This function is called when the plugin is run via :run-plugin hello
local function main()
  if tedit_echo then
    tedit_echo("Hello from the 'hello' plugin!")
  end

  -- Turn on line numbers (just as a demonstration)
  if tedit_command then
    tedit_command("set number on")
  end
end

-- Actually run our main() function
main()
```

Save, then in `tedit`:

```text
reload-plugins    # optional if tedit was already running
:run-plugin hello
```

You should see:

* A colored “Hello from the 'hello' plugin!” line.
* Line numbers turned on.

---

## 5. A Plugin That Sets Up Some Defaults

Here’s a more practical plugin that sets your favorite defaults whenever you run it:

```lua
-- comfy_defaults.lua
-- Example: set some personal defaults via a plugin

local function main()
  if not tedit_command or not tedit_echo then
    return
  end

  -- Set theme, highlighting, and some toggles
  tedit_command("theme dark")          -- or neon/matrix/etc.
  tedit_command("highlight on")
  tedit_command("set wrap on")
  tedit_command("set truncate off")
  tedit_command("set number on")
  tedit_command("set autosave 120")

  tedit_echo("Comfy defaults applied ✔")
end

main()
```

Use it:

```text
:run-plugin comfy_defaults
```

You can run this plugin whenever you start editing to quickly apply your favorite setup. Or, you can add an alias in `~/.teditrc` like:

```ini
alias comfy run-plugin comfy_defaults
```

Then in tedit:

```text
:comfy
```

---

## 6. A Plugin That Chains Multiple tedit Commands

Because plugins can call `tedit_command`, you can script little workflows.

Example: run a search and jump to the first match:

```lua
-- jump_to_main.lua
-- Find 'main' and jump to the first match.

local function main()
  if not tedit_command or not tedit_echo then
    return
  end

  -- Run a search
  tedit_command("find main")

  -- 'find' prints matches and remembers the search string
  -- Now 'n' jumps to the next match, so simulate that:
  tedit_command("n")

  tedit_echo("Jumped to first 'main' occurrence.")
end

main()
```

Run it:

```text
:run-plugin jump_to_main
```

---

## 7. Writing a Simple Lua Theme

Lua themes live in:

```text
~/tedit-config/themes
```

A theme file must return (or set) a **Lua table** with some string fields that control colors. Those fields map to the internal `ThemePalette`:

* `accent`
* `ok`
* `warn`
* `err`
* `dim`
* `prompt`
* `input`
* `gutter`
* `title`
* `help_cmd`
* `help_arg`
* `help_text`

Each value should be an ANSI color escape (e.g. `"\27[35m"` for magenta) or an empty string if you don’t want color.

> If you don’t know ANSI codes: you can start by tweaking only a couple fields, and letting others inherit from the default. If a field is missing, tedit falls back to its built-in default colors.

### Minimal theme example

```bash
mkdir -p ~/tedit-config/themes
tedit ~/tedit-config/themes/simple.lua
```

```lua
-- simple.lua - a very minimal tedit theme
-- You can start by only overriding a few fields.

local theme = {
  accent    = "\27[95m",  -- bright magenta
  ok        = "\27[92m",  -- bright green
  warn      = "\27[93m",  -- bright yellow
  err       = "\27[91m",  -- bright red
  dim       = "\27[90m",  -- bright black / gray

  -- leave others empty to use defaults
  prompt    = "",         -- default prompt color
  input     = "",
  gutter    = "",
  title     = "",
  help_cmd  = "",
  help_arg  = "",
  help_text = "",
}

return theme
```

Apply it in `tedit`:

```text
theme simple
```

If you’ve wired up the `lua-themes` command, you can double-check the name:

```text
lua-themes
# should list: simple
```

---

## 8. A “Pink” Theme Example (with comments)

Here’s a more opinionated theme that tries to be “pink-ish” while still readable:

```lua
-- pink.lua - example Lua theme for tedit
-- NOTE: This only affects tedit's *UI* colors, not syntax highlighting colors per language.

local pink    = "\27[95m"  -- bright magenta / pink
local hotpink = "\27[35m"  -- magenta
local green   = "\27[92m"
local yellow  = "\27[93m"
local red     = "\27[91m"
local dim     = "\27[90m"
local white   = "\27[97m"

local theme = {
  -- General accent colors
  accent    = pink,     -- used for general accent / decorative text
  ok        = green,    -- success messages
  warn      = yellow,   -- warnings
  err       = red,      -- errors
  dim       = dim,      -- low-importance text

  -- Prompt + input line
  prompt    = hotpink,  -- "tedit> " prompt
  input     = white,    -- the text you type after the prompt

  -- Left gutter (line numbers)
  gutter    = dim,      -- line numbers

  -- Section titles (like "Commands (...)")
  title     = pink,

  -- :help colors
  help_cmd  = hotpink,  -- command names
  help_arg  = white,    -- arguments
  help_text = dim,      -- explanatory text
}

-- tedit will read this table and overlay it on the default palette
return theme
```

Usage:

```text
theme pink
```

If `theme pink` doesn’t work, check:

* The file name is exactly `pink.lua`.
* It’s in `~/tedit-config/themes`.
* Your build includes the Lua theme loader.

---

## 9. Inline Lua: Quick One-Off Experiments

You don’t always need a full plugin file; sometimes a quick `:lua` is enough.

Examples:

```text
lua tedit_echo("Hello from inline lua")
lua tedit_command("theme matrix")
lua tedit_print(1)
```

These are handy for testing ideas before you commit them into a plugin file.

---

## 10. WARNING / DISCLAIMER — Plugin Safety & Malicious Code

> **⚠️ WARNING: Always treat third-party plugins as untrusted code.**

`tedit` itself is a single, auditable binary, but **Lua plugins are arbitrary code** that run with **your user’s permissions**. A malicious plugin can:

* Delete or modify your files
* Exfiltrate secrets (SSH keys, passwords in config files, API tokens)
* Run any shell command you can run
* Hide what it’s doing

`tedit` includes a **basic scanner** for common dangerous patterns (like `os.execute`, `io.popen`, etc.) and will **warn you** when you run such a plugin, but:

> A warning is **not** a full security audit. You are responsible for what you run.

### 10.1 APIs that tedit flags as “suspicious”

When a plugin contains these strings, tedit shows a warning before running it:

* `os.execute`
* `io.popen`
* `dofile`
* `loadfile`
* `package.loadlib`
* `debug.` (any `debug.*` usage)
* `os.remove`
* `os.rename`

These APIs are **not inherently evil**, but they are powerful enough to be abused. Here’s what each does and why it’s risky.

#### `os.execute("...")`

* **What it does:** Runs a **shell command**.

* **Why it’s risky:** Can do literally anything you can do in a shell:

  ```lua
  os.execute("rm -rf ~/")         -- deletes your home directory
  os.execute("curl ... | sh")     -- downloads and runs remote code
  ```

* Treat any plugin using `os.execute` as **high-risk** unless you’ve reviewed it carefully.

#### `io.popen("...")`

* **What it does:** Starts a process and returns a file handle to its **input or output**.

* **Why it’s risky:** Can both **run commands** and **read their output**, e.g.:

  ```lua
  local f = io.popen("cat ~/.ssh/id_rsa")
  local key = f:read("*a")
  f:close()
  -- now the plugin can send 'key' somewhere using other means
  ```

* This is a common way to quietly read sensitive files.

#### `dofile("...")` / `loadfile("...")`

* **What they do:** Load and execute another Lua file (`dofile`) or compile it to a function (`loadfile`).
* **Why it’s risky:** Malicious code can be split across files, making it harder to notice:

  ```lua
  dofile("hidden_payload.lua")
  ```

  If you only check the main plugin file, you might miss dangerous behavior in the loaded file.

#### `package.loadlib("...")`

* **What it does:** Loads a **native (C) library** and returns a function from it.
* **Why it’s risky:** This effectively bridges from Lua to **arbitrary native code**, which can do anything with no sandbox.

#### `debug.*` (e.g. `debug.getinfo`, `debug.setmetatable`, etc.)

* **What it does:** Gives deep introspection/manipulation of the Lua runtime.
* **Why it’s risky:**

  * Can be used to **hide behavior** or modify built-in functions.
  * Rarely needed in simple, honest plugins.
  * Can make code harder to reason about.

#### `os.remove("file")` / `os.rename("old", "new")`

* **What they do:** Delete a file or rename/move it.
* **Why it’s risky:** A plugin can:

  * Quietly delete important files (`.ssh`, configs, dotfiles).
  * Move files to locations you don’t expect.

### 10.2 Practical Safety Checklist

When you get a plugin from someone else:

1. **Open the `.lua` file in tedit** and skim it. Look for:

   * `os.execute`, `io.popen`, `os.remove`, `os.rename`
   * Any `debug.*`, `package.loadlib`, `dofile`, `loadfile`
   * Long, obfuscated strings, or weird encoding tricks

2. **Is all the code “editor-ish”?**
   Safe plugins usually:

   * Call `tedit_command("...")`, `tedit_echo("...")`, `tedit_print(...)`
   * Manipulate strings and numbers
   * Maybe read/write a small config file in `~/tedit-config`

3. **Minimal permissions mindset:**

   * Never run plugins from someone you don’t trust.
   * Avoid plugins that handle secrets, SSH keys, or dotfiles unless you fully understand them.
   * If in doubt: **don’t run it.**

4. **Remember:** if you run a plugin, you’re effectively saying:

   > “Run this program as **me** on my machine.”

---

## 11. Writing “Safer” Plugins (Guidelines)

Here are some conservative guidelines for **beginner-level, relatively safe** plugins:

* **DO:**

  * Use `tedit_command`, `tedit_echo`, and `tedit_print`.
  * Manipulate text entirely inside tedit.
  * Store small config inside `~/tedit-config/plugins` or `~/tedit-config`.

* **Avoid (especially if you’re sharing the plugin publicly):**

  * `os.execute`, `io.popen`, `os.remove`, `os.rename`
  * `debug.*`, `package.loadlib`, `dofile`, `loadfile`
  * Writing to arbitrary paths outside `~/tedit-config` unless absolutely necessary.

* **If you must use shell commands** (advanced use only):

  * Explain clearly in comments what you’re doing and why.
  * Avoid constructing shell commands from untrusted user input.
  * Prefer well-scoped commands that don’t touch sensitive files.

---

## 12. Summary

* **Plugins** live in `~/tedit-config/plugins`, are plain `.lua` files, and are run via `:run-plugin`.
* **Lua themes** live in `~/tedit-config/themes` and are applied via `theme <name>`.
* `tedit` gives Lua **safe helper functions**: `tedit_command`, `tedit_echo`, `tedit_print`.
* You can start with **tiny, readable plugins** that just call those helpers.
* Some Lua APIs (`os.execute`, `io.popen`, etc.) are powerful and dangerous — `tedit` warns about them, but **you** must decide what to trust.
* When in doubt, keep your plugins small, transparent, and focused on editing, not system-level magic.

If you want, next step I can help you write a specific plugin or theme you have in mind (e.g., “trim trailing spaces”, “auto-add header comment”, or a specific color scheme).
