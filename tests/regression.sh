#!/usr/bin/env sh
set -eu

BIN=${TEDIT_BIN:-./tedit}
FAKE_LSP="$(pwd)/tests/fake_lsp.py"
case "$BIN" in
  /*) ;;
  *) BIN="$(pwd)/$BIN" ;;
esac

TEST_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/tedit-regression.XXXXXX")
case "$TEST_ROOT" in
  "${TMPDIR:-/tmp}"/tedit-regression.*) ;;
  *) echo "unsafe temporary directory: $TEST_ROOT" >&2; exit 1 ;;
esac
trap 'rm -rf -- "$TEST_ROOT"' EXIT HUP INT TERM

TEST_HOME="$TEST_ROOT/home"
WORK="$TEST_ROOT/work"
mkdir -p "$TEST_HOME" "$WORK"

run_tedit() {
  (unset HOME; export USERPROFILE="$TEST_HOME"; "$BIN" "$@")
}

fail() { echo "FAIL: $*" >&2; exit 1; }

# Saving must preserve embedded NUL bytes.
printf 'left\000right\n' >"$WORK/nul.bin"
cp "$WORK/nul.bin" "$WORK/nul.expected"
printf 'w!\nq!\n' | run_tedit "$WORK/nul.bin" >"$TEST_ROOT/nul.out"
cmp "$WORK/nul.bin" "$WORK/nul.expected" || fail "binary-safe save"
printf 'no final newline' >"$WORK/no-final.txt"
cp "$WORK/no-final.txt" "$WORK/no-final.expected"
printf 'w!\nq!\n' | run_tedit "$WORK/no-final.txt" >"$TEST_ROOT/no-final.out"
cmp "$WORK/no-final.txt" "$WORK/no-final.expected" || fail "final newline preservation"

# Quoted commands and aliases must round-trip through the canonical config.
printf 'one\ntwo\n' >"$WORK/path with spaces.txt"
printf 'set status off\nrepl "two" "two words"\nsaveas "%s"\nalias vv "version"\nq!\n' \
  "$WORK/saved with spaces.txt" | run_tedit "$WORK/path with spaces.txt" >"$TEST_ROOT/quoted.out"
[ "$(sed -n '2p' "$WORK/saved with spaces.txt")" = "two words" ] || fail "quoted replacement/path"
printf 'vv\nq!\n' | run_tedit >"$TEST_ROOT/alias.out"
version=$("$BIN" --version)
grep -Fq "$version" "$TEST_ROOT/alias.out" || fail "alias persistence"
[ "$(stat -c %a "$TEST_HOME/.teditrc" 2>/dev/null || stat -f %Lp "$TEST_HOME/.teditrc")" = 600 ] || fail "private config mode"

# Filenames must not inject additional configuration records.
WEIRD_PATH="$WORK/odd
theme=matrix"
printf 'safe\n' >"$WEIRD_PATH"
printf 'q!\n' | run_tedit "$WEIRD_PATH" >"$TEST_ROOT/weird1.out"
printf 'set\nq!\n' | run_tedit >"$TEST_ROOT/weird2.out"
grep -q 'theme=default' "$TEST_ROOT/weird2.out" || fail "config newline injection"

# Undo stacks must remain attached to their own buffers, and normal quit must
# refuse to discard a dirty background buffer.
printf 'alpha\n' >"$WORK/a.txt"
printf 'beta\n' >"$WORK/b.txt"
printf 'edit 1 ALPHA\nbuffer 1\nedit 1 BETA\nundo\np 1\nq\nbuffer 1\nundo\np 1\nq\n' |
  run_tedit "$WORK/a.txt" "$WORK/b.txt" >"$TEST_ROOT/buffers.out"
grep -q 'background buffer has unsaved changes' "$TEST_ROOT/buffers.out" || fail "dirty background guard"
grep -q '1 | beta' "$TEST_ROOT/buffers.out" || fail "background undo isolation"
grep -q '1 | alpha' "$TEST_ROOT/buffers.out" || fail "active undo isolation"

# Language detection and local indentation cover common brace-based files.
printf 'int main() {\nreturn 0;\n}\n' >"$WORK/indent.cpp"
printf 'indent\np 1-$\nset\nq!\n' | run_tedit "$WORK/indent.cpp" >"$TEST_ROOT/indent.out"
grep -q '2 |     return 0;' "$TEST_ROOT/indent.out" || fail "local indentation"
grep -q 'lang=cpp' "$TEST_ROOT/indent.out" || fail "language detection"

# Undoing after a successful save must correctly make the buffer dirty again.
printf 'before\n' >"$WORK/save-undo.txt"
printf 'edit 1 after\nw\nundo\ninfo\nq!\n' | run_tedit "$WORK/save-undo.txt" >"$TEST_ROOT/save-undo.out"
grep -Fq "file: $WORK/save-undo.txt *" "$TEST_ROOT/save-undo.out" || fail "dirty state after post-save undo"

# Recovery snapshots are atomic, load when newer, and disappear on explicit discard.
printf 'disk\n' >"$WORK/recover.txt"
printf 'edit 1 recovered\n' | run_tedit "$WORK/recover.txt" >"$TEST_ROOT/recover1.out"
printf 'p 1\nq!\n' | run_tedit "$WORK/recover.txt" >"$TEST_ROOT/recover2.out"
grep -q 'recovery: found snapshot' "$TEST_ROOT/recover2.out" || fail "recovery discovery"
grep -q '1 | recovered' "$TEST_ROOT/recover2.out" || fail "recovery content"
printf 'q!\n' | run_tedit "$WORK/recover.txt" >"$TEST_ROOT/recover3.out"
if grep -q 'recovery: found snapshot' "$TEST_ROOT/recover3.out"; then fail "discarded recovery persisted"; fi

# The namespaced Lua API edits through undo-aware editor operations.
printf 'base\n' >"$WORK/lua.txt"
printf 'lua tedit.set_line(1, "changed"); tedit.append_line("last"); tedit.echo(tedit.line_count())\np 1-$\nundo\np 1-$\nq!\n' |
  run_tedit "$WORK/lua.txt" >"$TEST_ROOT/lua.out"
grep -q '> 2$' "$TEST_ROOT/lua.out" || fail "Lua line_count"
grep -q '1 | changed' "$TEST_ROOT/lua.out" || fail "Lua set_line"
grep -q '2 | last' "$TEST_ROOT/lua.out" || fail "Lua append_line"

# A stdio language server can return formatting edits, which must be undoable.
if command -v python3 >/dev/null 2>&1; then
  printf 'base\n' >"$WORK/lsp.txt"
  printf 'lsp start python3 "%s"\nlsp format\np 1\nundo\np 1\nlsp stop\nq!\n' "$FAKE_LSP" |
    run_tedit "$WORK/lsp.txt" >"$TEST_ROOT/lsp.out"
  grep -q 'lsp: formatting applied' "$TEST_ROOT/lsp.out" || fail "LSP formatting"
  grep -q '1 |     base' "$TEST_ROOT/lsp.out" || fail "LSP text edit"
  grep -q '1 | base' "$TEST_ROOT/lsp.out" || fail "LSP formatting undo"
fi

# Backups must atomically replace a hostile symlink, never truncate its target.
printf 'victim\n' >"$WORK/victim"
printf 'old\n' >"$WORK/save.txt"
ln -s "$WORK/victim" "$WORK/save.txt~"
printf 'edit 1 new\nw\nq!\n' | run_tedit "$WORK/save.txt" >"$TEST_ROOT/backup.out"
[ "$(sed -n '1p' "$WORK/victim")" = victim ] || fail "backup symlink target modified"
[ ! -L "$WORK/save.txt~" ] || fail "backup remained a symlink"
[ "$(sed -n '1p' "$WORK/save.txt~")" = old ] || fail "backup content"

# Themes are data-only: traversal is rejected and OS APIs are unavailable.
mkdir -p "$TEST_HOME/tedit-config/themes"
printf 'os.execute("touch %s")\nreturn {}\n' "$TEST_ROOT/pwned" >"$TEST_HOME/tedit-config/themes/evil.lua"
printf 'theme ../../evil\ntheme evil\nq!\n' | run_tedit >"$TEST_ROOT/theme.out" 2>&1
grep -q 'invalid theme name' "$TEST_ROOT/theme.out" || fail "theme traversal guard"
grep -q 'runtime error' "$TEST_ROOT/theme.out" || fail "theme sandbox"
[ ! -e "$TEST_ROOT/pwned" ] || fail "theme escaped sandbox"

# Non-interactive output must not contain ANSI escapes.
if LC_ALL=C grep "$(printf '\033')" "$TEST_ROOT/lua.out" >/dev/null; then
  fail "ANSI escapes leaked to redirected output"
fi

# Ed mode supports classic addresses, quiet print output, deletion, and s///.
printf 'alpha\nbeta\n' >"$WORK/ed.txt"
printf 'set edmode on\n1p\n2d\n1s/alpha/ALPHA/\n%%n\nw\nq\n' |
  run_tedit "$WORK/ed.txt" >"$TEST_ROOT/ed.out"
grep -q '^alpha$' "$TEST_ROOT/ed.out" || fail "ed print command"
grep -q '1 | ALPHA' "$TEST_ROOT/ed.out" || fail "ed numbered print/substitution"
[ "$(sed -n '1p' "$WORK/ed.txt")" = ALPHA ] || fail "ed write"
printf 'set edmode off\nq!\n' | run_tedit "$WORK/ed.txt" >"$TEST_ROOT/ed-off.out"

echo "regression tests passed"
