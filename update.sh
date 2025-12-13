#!/usr/bin/env sh
set -eu

APP_NAME="tedit"
DEFAULT_PREFIX="/usr/local"
USER_PREFIX="${PREFIX:-$HOME/.local}"

# -----------------------------
# Temp/log
# -----------------------------
mktemp_file() {
  if tmp=$(mktemp "${TMPDIR:-/tmp}/${APP_NAME}-update.XXXXXX" 2>/dev/null); then
    printf "%s" "$tmp"; return 0
  fi
  if tmp=$(mktemp -t "${APP_NAME}-update" 2>/dev/null); then
    printf "%s" "$tmp"; return 0
  fi
  printf "%s" "${TMPDIR:-/tmp}/${APP_NAME}-update.$(date +%s).$$"
}
LOG="${LOG_FILE:-$(mktemp_file)}.log"
: >"$LOG" 2>/dev/null || { echo "ERROR: cannot write log: $LOG" >&2; exit 2; }

have(){ command -v "$1" >/dev/null 2>&1; }

TTY=0; [ -t 1 ] && TTY=1

# -----------------------------
# Colors
# -----------------------------
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

log_line(){ printf "%s\n" "$*" >>"$LOG"; }

say(){ printf "%s\n" "$*"; log_line "$*"; }
ok(){  say "${GREEN}${BOLD}✓${RESET} $*"; }
info(){ say "${CYAN}${BOLD}::${RESET} $*"; }
warn(){ say "${YELLOW}${BOLD}!${RESET} $*"; }
err(){  say "${RED}${BOLD}x${RESET} $*"; }

finish_ui(){ [ "$TTY" -eq 1 ] && printf "\r\033[K%s" "$CURSOR_SHOW"; }

die(){
  finish_ui 2>/dev/null || :
  err "ERROR: $*"
  say "Log: $LOG"
  exit 1
}

# -----------------------------
# Cursor hide/show
# -----------------------------
CURSOR_HIDE=""; CURSOR_SHOW=""
if [ "$TTY" -eq 1 ] && have tput; then
  CURSOR_HIDE="$(tput civis 2>/dev/null || true)"
  CURSOR_SHOW="$(tput cnorm 2>/dev/null || true)"
fi
[ "$TTY" -eq 1 ] && printf "%s" "$CURSOR_HIDE"
trap 'finish_ui' EXIT INT TERM

# -----------------------------
# Options
# -----------------------------
VERBOSE="${VERBOSE-0}"
INSTALL_DEPS=1
DEPS_ONLY=0
NO_GIT=0
NO_PULL=0
NO_BUILD=0
NO_INSTALL=0
INSTALL_MAN=1
DO_STRIP=1
CUSTOM_PREFIX=""
FORCE_PREFIX_MODE="auto" # auto|system|user

usage(){
  cat <<EOF
$APP_NAME updater

Usage:
  ./tedit-update [options]

Repo selection:
  --repo PATH        Explicit repo path (or set TEDIT_REPO)
  --no-git           Don't use git (just rebuild current tree)
  --no-pull          Don't fetch/pull (rebuild current checkout)

Install behavior:
  --prefix PATH      Install prefix (auto unless specified)
  --system           Force system prefix ($DEFAULT_PREFIX)
  --user             Force user prefix ($USER_PREFIX)
  --deps-only        Only ensure dependencies, then exit
  --no-deps          Do not install dependencies
  --no-build         Skip building
  --no-install       Skip installing
  --no-man           Skip man page update
  --no-strip         Skip strip
  --verbose          More output
  -h, --help         Help

Environment:
  TEDIT_REPO=/abs/path   PREFIX=/some/prefix  VERBOSE=1  NO_COLOR=1  LOG_FILE=/path
EOF
}

REPO_DIR="${TEDIT_REPO:-}"

