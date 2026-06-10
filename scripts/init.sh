#!/usr/bin/env sh
# tedit init
# - Creates wrapper commands: tedit-install / tedit-update / tedit-uninstall
# - Records repo location in a marker file (used by wrappers + update script)
# - Portable POSIX sh with aggressive fallbacks
#
# Usage:
#   sh scripts/init.sh                  # user-level wrappers in ~/.local/bin (or XDG)
#   sh scripts/init.sh --system         # system-wide wrappers (default /usr/local/bin)
#   sh scripts/init.sh --bin-dir PATH   # override wrapper destination
#
# Notes:
# - System mode needs root OR sudo/doas.
# - Uses marker files:
#     user: ${XDG_DATA_HOME:-$HOME/.local/share}/tedit/repo
#     sys:  /usr/local/share/tedit/repo

set -eu
umask 022

APP_NAME="tedit"

# -----------------------------
# tiny portability helpers
# -----------------------------
have(){ command -v "$1" >/dev/null 2>&1; }

mktemp_safe(){
  # prints a temp path (not necessarily created)
  base="${TMPDIR:-/tmp}/${APP_NAME}-init.$(date +%s).$$"
  if have mktemp; then
    # try GNU/BSD styles
    p="$(mktemp "${TMPDIR:-/tmp}/${APP_NAME}-init.XXXXXX" 2>/dev/null || true)"
    if [ -n "${p:-}" ]; then printf "%s" "$p"; return 0; fi
    p="$(mktemp -t "${APP_NAME}-init" 2>/dev/null || true)"
    if [ -n "${p:-}" ]; then printf "%s" "$p"; return 0; fi
  fi
  printf "%s" "$base"
}

LOG="${LOG_FILE:-$(mktemp_safe).log}"
: >"$LOG" 2>/dev/null || { echo "ERROR: cannot write log: $LOG" >&2; exit 2; }

log(){ printf "%s\n" "$*" >>"$LOG"; }

TTY=0; [ -t 1 ] && TTY=1
NO_COLOR="${NO_COLOR-}"

if [ "$TTY" -eq 1 ] && [ -z "$NO_COLOR" ] && have tput; then
  BOLD="$(tput bold 2>/dev/null || true)"
  RESET="$(tput sgr0 2>/dev/null || true)"
  RED="$(tput setaf 1 2>/dev/null || true)"
  GREEN="$(tput setaf 2 2>/dev/null || true)"
  YELLOW="$(tput setaf 3 2>/dev/null || true)"
  CYAN="$(tput setaf 6 2>/dev/null || true)"
else
  BOLD=""; RESET=""; RED=""; GREEN=""; YELLOW=""; CYAN=""
fi

say(){ printf "%s\n" "$*"; log "$*"; }
info(){ say "${CYAN}${BOLD}::${RESET} $*"; }
ok(){   say "${GREEN}${BOLD}✓${RESET} $*"; }
warn(){ say "${YELLOW}${BOLD}!${RESET} $*"; }
die(){  say "${RED}${BOLD}x${RESET} $*"; say "Log: $LOG"; exit 1; }

is_utf(){ echo "${LC_ALL:-${LANG:-}}" | grep -qi 'utf-8'; }
repeat(){ n=$1; ch=$2; i=0; out=""; while [ "$i" -lt "$n" ]; do out="$out$ch"; i=$((i+1)); done; printf "%s" "$out"; }
term_width(){ w=80; if have tput; then w=$(tput cols 2>/dev/null || echo 80); fi; [ "$w" -gt 24 ] || w=80; echo "$w"; }
if is_utf; then FIL="█"; EMP="░"; else FIL="#"; EMP="-"; fi

TOTAL=8
STEP=0

