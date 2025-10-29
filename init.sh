#!/usr/bin/env sh
# tedit PATH initializer (POSIX sh)
# - Creates wrapper commands in ~/.local/bin (user) or /usr/local/bin (system)
# - Wrappers cd to the repo and exec install.sh / update.sh / uninstall.sh
# Usage:
#   sh init.sh                  # user-level wrappers in ~/.local/bin
#   sudo sh init.sh --system    # system-wide wrappers in /usr/local/bin

set -eu

APP_NAME="tedit"

# --- detect repo root (this script must live in the repo root) ---
REPO_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)
[ -d "$REPO_DIR/.git" ] || {
  printf "ERROR: This doesn't look like a %s git repo (no .git at %s)\n" "$APP_NAME" "$REPO_DIR" >&2
  exit 1
}
[ -f "$REPO_DIR/tedit.cpp" ] || {
  printf "ERROR: tedit.cpp not found in %s (are you in the repo root?)\n" "$REPO_DIR" >&2
  exit 1
}

# --- colors if TTY ---
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
  GREEN="$(tput setaf 2)"; YELLOW="$(tput setaf 3)"
  CYAN="$(tput setaf 6)"; BOLD="$(tput bold)"; RESET="$(tput sgr0)"
else
  GREEN=""; YELLOW=""; CYAN=""; BOLD=""; RESET=""
fi

say() { printf "%s\n" "$*"; }
warn(){ printf "%sWARNING:%s %s\n" "$YELLOW" "$RESET" "$*" >&2; }
die() { printf "%sERROR:%s %s\n" "$YELLOW" "$RESET" "$*" >&2; exit 1; }
have(){ command -v "$1" >/dev/null 2>&1; }

# --- args ---
SYSTEM=0
[ "${1-}" = "--system" ] && SYSTEM=1

# If running as root without --system, default to system wrappers
if [ "$(id -u)" -eq 0 ] && [ "$SYSTEM" -eq 0 ]; then
  SYSTEM=1
fi

# --- destination bin + privilege helper ---
DEST_BIN="$HOME/.local/bin"
SUDO=""
if [ "$SYSTEM" -eq 1 ]; then
  DEST_BIN="/usr/local/bin"
  if [ "$(id -u)" -ne 0 ]; then
    if   have sudo; then SUDO="sudo"
    elif have doas; then SUDO="doas"
    else die "Need sudo or doas to write to $DEST_BIN (or run without --system for user install)."
    fi
  fi
fi

# --- ensure bin dir exists ---
if [ "$SYSTEM" -eq 1 ]; then
  ${SUDO:+$SUDO }mkdir -p "$DEST_BIN"
else
  mkdir -p "$DEST_BIN"
fi

# --- safe printer for a single-quoted shell literal ---
sq() { printf %s "$1" | sed "s/'/'\"'\"'/g; s/^/'/; s/\$/'/"; }

# --- create wrapper helper ---
make_wrapper() {
  name="$1"    # e.g. tedit-update
  target="$2"  # e.g. update.sh
  tmp="${DEST_BIN}/.${name}.$$"

  # Absolute repo path (shell-safe single-quoted)
  REPO_ESC="$(sq "$REPO_DIR")"
  TARGET_ESC="$(sq "./$target")"

  {
    printf '%s\n' '#!/usr/bin/env sh'
    printf 'set -eu\n'
    printf 'REPO_DIR=%s\n' "$REPO_ESC"
    printf 'TARGET=%s\n' "$TARGET_ESC"
    printf 'if [ ! -d "$REPO_DIR/.git" ] || [ ! -f "$REPO_DIR/tedit.cpp" ]; then\n'
    printf '  echo "ERROR: tedit repo not found at $REPO_DIR."; echo "Hint: re-clone and re-run: sh init.sh"; exit 1\n'
    printf 'fi\n'
    printf 'cd -- "$REPO_DIR"\n'
    printf 'exec sh "$TARGET" "$@"\n'
  } > "$tmp"

  # move into place with correct perms
  if [ "$SYSTEM" -eq 1 ]; then
    ${SUDO:+$SUDO }mv -f "$tmp" "${DEST_BIN}/${name}"
    ${SUDO:+$SUDO }chmod 0755 "${DEST_BIN}/${name}"
  else
    mv -f "$tmp" "${DEST_BIN}/${name}"
    chmod 0755 "${DEST_BIN}/${name}"
  fi

  say "Installed wrapper: ${DEST_BIN}/${name} -> ${target}"
}

# --- require scripts in repo ---
[ -f "$REPO_DIR/install.sh" ]   || die "install.sh not found in repo."
[ -f "$REPO_DIR/update.sh" ]    || die "update.sh not found in repo."
[ -f "$REPO_DIR/uninstall.sh" ] || die "uninstall.sh not found in repo."

# --- create wrappers ---
make_wrapper "tedit-install"   "install.sh"
make_wrapper "tedit-update"    "update.sh"
make_wrapper "tedit-uninstall" "uninstall.sh"

# --- PATH integration (user installs only) ---
if [ "$SYSTEM" -eq 0 ]; then
  case ":$PATH:" in
    *:"$DEST_BIN":*) in_path=1 ;;
    *) in_path=0 ;;
  esac
  if [ $in_path -eq 0 ]; then
    profile=""
    [ -n "${ZDOTDIR-}" ] && [ -w "${ZDOTDIR}/.zprofile" 2>/dev/null ] && profile="${ZDOTDIR}/.zprofile"
    [ -z "$profile" ] && [ -w "$HOME/.zprofile" 2>/dev/null ] && profile="$HOME/.zprofile"
    [ -z "$profile" ] && [ -w "$HOME/.bash_profile" 2>/dev/null ] && profile="$HOME/.bash_profile"
    [ -z "$profile" ] && profile="$HOME/.profile"

    mkdir -p "$(dirname "$profile")" 2>/dev/null || true
    if ! grep -F "$DEST_BIN" "$profile" >/dev/null 2>&1; then
      printf '\n# Added by %s init\nexport PATH="%s:$PATH"\n' "$APP_NAME" "$DEST_BIN" >> "$profile"
      say "${YELLOW}Added ${DEST_BIN} to PATH in ${profile}.${RESET}"
    fi

    # immediate use without restart
    say "Run this now to use wrappers immediately:"
    say "  ${BOLD}export PATH=\"${DEST_BIN}:\$PATH\"${RESET}"
    say "Or restart your terminal."
  fi
fi

printf "%s%s✅ %s wrappers ready.%s\n" "$GREEN" "$BOLD" "$APP_NAME" "$RESET"
say "Use from anywhere:"
say "  ${BOLD}sudo tedit-install${RESET}    # build + install to /usr/local/bin"
say "  ${BOLD}sudo tedit-update${RESET}     # fetch, rebuild, reinstall"
say "  ${BOLD}sudo tedit-uninstall${RESET}  # remove binary/man/wrappers"
