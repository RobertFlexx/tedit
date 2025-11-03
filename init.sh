#!/usr/bin/env sh
# - Creates wrapper commands for install/update/uninstall (does anybody read these? D;)
# - Wrappers cd into the repo and exec the real scripts
# - Records repo location in a marker file for tools like tedit-update
#
# Usage:
#   sh init.sh                  # user-level wrappers in ~/.local/bin
#   sudo sh init.sh --system    # system-wide wrappers in /usr/local/bin

set -eu

APP_NAME="tedit"

# ---------- detect repo root ----------
REPO_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)

if [ ! -f "$REPO_DIR/tedit.cpp" ]; then
  printf "ERROR: tedit.cpp not found in %s (are you in the repo root?)\n" "$REPO_DIR" >&2
  exit 1
fi

# .git is *nice* but not strictly required; warn if missing
if [ ! -d "$REPO_DIR/.git" ]; then
  printf "WARNING: %s does not contain a .git directory.\n" "$REPO_DIR" >&2
  printf "         Wrappers will still work, but git-based update features may be limited.\n" >&2
fi

# ---------- colors ----------
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
  GREEN="$(tput setaf 2)"
  YELLOW="$(tput setaf 3)"
  RED="$(tput setaf 1)"
  CYAN="$(tput setaf 6)"
  BOLD="$(tput bold)"
  RESET="$(tput sgr0)"
else
  GREEN=""; YELLOW=""; RED=""; CYAN=""; BOLD=""; RESET=""
fi