draw_bar(){
  [ "$TTY" -eq 1 ] || return 0
  n=$1; tot=$2; msg=$3
  tw=$(term_width); bw=$(( tw - 32 )); [ "$bw" -lt 12 ] && bw=12; [ "$bw" -gt 60 ] && bw=60
  [ "$tot" -gt 0 ] || tot=1
  pct=$(( n*100 / tot )); fill=$(( n*bw / tot )); empty=$(( bw - fill ))
  bar="$(repeat "$fill" "$FIL")$(repeat "$empty" "$EMP")"
  printf "\r\033[K%s%s[%s]%s %d/%d (%d%%) %s" "$CYAN" "$BOLD" "$bar" "$RESET" "$n" "$tot" "$pct" "$msg"
  log "$(printf '[%s] %d/%d (%d%%) %s' "$(repeat "$fill" "#")$(repeat "$empty" "-")" "$n" "$tot" "$pct" "$msg")"
}
next(){ STEP=$((STEP+1)); [ "$STEP" -gt "$TOTAL" ] && STEP="$TOTAL"; draw_bar "$STEP" "$TOTAL" "$1"; }

finish_ui(){
  [ "$TTY" -eq 1 ] && printf "\r\033[K\n"
}
trap 'finish_ui' EXIT INT TERM

# -----------------------------
# args
# -----------------------------
SYSTEM=0
FORCE=0
NO_PATH=0
DEST_BIN=""
SHOW=0

usage(){
  cat <<EOF
Usage: sh scripts/init.sh [options]

Options:
  --system         Install wrappers system-wide (default: /usr/local/bin)
  --user           Install wrappers user-level (default)
  --bin-dir PATH   Install wrappers into PATH instead of defaults
  --no-path        Do not edit shell profile to add bin dir (user mode only)
  --force          Overwrite existing wrapper files even if not ours
  --show           Print computed paths/mode and exit
  -h, --help       Show help

Env:
  LOG_FILE         Write log here instead of a temp file
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --system) SYSTEM=1 ;;
    --user)   SYSTEM=0 ;;
    --bin-dir)
      shift || die "--bin-dir requires a value"
      DEST_BIN="$1"
      ;;
    --no-path) NO_PATH=1 ;;
    --force) FORCE=1 ;;
    --show) SHOW=1 ;;
    -h|--help) usage; exit 0 ;;
    *) die "Unknown option: $1" ;;
  esac
  shift
done

# If root and not forced user, default to system wrappers (matches your old behavior)
if [ "$(id -u)" -eq 0 ] && [ "$SYSTEM" -eq 0 ]; then
  SYSTEM=1
fi

# -----------------------------
# detect repo root (script dir)
# -----------------------------
next "Locating repo"
SCRIPT_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)
REPO_DIR=$(CDPATH= cd -P -- "$SCRIPT_DIR/.." 2>/dev/null && pwd)

[ -f "$REPO_DIR/src/tedit.cpp" ] || die "src/tedit.cpp not found in $REPO_DIR."

# required scripts (wrappers call these)
[ -f "$REPO_DIR/scripts/install.sh" ]   || die "scripts/install.sh not found in repo."
[ -f "$REPO_DIR/scripts/update.sh" ]    || die "scripts/update.sh not found in repo."
[ -f "$REPO_DIR/scripts/uninstall.sh" ] || die "scripts/uninstall.sh not found in repo."

if [ ! -d "$REPO_DIR/.git" ]; then
  warn "No .git found in repo dir. Wrappers still work; git-based update features may be limited."
fi

# -----------------------------
# choose destination + marker paths
# -----------------------------
next "Selecting target paths"
XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
USER_MARKER="${XDG_DATA_HOME}/tedit/repo"
SYS_MARKER="/usr/local/share/tedit/repo"

# default user bin: ~/.local/bin (or if user already uses ~/bin, allow fallback)
DEFAULT_USER_BIN="$HOME/.local/bin"
[ -d "$HOME/bin" ] && DEFAULT_USER_BIN="$HOME/bin"