while [ $# -gt 0 ]; do
  case "$1" in
    --repo) shift; [ $# -gt 0 ] || die "--repo needs a path"; REPO_DIR="$1" ;;
    --prefix) shift; [ $# -gt 0 ] || die "--prefix needs a path"; CUSTOM_PREFIX="$1" ;;
    --system) FORCE_PREFIX_MODE="system" ;;
    --user) FORCE_PREFIX_MODE="user" ;;
    --no-git) NO_GIT=1 ;;
    --no-pull) NO_PULL=1 ;;
    --deps-only) DEPS_ONLY=1 ;;
    --no-deps) INSTALL_DEPS=0 ;;
    --no-build) NO_BUILD=1 ;;
    --no-install) NO_INSTALL=1 ;;
    --no-man) INSTALL_MAN=0 ;;
    --no-strip) DO_STRIP=0 ;;
    --verbose) VERBOSE=1 ;;
    -h|--help) usage; exit 0 ;;
    *) die "Unknown option: $1 (use --help)" ;;
  esac
  shift
done

# -----------------------------
# Privileges
# -----------------------------
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if have sudo; then SUDO="sudo"
  elif have doas; then SUDO="doas"
  fi
fi

auth_once(){
  [ -z "$SUDO" ] && return 0
  info "Elevating privileges (may prompt once)..."
  if [ "$SUDO" = "sudo" ]; then sudo -v
  else $SUDO true
  fi
}

# -----------------------------
# Safe runner with correct exit codes
# -----------------------------
run_logged(){
  cmd="$1"
  if [ "$VERBOSE" = "1" ]; then
    tmpout="$(mktemp_file)"
    set +e
    sh -c "$cmd" >"$tmpout" 2>&1
    rc=$?
    set -e
    cat "$tmpout"
    cat "$tmpout" >>"$LOG"
    rm -f "$tmpout" 2>/dev/null || :
    return "$rc"
  fi
  set +e
  sh -c "$cmd" >>"$LOG" 2>&1
  rc=$?
  set -e
  return "$rc"
}

spinner(){
  msg="$1"; cmd="$2"
  if [ "$TTY" -ne 1 ] || [ "$VERBOSE" = "1" ]; then
    info "$msg"
    run_logged "$cmd" || return $?
    return 0
  fi

  run_logged "$cmd" &
  pid=$!
  frames='-\|/'; i=0
  while kill -0 "$pid" 2>/dev/null; do
    i=$(( (i+1) % 4 ))
    c=$(printf %s "$frames" | cut -c $((i+1)))
    printf "\r\033[K%s%s[%s]%s %s" "$CYAN" "$BOLD" "$c" "$RESET" "$msg"
    sleep 0.1
  done
  wait "$pid"
  rc=$?
  printf "\r\033[K"
  return "$rc"
}

run_root(){
  msg="$1"; cmd="$2"
  if [ -z "$SUDO" ]; then
    spinner "$msg" "$cmd"
    return $?
  fi
  if $SUDO -n true 2>/dev/null; then
    spinner "$msg" "$SUDO sh -c $(printf %s "$cmd" | sed "s/'/'\\\\''/g;1s/^/'/;\$s/\$/'/")"
  else
    warn "[auth] $msg"
    $SUDO sh -c "$cmd" 2>&1 | tee -a "$LOG"
  fi
}

# -----------------------------
# Detect OS + package manager
# -----------------------------
ID=""; ID_LIKE=""
if [ -r /etc/os-release ]; then
  # shellcheck disable=SC1091
  . /etc/os-release || true
fi
ID="${ID-}"; ID_LIKE="${ID_LIKE-}"

PKG=""
case "${ID:-}" in
  alpine|postmarketos|chimera) PKG="apk" ;;
  arch|manjaro|endeavouros|arco|artix|cachyos) PKG="pacman" ;;
  debian|ubuntu|pop|elementary|linuxmint|zorin) PKG="apt" ;;
  fedora|rhel|centos|rocky|almalinux) PKG="dnf" ;;
  opensuse*|sles) PKG="zypper" ;;
  gentoo) PKG="emerge" ;;
  void) PKG="xbps-install" ;;
  solus) PKG="eopkg" ;;
  freebsd|dragonfly|midnightbsd|ghostbsd) PKG="pkg" ;;
  openbsd) PKG="pkg_add" ;;
  netbsd) PKG="pkgin" ;;