say()  { printf "%s\n" "$*"; }
warn() { printf "%sWARNING:%s %s\n" "$YELLOW" "$RESET" "$*" >&2; }
die()  { printf "%sERROR:%s %s\n"  "$RED"    "$RESET" "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

# ---------- args ----------
SYSTEM=0
if [ "${1-}" = "--system" ]; then
  SYSTEM=1
fi

# If running as root without --system, default to system wrappers
if [ "$(id -u)" -eq 0 ] && [ "$SYSTEM" -eq 0 ]; then
  SYSTEM=1
fi

# ---------- destination bin + privilege helper ----------
DEST_BIN="$HOME/.local/bin"
MARKER_USER="$HOME/.local/share/tedit/repo"
MARKER_SYS="/usr/local/share/tedit/repo"

SUDO=""
if [ "$SYSTEM" -eq 1 ]; then
  DEST_BIN="/usr/local/bin"
  if [ "$(id -u)" -ne 0 ]; then
    if   have sudo; then SUDO="sudo"
    elif have doas; then SUDO="doas"
    else die "Need sudo or doas to write to $DEST_BIN (or run without --system for user-level wrappers)."
    fi
  fi
fi

run_root() {
  if [ "$SYSTEM" -eq 1 ] && [ -n "$SUDO" ]; then
    # shellcheck disable=SC2086
    $SUDO "$@"
  else
    "$@"
  fi
}

# ---------- ensure bin dir exists ----------
if [ "$SYSTEM" -eq 1 ]; then
  run_root mkdir -p "$DEST_BIN"
else
  mkdir -p "$DEST_BIN"
fi

# ---------- record repo marker (for tedit-update, etc.) ----------
if [ "$SYSTEM" -eq 1 ]; then
  run_root mkdir -p "$(dirname "$MARKER_SYS")"
  printf '%s\n' "$REPO_DIR" | run_root tee "$MARKER_SYS" >/dev/null
else
  mkdir -p "$(dirname "$MARKER_USER")"
  printf '%s\n' "$REPO_DIR" > "$MARKER_USER"
fi

# ---------- safe printer for single-quoted shell literal ----------
sq() {
  # Wrap $1 as a single-quoted shell literal, handling embedded single quotes
  printf %s "$1" | sed "s/'/'\"'\"'/g; s/^/'/; s/\$/'/"
}

# ---------- create wrapper helper ----------
make_wrapper() {
  name="$1"    # e.g. tedit-update
  target="$2"  # e.g. update.sh
  tmp="${DEST_BIN}/.${name}.$$"

  REPO_ESC="$(sq "$REPO_DIR")"
  TARGET_ESC="$(sq "./$target")"

  {
    printf '%s\n' '#!/usr/bin/env sh'
    printf 'set -eu\n'
    printf 'APP_NAME=%s\n' "$(sq "$APP_NAME")"
    printf 'DEFAULT_REPO_DIR=%s\n' "$REPO_ESC"
    printf 'MARKER_SYS=/usr/local/share/tedit/repo\n'
    printf 'MARKER_USER=$HOME/.local/share/tedit/repo\n'
    printf 'TARGET=%s\n' "$TARGET_ESC"
    printf 'REPO_DIR="${TEDIT_REPO:-}"\n'
    printf 'if [ -z "$REPO_DIR" ] && [ -f "$MARKER_SYS" ]; then REPO_DIR=$(cat "$MARKER_SYS" 2>/dev/null || true); fi\n'
    printf 'if [ -z "$REPO_DIR" ] && [ -f "$MARKER_USER" ]; then REPO_DIR=$(cat "$MARKER_USER" 2>/dev/null || true); fi\n'
    printf 'if [ -z "$REPO_DIR" ]; then REPO_DIR="$DEFAULT_REPO_DIR"; fi\n'
    printf 'if [ ! -f "$REPO_DIR/tedit.cpp" ]; then\n'
    printf '  echo "ERROR: tedit repo not found at $REPO_DIR." >&2\n'
    printf '  echo "Hint: set TEDIT_REPO=/absolute/path/to/tedit or re-run init.sh in the repo." >&2\n'
    printf '  exit 1\n'
    printf 'fi\n'
    printf 'cd -- "$REPO_DIR"\n'
    printf 'exec sh "$TARGET" "$@"\n'
  } > "$tmp"
 # path (hi)
  if [ "$SYSTEM" -eq 1 ]; then
    run_root mv -f "$tmp" "${DEST_BIN}/${name}"
    run_root chmod 0755 "${DEST_BIN}/${name}"
  else
    mv -f "$tmp" "${DEST_BIN}/${name}"
    chmod 0755 "${DEST_BIN}/${name}"
  fi

  if [ -x "${DEST_BIN}/${name}" ]; then
    say "Wrapper ${BOLD}${name}${RESET} installed at ${CYAN}${DEST_BIN}/${name}${RESET}"
  else
    warn "Wrapper ${name} was created but is not executable."
  fi
}

# ---------- require scripts in repo ----------
[ -f "$REPO_DIR/install.sh" ]   || die "install.sh not found in repo."
[ -f "$REPO_DIR/update.sh" ]    || die "update.sh not found in repo."
[ -f "$REPO_DIR/uninstall.sh" ] || die "uninstall.sh not found in repo."

printf "%s%s>> Initializing %s wrappers <<%s\n" "$CYAN" "$BOLD" "$APP_NAME" "$RESET"

# ---------- create wrappers ----------
make_wrapper "tedit-install"   "install.sh"
make_wrapper "tedit-update"    "update.sh"
make_wrapper "tedit-uninstall" "uninstall.sh"

# ---------- PATH integration (user installs only) ----------
if [ "$SYSTEM" -eq 0 ]; then
  case ":$PATH:" in
    *:"$DEST_BIN":*) in_path=1 ;;
    *)               in_path=0 ;;
  esac

  if [ "$in_path" -eq 0 ]; then
    profile=""
    # Prefer zsh, then bash, then fallback to .profile
    if [ -n "${ZDOTDIR-}" ] && [ -w "${ZDOTDIR}/.zprofile" 2>/dev/null ]; then
      profile="${ZDOTDIR}/.zprofile"
    elif [ -w "$HOME/.zprofile" 2>/dev/null ]; then
      profile="$HOME/.zprofile"
    elif [ -w "$HOME/.bash_profile" 2>/dev/null ]; then
      profile="$HOME/.bash_profile"
    else
      profile="$HOME/.profile"
    fi

    mkdir -p "$(dirname "$profile")" 2>/dev/null || true
    if ! grep -F "$DEST_BIN" "$profile" >/dev/null 2>&1; then
      printf '\n# Added by %s init\nexport PATH="%s:$PATH"\n' \
        "$APP_NAME" "$DEST_BIN" >> "$profile"
      say "${YELLOW}Added ${DEST_BIN} to PATH in ${profile}.${RESET}"
    fi

    say "To use wrappers immediately in this shell, run:"
    say "  ${BOLD}export PATH=\"${DEST_BIN}:\$PATH\"${RESET}"
    say "Or restart your terminal."
  fi
fi

printf "%s%s✅ %s wrappers ready.%s\n" "$GREEN" "$BOLD" "$APP_NAME" "$RESET"
say "From any directory, you can now run:"
say "  ${BOLD}sudo tedit-install${RESET}    # build + install to /usr/local/bin"
say "  ${BOLD}sudo tedit-update${RESET}     # fetch, rebuild, reinstall"
say "  ${BOLD}sudo tedit-uninstall${RESET}  # remove binary/man/wrappers"