# system bin defaults
OS="$(uname -s 2>/dev/null || echo unknown)"
DEFAULT_SYS_BIN="/usr/local/bin"
# macOS / Homebrew quirks: /opt/homebrew/bin exists on Apple Silicon
if [ "$OS" = "Darwin" ] && [ -d "/opt/homebrew/bin" ]; then
  # only choose it if /usr/local/bin doesn't exist and /opt/homebrew/bin does
  [ -d "/usr/local/bin" ] || DEFAULT_SYS_BIN="/opt/homebrew/bin"
fi

if [ -n "$DEST_BIN" ]; then
  : # keep override
else
  if [ "$SYSTEM" -eq 1 ]; then DEST_BIN="$DEFAULT_SYS_BIN"
  else DEST_BIN="$DEFAULT_USER_BIN"
  fi
fi

# -----------------------------
# privilege helper
# -----------------------------
next "Preparing privileges"
SUDO=""
if [ "$SYSTEM" -eq 1 ] && [ "$(id -u)" -ne 0 ]; then
  if have sudo; then SUDO="sudo"
  elif have doas; then SUDO="doas"
  else die "Need sudo or doas for system install into $DEST_BIN (or run --user)."
  fi
fi

run_root(){
  if [ "$SYSTEM" -eq 1 ] && [ -n "$SUDO" ]; then
    # shellcheck disable=SC2086
    $SUDO "$@"
  else
    "$@"
  fi
}

auth_once(){
  [ -z "$SUDO" ] && return 0
  [ "$TTY" -eq 1 ] && printf "\r\033[K"
  info "Elevating privileges (may prompt once)..."
  if [ "$SUDO" = "sudo" ]; then sudo -v
  else $SUDO true
  fi
}

auth_once || true

# -----------------------------
# show mode/path and exit if requested
# -----------------------------
if [ "$SHOW" -eq 1 ]; then
  finish_ui
  printf "REPO_DIR=%s\n" "$REPO_DIR"
  printf "MODE=%s\n" "$( [ "$SYSTEM" -eq 1 ] && echo system || echo user )"
  printf "DEST_BIN=%s\n" "$DEST_BIN"
  printf "USER_MARKER=%s\n" "$USER_MARKER"
  printf "SYS_MARKER=%s\n" "$SYS_MARKER"
  printf "Log: %s\n" "$LOG"
  exit 0
fi

# -----------------------------
# ensure dirs exist
# -----------------------------
next "Creating directories"
if [ "$SYSTEM" -eq 1 ]; then
  run_root mkdir -p "$DEST_BIN"
  run_root mkdir -p "$(dirname "$SYS_MARKER")"
else
  mkdir -p "$DEST_BIN"
  mkdir -p "$(dirname "$USER_MARKER")"
fi

# -----------------------------
# atomic marker writer
# -----------------------------
next "Writing repo marker"
write_marker(){
  marker="$1"
  tmp="$(mktemp_safe).marker"
  : >"$tmp" 2>/dev/null || die "Cannot write temp marker: $tmp"
  printf "%s\n" "$REPO_DIR" >"$tmp"
  if [ "$SYSTEM" -eq 1 ]; then
    run_root mv -f "$tmp" "$marker"
    run_root chmod 0644 "$marker" 2>/dev/null || :
  else
    mv -f "$tmp" "$marker"
    chmod 0644 "$marker" 2>/dev/null || :
  fi
}

if [ "$SYSTEM" -eq 1 ]; then
  write_marker "$SYS_MARKER"
else
  write_marker "$USER_MARKER"
fi

# -----------------------------
# safe shell quoting (no sed, POSIX)
# outputs a single-quoted literal
# -----------------------------
shell_quote(){
  s=$1
  printf "'"
  while :; do
    case "$s" in
      *"'"*)
        before=${s%%\'*}
        after=${s#*\'}
        printf "%s'\"'\"'" "$before"
        s=$after
        ;;
      *)
        printf "%s'" "$s"
        break
        ;;
    esac
  done
}