esac

[ -z "$PKG" ] && have apk && PKG="apk" || :
[ -z "$PKG" ] && have apt-get && PKG="apt" || :
[ -z "$PKG" ] && have apt && PKG="apt" || :
[ -z "$PKG" ] && have dnf && PKG="dnf" || :
[ -z "$PKG" ] && have yum && PKG="yum" || :
[ -z "$PKG" ] && have pacman && PKG="pacman" || :
[ -z "$PKG" ] && have zypper && PKG="zypper" || :
[ -z "$PKG" ] && have xbps-install && PKG="xbps-install" || :
[ -z "$PKG" ] && have eopkg && PKG="eopkg" || :
[ -z "$PKG" ] && have emerge && PKG="emerge" || :
[ -z "$PKG" ] && have pkg && PKG="pkg" || :
[ -z "$PKG" ] && have pkg_add && PKG="pkg_add" || :
[ -z "$PKG" ] && have pkgin && PKG="pkgin" || :
[ -z "$PKG" ] && have brew && PKG="brew" || :
[ -z "$PKG" ] && have port && PKG="port" || :
[ -z "$PKG" ] && have nix-shell && PKG="nix" || :
[ -z "$PKG" ] && have guix && PKG="guix" || :

# -----------------------------
# Tool selection
# -----------------------------
choose_cxx(){
  if have c++; then printf %s "c++"
  elif have g++; then printf %s "g++"
  elif have clang++; then printf %s "clang++"
  else printf %s ""
  fi
}

choose_make(){
  case "${ID:-}" in
    freebsd|dragonfly|midnightbsd|ghostbsd|openbsd|netbsd) have gmake && { printf %s "gmake"; return; } ;;
  esac
  printf %s "make"
}

# -----------------------------
# Deps (git/make/compiler)
# -----------------------------
cxx_test(){
  cxx="$1"
  tmp="$(mktemp_file)"
  cat >"$tmp.cpp" <<'EOF'
#include <iostream>
int main(){ std::cout << "ok\n"; return 0; }
EOF
  set +e
  "$cxx" -std=c++17 -o "$tmp.out" "$tmp.cpp" >/dev/null 2>&1
  rc=$?
  set -e
  rm -f "$tmp.cpp" "$tmp.out" 2>/dev/null || :
  return "$rc"
}

need_deps(){
  MAKE_BIN="$1"
  CXX_BIN="$2"
  [ "$NO_GIT" -eq 1 ] || have git || return 0
  have "$MAKE_BIN" || return 0
  [ -n "$CXX_BIN" ] || return 0
  cxx_test "$CXX_BIN" || return 0
  return 1
}

