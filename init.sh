#!/usr/bin/env sh
# tedit PATH initializer (POSIX sh)
# - Creates wrapper commands in ~/.local/bin (or /usr/local/bin with --system)
# - Wrappers cd to the repo first, then exec install.sh / update.sh / uninstall.sh
# Usage:
#   sh init.sh
#   sh init.sh --system        # install wrappers to /usr/local/bin (needs sudo/doas)

set -eu

APP_NAME="tedit"

# --- detect repo root (this script must live in the repo root) ---
REPO_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)
[ -d "$REPO_DIR/.git" ] || {
  printf "ERROR: This doesn't look like a tedit git repo (no .git at %s)\n" "$REPO_DIR" >&2
  exit 1
}
[ -f "$REPO_DIR/tedit.cpp" ] || {
  printf "ERROR: tedit.cpp not found in %s (are you in the repo root?)\n" "$REPO_DIR" >&2
  exit 1
}

# --- colors if TTY ---
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
  GREEN="$(tput setaf 2)"; YELLOW="$(tput setaf 3)"
  RED="$(tput setaf 1)"; BOLD="$(tput bold)"; RESET="$(tput sgr0)"
else
  GREEN=""; YELLOW=""; RED=""; BOLD=""; RESET=""
fi

say() { printf "%s\n" "$*"; }
die() { printf "%sERROR:%s %s\n" "$RED" "$RESET" "$*" >&2; exit 1; }
have(){ command -v "$1" >/dev/null 2>&1; }

# --- dest bin selection ---
DEST_BIN="$HOME/.local/bin"
SYSTEM=0
[ "${1-}" = "--system" ] && SYSTEM=1

SUDO=""
if [ "$SYSTEM" -eq 1 ]; then
  DEST_BIN="/usr/local/bin"
  if [ "$(id -u)" -ne 0 ]; then
    if   have doas; then SUDO="doas"
    elif have sudo; then SUDO="sudo"
    else die "Need sudo or doas to write to $DEST_BIN (or run: sh init.sh without --system)."
    fi
  fi
fi

# --- ensure bin dir exists ---
if [ "$SYSTEM" -eq 1 ]; then
  ${SUDO:+$SUDO }mkdir -p "$DEST_BIN"
else
  mkdir -p "$DEST_BIN"
fi

# --- create wrapper helper ---
make_wrapper() {
  name="$1"    # e.g. tedit-update
  target="$2"  # e.g. update.sh
  tmp="${DEST_BIN}/.${name}.$$"
  {
    printf '%s\n' '#!/usr/bin/env sh'
    printf 'set -eu\n'
    # shellsafe absolute path to repo
    printf 'REPO_DIR=%s\n' "$(printf %s "$REPO_DIR" | sed "s/'/'\"'\"'/g; s/^/'/; s/$/'/")"
    printf 'cd -- "$REPO_DIR"\n'
    printf 'exec sh %s "$@"\n' "$(printf %s "./$target" | sed "s/'/'\"'\"'/g; s/^/'/; s/$/'/")"
  } > "$tmp"
  if [ "$SYSTEM" -eq 1 ]; then
    ${SUDO:+$SUDO }mv -f "$tmp" "${DEST_BIN}/${name}"
    ${SUDO:+$SUDO }chmod 0755 "${DEST_BIN}/${name}"
  else
    mv -f "$tmp" "${DEST_BIN}/${name}"
    chmod 0755 "${DEST_BIN}/${name}"
  fi
  say "Installed wrapper: ${DEST_BIN}/${name} -> ${target}"
}

# --- create wrappers for the three scripts ---
[ -f "$REPO_DIR/install.sh" ]   || die "install.sh not found in repo."
[ -f "$REPO_DIR/update.sh" ]    || die "update.sh not found in repo."
[ -f "$REPO_DIR/uninstall.sh" ] || die "uninstall.sh not found in repo."

make_wrapper "tedit-install"   "install.sh"
make_wrapper "tedit-update"    "update.sh"
make_wrapper "tedit-uninstall" "uninstall.sh"

# --- ensure DEST_BIN is in PATH (for user installs) ---
if [ "$SYSTEM" -eq 0 ]; then
  case ":$PATH:" in
    *:"$DEST_BIN":*) in_path=1 ;;
    *) in_path=0 ;;
  esac
  if [ $in_path -eq 0 ]; then
    # choose a profile to modify
    profile=""
    [ -n "${ZDOTDIR-}" ] && [ -w "${ZDOTDIR}/.zprofile" 2>/dev/null ] && profile="${ZDOTDIR}/.zprofile"
    [ -z "$profile" ] && [ -w "$HOME/.zprofile" 2>/dev/null ] && profile="$HOME/.zprofile"
    [ -z "$profile" ] && [ -w "$HOME/.bash_profile" 2>/dev/null ] && profile="$HOME/.bash_profile"
    [ -z "$profile" ] && profile="$HOME/.profile"
    mkdir -p "$(dirname "$profile")" 2>/dev/null || true
    if ! grep -F "$DEST_BIN" "$profile" >/dev/null 2>&1; then
      printf '\n# Added by %s init\nexport PATH="%s:$PATH"\n' "$APP_NAME" "$DEST_BIN" >> "$profile"
      say "${YELLOW}Added ${DEST_BIN} to PATH in ${profile}.${RESET}"
      say "Run: ${BOLD}export PATH=\"${DEST_BIN}:\$PATH\"${RESET} (or restart your shell) to use the commands now."
    fi
  fi
fi

printf "%s%s✅ PATH setup complete.%s\n" "$GREEN" "$BOLD" "$RESET"
say "Use: ${BOLD}tedit-install${RESET}, ${BOLD}tedit-update${RESET}, ${BOLD}tedit-uninstall${RESET} from anywhere."