# -----------------------------
# wrapper builder (atomic + safe overwrite)
# -----------------------------
is_ours(){
  f="$1"
  [ -f "$f" ] || return 1
  # check for our signature comment
  head -n 3 "$f" 2>/dev/null | grep -q "Generated by tedit init" && return 0
  return 1
}

confirm_overwrite(){
  # only ask if interactive and not -y (init doesn't have -y; keep it simple)
  [ "$TTY" -eq 1 ] || return 1
  printf "%s%s?%s Overwrite existing %s [y/N] " "$BOLD" "" "$RESET" "$1" 1>&2
  read -r A || A=""
  case "$A" in y|Y|yes|YES) return 0 ;; *) return 1 ;; esac
}

install_file_atomic(){
  src="$1"
  dst="$2"
  mode="$3"

  if [ -e "$dst" ]; then
    if [ "$FORCE" -eq 1 ] || is_ours "$dst"; then
      : # ok to overwrite
    else
      warn "Refusing to overwrite non-tedit file: $dst (use --force)"
      rm -f "$src" 2>/dev/null || :
      return 0
    fi
  fi

  if [ "$SYSTEM" -eq 1 ]; then
    # Try install(1) if available, else mv+chmod
    if have install; then
      run_root install -m "$mode" "$src" "$dst" >>"$LOG" 2>&1 || {
        run_root mv -f "$src" "$dst" >>"$LOG" 2>&1 || die "Failed to place wrapper at $dst"
        run_root chmod "$mode" "$dst" 2>/dev/null || :
      }
      run_root rm -f "$src" 2>/dev/null || :
    else
      run_root mv -f "$src" "$dst" >>"$LOG" 2>&1 || die "Failed to place wrapper at $dst"
      run_root chmod "$mode" "$dst" 2>/dev/null || :
    fi
  else
    mv -f "$src" "$dst" >>"$LOG" 2>&1 || die "Failed to place wrapper at $dst"
    chmod "$mode" "$dst" 2>/dev/null || :
  fi
}

make_wrapper(){
  name="$1"     # tedit-install
  target="$2"   # install.sh
  dst="${DEST_BIN}/${name}"
  tmp="$(mktemp_safe).wrap"

  # content constants (quoted literals inside wrapper)
  APP_Q="$(shell_quote "$APP_NAME")"
  DEF_REPO_Q="$(shell_quote "$REPO_DIR")"
  TARGET_Q="$(shell_quote "./$target")"
  SYS_MARKER_Q="$(shell_quote "$SYS_MARKER")"
  USER_MARKER_Q="$(shell_quote "$USER_MARKER")"

  : >"$tmp" 2>/dev/null || die "Cannot write temp wrapper: $tmp"

  {
    printf "%s\n" "#!/usr/bin/env sh"
    printf "%s\n" "# Generated by tedit init (do not hand-edit unless you like pain)"
    printf "%s\n" "set -eu"
    printf "%s\n" "APP_NAME=$APP_Q"
    printf "%s\n" "DEFAULT_REPO_DIR=$DEF_REPO_Q"
    printf "%s\n" "MARKER_SYS=$SYS_MARKER_Q"
    printf "%s\n" "MARKER_USER=$USER_MARKER_Q"
    printf "%s\n" "TARGET=$TARGET_Q"
    printf "%s\n" ""
    printf "%s\n" "REPO_DIR=\${TEDIT_REPO:-}"
    printf "%s\n" "if [ -z \"\$REPO_DIR\" ] && [ -f \"\$MARKER_SYS\" ]; then REPO_DIR=\$(cat \"\$MARKER_SYS\" 2>/dev/null || true); fi"
    printf "%s\n" "if [ -z \"\$REPO_DIR\" ] && [ -f \"\$MARKER_USER\" ]; then REPO_DIR=\$(cat \"\$MARKER_USER\" 2>/dev/null || true); fi"
    printf "%s\n" "if [ -z \"\$REPO_DIR\" ]; then REPO_DIR=\"\$DEFAULT_REPO_DIR\"; fi"
    printf "%s\n" ""
    printf "%s\n" "if [ ! -f \"\$REPO_DIR/src/tedit.cpp\" ]; then"
    printf "%s\n" "  echo \"ERROR: \$APP_NAME repo not found at: \$REPO_DIR\" >&2"
    printf "%s\n" "  echo \"Hint: set TEDIT_REPO=/absolute/path/to/tedit or re-run init.sh in the repo.\" >&2"
    printf "%s\n" "  exit 1"
    printf "%s\n" "fi"
    printf "%s\n" "cd \"\$REPO_DIR\" || { echo \"ERROR: cannot cd to \$REPO_DIR\" >&2; exit 2; }"
    printf "%s\n" "if [ ! -f \"\$TARGET\" ]; then"
    printf "%s\n" "  echo \"ERROR: missing script: \$TARGET (repo looks incomplete)\" >&2"
    printf "%s\n" "  exit 1"
    printf "%s\n" "fi"
    printf "%s\n" "exec sh \"\$TARGET\" \"\$@\""
  } >>"$tmp"

  install_file_atomic "$tmp" "$dst" 0755

  if [ -x "$dst" ]; then
    ok "Installed wrapper: ${BOLD}${name}${RESET} → ${CYAN}${dst}${RESET}"
  else
    warn "Wrapper created but not executable: $dst"
  fi
}