install_deps(){
  info "Installing deps via: ${PKG:-unknown}"
  case "$PKG" in
    apk)  run_root "apk deps" "apk add --no-cache --update-cache git build-base" ;;
    apt)
      run_root "apt update" "sh -c 'apt-get update -y || apt update -y'"
      run_root "apt deps"   "sh -c 'apt-get install -y git build-essential make || apt install -y git build-essential make'"
      ;;
    dnf)  run_root "dnf deps" "dnf -y install git make gcc gcc-c++ || true" ;;
    yum)  run_root "yum deps" "yum -y install git make gcc gcc-c++ || true" ;;
    pacman)
      run_root "pacman sync" "pacman -Sy --noconfirm"
      run_root "pacman deps" "pacman -S --needed --noconfirm base-devel git"
      ;;
    zypper) run_root "zypper deps" "zypper -n install git make gcc-c++ || true" ;;
    xbps-install) run_root "xbps deps" "xbps-install -Sy git gcc make" ;;
    eopkg) run_root "eopkg deps" "sh -c 'eopkg -y it -c system.devel git || eopkg -y it make gcc binutils git'" ;;
    emerge)
      warn "Gentoo: may be interactive depending on your setup."
      run_root "emerge deps" "emerge --quiet-build=y --oneshot dev-vcs/git sys-devel/make sys-devel/gcc || true"
      ;;
    pkg) run_root "pkg deps" "pkg update -f || true; pkg install -y git gmake llvm || pkg install -y git gmake || true" ;;
    pkg_add) warn "OpenBSD may need PKG_PATH."; run_root "pkg_add deps" "pkg_add git gmake || true" ;;
    pkgin) run_root "pkgin deps" "pkgin -y update || true; pkgin -y install git gmake || true" ;;
    brew) spinner "brew deps" "brew install git make gcc || true" ;;
    port)
      run_root "macports selfupdate" "port selfupdate || true"
      run_root "macports deps" "port install git gmake clang-17 || port install git gmake || true"
      ;;
    nix)  warn "Nix: updater can run in nix-shell when needed." ;;
    guix) warn "Guix: updater can run in guix shell when needed." ;;
    *)
      die "No supported package manager detected. Install git, make, and a C++17 compiler."
      ;;
  esac
}

# -----------------------------
# Repo discovery (stronger + marker support)
# -----------------------------
SCRIPT_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)

repo_has_markers(){
  d="$1"
  [ -f "$d/tedit.cpp" ] && return 0
  [ -f "$d/Makefile" ] && grep -qE '(^|\s)tedit(\s|$)' "$d/Makefile" 2>/dev/null && return 0
  return 1
}

read_marker(){
  m="$1"
  [ -f "$m" ] || return 1
  p="$(cat "$m" 2>/dev/null || true)"
  [ -n "$p" ] && repo_has_markers "$p" && { printf "%s" "$p"; return 0; }
  return 1
}

locate_repo(){
  # explicit
  if [ -n "$REPO_DIR" ] && repo_has_markers "$REPO_DIR"; then
    printf "%s" "$REPO_DIR"; return 0
  fi
  # CWD
  if repo_has_markers "$PWD"; then
    printf "%s" "$PWD"; return 0
  fi
  # script dir
  if repo_has_markers "$SCRIPT_DIR"; then
    printf "%s" "$SCRIPT_DIR"; return 0
  fi
  # markers
  if p="$(read_marker "/usr/local/share/tedit/repo" 2>/dev/null || true)"; then
    printf "%s" "$p"; return 0
  fi
  if p="$(read_marker "$HOME/.local/share/tedit/repo" 2>/dev/null || true)"; then
    printf "%s" "$p"; return 0
  fi
  # common fallbacks
  for d in "$HOME/tedit" "$HOME/src/tedit" "$HOME/dev/tedit" "$HOME/projects/tedit"; do
    repo_has_markers "$d" && { printf "%s" "$d"; return 0; }
  done
  return 1
}

# -----------------------------
# Prefix inference (match installed binary if possible)
# -----------------------------
infer_prefix_from_installed(){
  cur="$(command -v "$APP_NAME" 2>/dev/null || true)"
  [ -z "$cur" ] && return 1
  case "$cur" in
    */.local/bin/"$APP_NAME") printf "%s" "$HOME/.local"; return 0 ;;
    */bin/"$APP_NAME")
      # heuristic: /usr/local/bin/tedit -> /usr/local
      p="$(dirname "$(dirname "$cur")")"
      [ -n "$p" ] && printf "%s" "$p" && return 0
      ;;
  esac
  return 1
}

choose_prefix(){
  if [ -n "$CUSTOM_PREFIX" ]; then
    printf "%s" "$CUSTOM_PREFIX"; return 0
  fi
  case "$FORCE_PREFIX_MODE" in
    system) printf "%s" "$DEFAULT_PREFIX"; return 0 ;;
    user)   printf "%s" "$USER_PREFIX"; return 0 ;;
  esac
  if p="$(infer_prefix_from_installed 2>/dev/null || true)"; then
    printf "%s" "$p"; return 0
  fi
  if [ "$(id -u)" -eq 0 ]; then printf "%s" "$DEFAULT_PREFIX"
  else printf "%s" "$USER_PREFIX"
  fi
}

# -----------------------------
# Git helpers (safe-ish)
# -----------------------------
quiet_git(){
  GIT_TERMINAL_PROMPT=0 git \
    -c advice.statusHints=false \
    -c advice.detachedHead=false "$@" >>"$LOG" 2>&1
}

in_git_repo(){
  have git || return 1
  git rev-parse --is-inside-work-tree >/dev/null 2>&1
}

git_dirty(){
  git update-index -q --refresh >/dev/null 2>&1 || true
  git diff --quiet --ignore-submodules -- || return 0
  git diff --cached --quiet --ignore-submodules -- || return 0
  return 1
}

# -----------------------------
# Start
# -----------------------------
info "Updating $APP_NAME"
say "Log: $LOG"

REPO_DIR_FOUND="$(locate_repo 2>/dev/null || true)"
[ -n "$REPO_DIR_FOUND" ] || die "Could not locate repo. Use --repo /path or set TEDIT_REPO."
cd "$REPO_DIR_FOUND" || die "Cannot cd to repo: $REPO_DIR_FOUND"
ok "Repo: $REPO_DIR_FOUND"

MAKE_BIN="$(choose_make)"
CXX_BIN="$(choose_cxx)"

info "Detected:"
say "  make: $MAKE_BIN"
say "  c++ : ${CXX_BIN:-none}"
say "  pkg : ${PKG:-none}"

auth_once

# Nix/Guix mode (ephemeral env)
if [ "$PKG" = "nix" ] && [ "$INSTALL_DEPS" -eq 1 ]; then
  warn "Nix detected: using nix-shell for deps + build."
  spinner "nix-shell: build" "nix-shell -p gnumake gcc pkg-config lua --run '$MAKE_BIN CXX=${CXX_BIN:-c++}'" || die "Build failed."
elif [ "$PKG" = "guix" ] && [ "$INSTALL_DEPS" -eq 1 ]; then
  warn "Guix detected: using guix shell for deps + build."
  spinner "guix: build" "guix shell gnu-make gcc-toolchain pkg-config lua -- $MAKE_BIN CXX=${CXX_BIN:-c++}" || die "Build failed."
else
  if need_deps "$MAKE_BIN" "$CXX_BIN"; then
    if [ "$INSTALL_DEPS" -eq 1 ]; then
      install_deps
      MAKE_BIN="$(choose_make)"
      CXX_BIN="$(choose_cxx)"
      need_deps "$MAKE_BIN" "$CXX_BIN" && die "Deps still missing after install. Check log."
      ok "Dependencies OK"
    else
      die "Missing deps and --no-deps set. Need: git (unless --no-git), make, C++17 compiler."
    fi
  else
    ok "Dependencies OK"
  fi
fi

[ "$DEPS_ONLY" -eq 1 ] && { ok "Deps-only complete."; exit 0; }