# -----------------------------
# do it
# -----------------------------
finish_ui
info ">> Initializing $APP_NAME wrappers <<"
say "Repo: $REPO_DIR"
say "Mode: $( [ "$SYSTEM" -eq 1 ] && echo system || echo user )"
say "Bin : $DEST_BIN"
say "Log : $LOG"
[ "$TTY" -eq 1 ] && printf "\n"

# resume bar after header
draw_bar "$STEP" "$TOTAL" "Starting"

next "Creating wrappers"
make_wrapper "tedit-install"   "scripts/install.sh"
make_wrapper "tedit-update"    "scripts/update.sh"
make_wrapper "tedit-uninstall" "scripts/uninstall.sh"

next "PATH integration"
if [ "$SYSTEM" -eq 0 ] && [ "$NO_PATH" -eq 0 ]; then
  case ":$PATH:" in
    *:"$DEST_BIN":*) in_path=1 ;;
    *) in_path=0 ;;
  esac

  if [ "$in_path" -eq 0 ]; then
    # choose a profile file (zsh > bash > sh)
    profile=""
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
      {
        printf "\n# Added by %s init\n" "$APP_NAME"
        printf "export PATH=\"%s:\$PATH\"\n" "$DEST_BIN"
      } >>"$profile"
      ok "Added $DEST_BIN to PATH via $profile"
    else
      ok "PATH already mentions $DEST_BIN in $profile"
    fi
  else
    ok "$DEST_BIN already in PATH"
  fi
else
  ok "Skipping PATH changes"
fi

next "Final checks"
# quick verify wrappers exist
[ -x "$DEST_BIN/tedit-install" ]   || warn "Missing wrapper: $DEST_BIN/tedit-install"
[ -x "$DEST_BIN/tedit-update" ]    || warn "Missing wrapper: $DEST_BIN/tedit-update"
[ -x "$DEST_BIN/tedit-uninstall" ] || warn "Missing wrapper: $DEST_BIN/tedit-uninstall"

next "Done"
finish_ui
ok "$APP_NAME wrappers ready :D"

say ""
say "Try:"
say "  ${BOLD}tedit-install${RESET}"
say "  ${BOLD}tedit-update${RESET}"
say "  ${BOLD}tedit-uninstall${RESET}"

if [ "$SYSTEM" -eq 0 ]; then
  case ":$PATH:" in
    *:"$DEST_BIN":*) : ;;
    *)
      say ""
      warn "Your current shell may not see the wrappers yet."
      say "Run this once:"
      say "  ${BOLD}export PATH=\"${DEST_BIN}:\$PATH\"${RESET}"
      ;;
  esac
fi