# Git update flow (optional)
if [ "$NO_GIT" -eq 0 ] && in_git_repo; then
  ok "Git repo detected"
  if [ "$NO_PULL" -eq 0 ]; then
    spinner "Fetching..." "quiet_git fetch --quiet --all --tags --prune || true" || true

    upstream="$(git rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>/dev/null || true)"
    branch="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || true)"

    if [ -z "$upstream" ] && [ -n "$branch" ] && git rev-parse "origin/$branch" >/dev/null 2>&1; then
      upstream="origin/$branch"
    fi

    STASHED=0
    if git_dirty; then
      warn "Working tree dirty — stashing temporarily"
      spinner "Stashing..." "quiet_git stash push -u -m 'tedit-autostash-$(date +%s)' || true" || true
      STASHED=1
    fi

    if [ -n "$upstream" ]; then
      behind="$(git rev-list --count "HEAD..$upstream" 2>/dev/null || echo 0)"
      if [ "$behind" -eq 0 ]; then
        ok "Already up to date with $upstream"
      else
        spinner "Pulling updates..." "quiet_git pull --rebase --autostash --stat || quiet_git pull --rebase || quiet_git pull" || warn "Git pull had issues (continuing rebuild anyway)."
      fi
    else
      warn "No upstream set; skipping pull."
    fi

    if [ "$STASHED" -eq 1 ]; then
      spinner "Restoring stash..." "quiet_git stash pop || true" || warn "Could not auto-apply stash cleanly. Your stash is still there."
    fi
  else
    warn "--no-pull set: not fetching/pulling"
  fi
else
  [ "$NO_GIT" -eq 1 ] && warn "--no-git set: rebuilding current tree"
  [ "$NO_GIT" -eq 0 ] && warn "Not a git repo: rebuilding current tree"
fi

# Build
if [ "$NO_BUILD" -eq 0 ]; then
  info "Building..."
  spinner "make" "CXX='${CXX_BIN:-c++}' $MAKE_BIN" || die "Build failed."
  ok "Build OK"
else
  warn "--no-build set: skipping"
fi

# Install
TARGET_PREFIX="$(choose_prefix)"
info "Install prefix: $TARGET_PREFIX"

if [ "$NO_INSTALL" -eq 0 ]; then
  if [ "$TARGET_PREFIX" = "$DEFAULT_PREFIX" ] && [ "$(id -u)" -ne 0 ] && [ -z "$SUDO" ]; then
    die "System prefix chosen but no sudo/doas available. Use --user or --prefix."
  fi

  if [ "$(id -u)" -eq 0 ] || [ "$TARGET_PREFIX" = "$USER_PREFIX" ] || [ -z "$SUDO" ]; then
    spinner "Installing..." "$MAKE_BIN PREFIX='$TARGET_PREFIX' install" || die "Install failed."
  else
    run_root "Installing..." "$MAKE_BIN PREFIX='$TARGET_PREFIX' install" || die "Install failed."
  fi
  ok "Installed"
else
  warn "--no-install set: skipping install"
fi

# Strip
if [ "$DO_STRIP" -eq 1 ] && have strip; then
  BIN="${TARGET_PREFIX}/bin/${APP_NAME}"
  if [ -e "$BIN" ]; then
    spinner "Stripping binary..." "${SUDO:+$SUDO }strip '$BIN' 2>/dev/null || true"
  fi
fi

# Man page
if [ "$INSTALL_MAN" -eq 1 ]; then
  if [ -f "./${APP_NAME}.1" ]; then
    MAN_DIR="${TARGET_PREFIX}/share/man/man1"
    if [ "$(id -u)" -eq 0 ] || [ "$TARGET_PREFIX" = "$USER_PREFIX" ] || [ -z "$SUDO" ]; then
      spinner "Man page..." "mkdir -p '$MAN_DIR' && install -m 0644 './${APP_NAME}.1' '$MAN_DIR/${APP_NAME}.1'" || true
      have gzip && spinner "Compressing man..." "gzip -f -9 '$MAN_DIR/${APP_NAME}.1' 2>/dev/null || true" || true
    else
      run_root "Man page..." "mkdir -p '$MAN_DIR' && install -m 0644 './${APP_NAME}.1' '$MAN_DIR/${APP_NAME}.1'" || true
      have gzip && run_root "Compressing man..." "gzip -f -9 '$MAN_DIR/${APP_NAME}.1' 2>/dev/null || true" || true
    fi
  else
    warn "No man page in repo; skipping"
  fi
else
  warn "--no-man set: skipping man page"
fi

ok "$APP_NAME updated successfully :D"
say "Binary: ${TARGET_PREFIX}/bin/${APP_NAME}"
say "Log: $LOG"
