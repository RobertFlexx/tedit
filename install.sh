#!/usr/bin/env sh
set -eu

APP_NAME="tedit"
DEFAULT_PREFIX="/usr/local"
USER_PREFIX="${PREFIX:-$HOME/.local}"

# -----------------------------
# Basics
# -----------------------------
SCRIPT_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)
cd "$SCRIPT_DIR" || { echo "ERROR: cannot cd to $SCRIPT_DIR" >&2; exit 2; }

mktemp_file() {
  if tmp=$(mktemp "${TMPDIR:-/tmp}/${APP_NAME}-install.XXXXXX" 2>/dev/null); then
    printf "%s" "$tmp"; return 0
  fi
  if tmp=$(mktemp -t "${APP_NAME}-install" 2>/dev/null); then
    printf "%s" "$tmp"; return 0
  fi
  printf "%s" "${TMPDIR:-/tmp}/${APP_NAME}-install.$(date +%s).$$"
}

LOG="${LOG_FILE:-$(mktemp_file)}.log"
: >"$LOG" 2>/dev/null || { echo "ERROR: cannot write log file: $LOG" >&2; exit 2; }

have(){ command -v "$1" >/dev/null 2>&1; }

TTY=0
[ -t 1 ] && TTY=1

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

# -----------------------------
# Output helpers (always visible + always logged)
# -----------------------------
log_line(){ printf "%s\n" "$*" >>"$LOG"; }

say(){ printf "%s\n" "$*"; log_line "$*"; }
ok(){  say "${GREEN}${BOLD}✓${RESET} $*"; }
info(){ say "${CYAN}${BOLD}::${RESET} $*"; }
warn(){ say "${YELLOW}${BOLD}!${RESET} $*"; }
err(){  say "${RED}${BOLD}x${RESET} $*"; }

die(){
  finish_ui 2>/dev/null || :
  err "ERROR: $*"
  say "Log: $LOG"
  exit 1
}

# -----------------------------
# Options
# -----------------------------
VERBOSE="${VERBOSE-0}"
INSTALL_DEPS=1
DEPS_ONLY=0
INSTALL_MAN=1
DO_STRIP=1
UPDATE_PATH=1
FORCE_PREFIX_MODE="auto"  # auto|system|user
CUSTOM_PREFIX=""

usage(){
  cat <<EOF
$APP_NAME installer

Usage:
  ./install.sh [options]

Options:
  --prefix PATH      Install prefix (default: $DEFAULT_PREFIX, or user prefix if not root)
  --system           Force system install (needs root)
  --user             Force user install (~/.local)
  --no-deps          Do not install dependencies automatically
  --deps-only        Only install dependencies, then exit
  --no-man           Skip man page install
  --no-strip         Skip stripping binary
  --no-path          Do not edit shell profiles for PATH
  --verbose          More output (also logs)
  -h, --help         Show this help

Environment:
  PREFIX, VERBOSE=1, NO_COLOR=1, LOG_FILE=/path/to/log
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --prefix) shift; [ $# -gt 0 ] || die "--prefix needs a path"; CUSTOM_PREFIX="$1" ;;
    --system) FORCE_PREFIX_MODE="system" ;;
    --user) FORCE_PREFIX_MODE="user" ;;
    --no-deps) INSTALL_DEPS=0 ;;
    --deps-only) DEPS_ONLY=1 ;;
    --no-man) INSTALL_MAN=0 ;;
    --no-strip) DO_STRIP=0 ;;
    --no-path) UPDATE_PATH=0 ;;
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
  else SUDO=""
  fi
fi

auth_once(){
  [ -z "$SUDO" ] && return 0
  info "Elevating privileges (may prompt once)..."
  if [ "$SUDO" = "sudo" ]; then
    sudo -v
  else
    $SUDO true
  fi
}

# -----------------------------
# UI (simple, safe)
# -----------------------------
CURSOR_HIDE=""; CURSOR_SHOW=""
if [ "$TTY" -eq 1 ] && have tput; then
  CURSOR_HIDE="$(tput civis 2>/dev/null || true)"
  CURSOR_SHOW="$(tput cnorm 2>/dev/null || true)"
fi

finish_ui(){ [ "$TTY" -eq 1 ] && printf "\r\033[K%s" "$CURSOR_SHOW"; }

trap 'finish_ui' EXIT INT TERM
[ "$TTY" -eq 1 ] && printf "%s" "$CURSOR_HIDE"

run_logged(){
  tmpout="$(mktemp_file)"
  set +e
  "$@" >"$tmpout" 2>&1
  rc=$?
  set -e

  if [ "$VERBOSE" = "1" ]; then
    cat "$tmpout"
  fi
  cat "$tmpout" >>"$LOG"
  rm -f "$tmpout" 2>/dev/null || :
  return "$rc"
}

spinner(){
  msg="$1"; shift
  if [ "$TTY" -ne 1 ] || [ "$VERBOSE" = "1" ]; then
    info "$msg"
    run_logged "$@" || return $?
    return 0
  fi

  run_logged "$@" &
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
  msg="$1"; shift
  if [ -z "$SUDO" ]; then
    spinner "$msg" "$@"; return $?
  fi

  # If non-interactive auth is possible, keep spinner; otherwise run plainly (so prompts work)
  if [ "$SUDO" = "sudo" ] && sudo -n true 2>/dev/null; then
    spinner "$msg" sudo -n "$@"; return $?
  fi
  if [ "$SUDO" = "doas" ] && doas -n true 2>/dev/null; then
    spinner "$msg" doas -n "$@"; return $?
  fi

  warn "[auth] $msg"
  run_logged "$SUDO" "$@"
}

# -----------------------------
# Package manager detection
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

# fallback by availability
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
  # Prefer gmake if present (BSD/macOS/GNU make installs)
  if have gmake; then printf %s "gmake"; return 0; fi
  printf %s "make"
}

choose_pc(){
  if have pkg-config; then printf %s "pkg-config"; return 0; fi
  if have pkgconf; then printf %s "pkgconf"; return 0; fi
  printf %s ""
}

# -----------------------------
# Dependency checks (real checks)
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

# Globals filled in by Lua detection
LUA_CFLAGS=""
LUA_LIBS=""
LUA_PCNAME=""

lua_pc_pick(){
  pcbin="$1"
  [ -n "$pcbin" ] || return 1
  for n in lua5.4 lua-5.4 lua54 lua5.3 lua-5.3 lua53 lua; do
    "$pcbin" --exists "$n" >/dev/null 2>&1 && { printf %s "$n"; return 0; }
  done
  return 1
}

lua_try_compile(){
  cxx="$1"; cflags="$2"; libs="$3"
  tmp="$(mktemp_file)"
  cat >"$tmp.c" <<'EOF'
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
int main(void){ lua_State* L = luaL_newstate(); lua_close(L); return 0; }
EOF
  set +e
  # shellcheck disable=SC2086
  "$cxx" -std=c++17 $cflags -o "$tmp.out" "$tmp.c" $libs >/dev/null 2>&1
  rc=$?
  set -e
  rm -f "$tmp.c" "$tmp.out" 2>/dev/null || :
  return "$rc"
}

detect_lua_flags(){
  cxx="$1"
  pcbin="$2"

  LUA_CFLAGS=""; LUA_LIBS=""; LUA_PCNAME=""

  # 1) Prefer pkg-config if available
  if [ -n "$pcbin" ]; then
    pcname="$(lua_pc_pick "$pcbin" 2>/dev/null || true)"
    if [ -n "$pcname" ]; then
      cflags="$("$pcbin" --cflags "$pcname" 2>/dev/null || true)"
      libs="$("$pcbin" --libs "$pcname" 2>/dev/null || true)"
      if lua_try_compile "$cxx" "$cflags" "$libs"; then
        LUA_PCNAME="$pcname"
        LUA_CFLAGS="$cflags"
        LUA_LIBS="$libs"
        return 0
      fi
    fi
  fi

  # 2) Fallback: common include/lib combos (for systems without pkg-config .pc files)
  for inc in /usr/include/lua5.4 /usr/local/include/lua5.4 \
             /usr/include/lua54 /usr/local/include/lua54 \
             /usr/include/lua /usr/local/include/lua; do
    [ -d "$inc" ] || continue
    for lib in lua5.4 lua54 lua; do
      # try with and without -ldl (macOS doesn’t have libdl)
      if lua_try_compile "$cxx" "-I$inc" "-l$lib -lm"; then
        LUA_CFLAGS="-I$inc"
        LUA_LIBS="-l$lib -lm"
        return 0
      fi
      if lua_try_compile "$cxx" "-I$inc" "-l$lib -lm -ldl"; then
        LUA_CFLAGS="-I$inc"
        LUA_LIBS="-l$lib -lm -ldl"
        return 0
      fi
    done
  done

  return 1
}

deps_missing(){
  MAKE_BIN="$1"
  CXX_BIN="$2"
  PC_BIN="$3"

  have "$MAKE_BIN" || return 0
  [ -n "$CXX_BIN" ] || return 0
  cxx_test "$CXX_BIN" || return 0

  detect_lua_flags "$CXX_BIN" "$PC_BIN" || return 0
  return 1
}

# -----------------------------
# Install dependencies
# -----------------------------
install_deps(){
  info "Installing dependencies via: $PKG"
  case "$PKG" in
    apk)
      run_root "apk: build tools" apk add --no-cache --update-cache build-base || true
      run_root "apk: pkgconf"     apk add --no-cache --update-cache pkgconf || true
      run_root "apk: lua dev"     apk add --no-cache --update-cache lua5.4-dev lua5.4 || \
                                  apk add --no-cache --update-cache lua-dev lua || \
                                  apk add --no-cache --update-cache lua5.3-dev lua5.3 || true
      ;;
    apt)
      run_root "apt: update index" sh -c "apt-get update -y || apt update -y" || true
      run_root "apt: build tools"  sh -c "apt-get install -y build-essential || apt install -y build-essential" || true
      run_root "apt: pkg-config"   sh -c "apt-get install -y pkg-config || apt install -y pkg-config" || true
      run_root "apt: lua dev"      sh -c "apt-get install -y liblua5.4-dev lua5.4 || apt-get install -y liblua5.3-dev lua5.3 || apt-get install -y liblua5.2-dev lua5.2 || true" || true
      ;;
    dnf)
      run_root "dnf: build tools" dnf -y install make gcc gcc-c++ || true
      run_root "dnf: pkg-config"  dnf -y install pkgconf-pkg-config pkgconfig pkgconf || true
      run_root "dnf: lua dev"     dnf -y install lua lua-devel || true
      ;;
    yum)
      run_root "yum: build tools" yum -y install make gcc gcc-c++ || true
      run_root "yum: pkg-config"  yum -y install pkgconf-pkg-config pkgconfig pkgconf || true
      run_root "yum: lua dev"     yum -y install lua lua-devel || true
      ;;
    pacman)
      run_root "pacman: sync" pacman -Sy --noconfirm || true
      run_root "pacman: deps" pacman -S --needed --noconfirm base-devel pkgconf lua || true
      ;;
    zypper)
      run_root "zypper: refresh" zypper -n refresh || true
      run_root "zypper: build tools" zypper -n install make gcc-c++ || true
      run_root "zypper: pkg-config"  zypper -n install pkgconf pkg-config || true
      run_root "zypper: lua dev"     zypper -n install lua54-devel lua-devel lua || true
      ;;
    xbps-install)
      run_root "xbps: sync" xbps-install -Sy || true
      run_root "xbps: build tools" xbps-install -y base-devel || xbps-install -y gcc make binutils || true
      run_root "xbps: pkg-config"  xbps-install -y pkg-config || xbps-install -y pkgconf || true
      run_root "xbps: lua dev"     xbps-install -y lua54 lua54-devel || xbps-install -y lua lua-devel || true
      ;;
    eopkg)
      run_root "eopkg: system.devel" eopkg -y it -c system.devel || eopkg -y it make gcc binutils || true
      run_root "eopkg: pkg-config"   eopkg -y it pkg-config || true
      run_root "eopkg: lua dev"      eopkg -y it lua-devel || eopkg -y it lua || true
      ;;
    emerge)
      warn "Gentoo: this may be interactive depending on your setup."
      # Correct Gentoo atoms:
      # - make         => dev-build/make  (your log failed because it tried sys-devel/make)
      # - pkg-config   => dev-util/pkgconf (pkg-config compatible) / virtual/pkgconfig
      # - lua          => dev-lang/lua
      run_root "emerge: base tools" emerge --quiet-build=y --oneshot dev-build/make dev-util/pkgconf virtual/pkgconfig || true
      run_root "emerge: lua"        emerge --quiet-build=y --oneshot dev-lang/lua || true
      ;;
    pkg)
      run_root "pkg: update" pkg update -f || true
      run_root "pkg: deps"   pkg install -y gmake pkgconf lua54 || pkg install -y gmake pkgconf lua53 || pkg install -y gmake pkgconf lua || true
      ;;
    pkg_add)
      warn "OpenBSD: you may need PKG_PATH set (see pkg_add(1))."
      run_root "pkg_add: deps" pkg_add gmake pkgconf lua%5.4 || pkg_add gmake pkgconf lua || true
      ;;
    pkgin)
      run_root "pkgin: update" pkgin -y update || true
      run_root "pkgin: deps"   pkgin -y install gmake pkgconf lua54 || pkgin -y install gmake pkgconf lua || true
      ;;
    brew)
      spinner "brew: deps" brew install make pkg-config lua || true
      ;;
    port)
      run_root "macports: selfupdate" port selfupdate || true
      run_root "macports: deps"       port install gmake pkgconfig lua || true
      ;;
    nix)
      warn "Nix: using a temporary build environment (no system changes)."
      ;;
    guix)
      warn "Guix: using a temporary build environment (no system changes)."
      ;;
    *)
      die "Unsupported/undetected package manager. Install: make, a C++17 compiler, and Lua dev headers/libs."
      ;;
  esac
}

# -----------------------------
# Prefix selection
# -----------------------------
TARGET_PREFIX=""
if [ -n "$CUSTOM_PREFIX" ]; then
  TARGET_PREFIX="$CUSTOM_PREFIX"
else
  case "$FORCE_PREFIX_MODE" in
    system) TARGET_PREFIX="$DEFAULT_PREFIX" ;;
    user)   TARGET_PREFIX="$USER_PREFIX" ;;
    auto)
      if [ "$(id -u)" -eq 0 ]; then TARGET_PREFIX="$DEFAULT_PREFIX"
      else TARGET_PREFIX="$USER_PREFIX"
      fi
      ;;
    *) TARGET_PREFIX="$USER_PREFIX" ;;
  esac
fi

if [ "$FORCE_PREFIX_MODE" = "system" ] && [ "$(id -u)" -ne 0 ] && [ -z "$SUDO" ]; then
  die "System install requested but no sudo/doas found. Re-run as root, or use --user / --prefix."
fi

# -----------------------------
# Start
# -----------------------------
info "Installing $APP_NAME"
say "Log: $LOG"

[ -f "tedit.cpp" ] || die "Run this from the $APP_NAME source directory (missing: tedit.cpp)."

MAKE_BIN="$(choose_make)"
CXX_BIN="$(choose_cxx)"
PC_BIN="$(choose_pc)"

info "Detected:"
say "  make: ${MAKE_BIN}"
say "  c++ : ${CXX_BIN:-none}"
say "  pkg : ${PKG:-none}"
say "  pc  : ${PC_BIN:-none}"
say "  prefix: ${TARGET_PREFIX}"

auth_once

# Nix/Guix: ephemeral env build
if [ "$PKG" = "nix" ]; then
  [ "$INSTALL_DEPS" -eq 1 ] || die "Nix detected but --no-deps set. Use nix-shell manually."
  info "Building in nix-shell..."
  spinner "nix-shell: build + install" nix-shell -p gnumake gcc pkg-config lua --run \
    "make && make PREFIX='$TARGET_PREFIX' install"
  ok "$APP_NAME installed to: $TARGET_PREFIX/bin/$APP_NAME"
  exit 0
fi

if [ "$PKG" = "guix" ]; then
  [ "$INSTALL_DEPS" -eq 1 ] || die "Guix detected but --no-deps set. Use guix shell manually."
  info "Building in guix shell..."
  spinner "guix: build + install" guix shell gnu-make gcc-toolchain pkg-config lua -- \
    make && make PREFIX="$TARGET_PREFIX" install
  ok "$APP_NAME installed to: $TARGET_PREFIX/bin/$APP_NAME"
  exit 0
fi

# Deps
if deps_missing "$MAKE_BIN" "$CXX_BIN" "$PC_BIN"; then
  if [ "$INSTALL_DEPS" -eq 1 ]; then
    install_deps
  else
    die "Missing deps and --no-deps set. Install: make, C++17 compiler, and Lua dev headers/libs."
  fi
fi

if [ "$DEPS_ONLY" -eq 1 ]; then
  ok "Dependencies installed."
  exit 0
fi

# Re-check after deps install
MAKE_BIN="$(choose_make)"
CXX_BIN="$(choose_cxx)"
PC_BIN="$(choose_pc)"
deps_missing "$MAKE_BIN" "$CXX_BIN" "$PC_BIN" && die "Deps still missing after install. Check the log."

if [ -n "$LUA_PCNAME" ]; then
  ok "Lua dev found via pkg-config: $LUA_PCNAME"
else
  ok "Lua dev found (fallback detection)"
fi

# Build (pass detected Lua flags into make vars; your Makefile supports overriding LUA_CFLAGS/LUA_LIBS)
info "Building..."
spinner "make" "$MAKE_BIN" CXX="$CXX_BIN" LUA_CFLAGS="$LUA_CFLAGS" LUA_LIBS="$LUA_LIBS" || die "Build failed."

# Install
info "Installing..."
if [ "$(id -u)" -eq 0 ] || [ "$TARGET_PREFIX" = "$USER_PREFIX" ] || [ -z "$SUDO" ]; then
  spinner "make install" "$MAKE_BIN" PREFIX="$TARGET_PREFIX" install || die "Install failed."
else
  run_root "make install" "$MAKE_BIN" PREFIX="$TARGET_PREFIX" install || die "Install failed."
fi

# Strip
if [ "$DO_STRIP" -eq 1 ] && have strip; then
  BIN="${TARGET_PREFIX}/bin/${APP_NAME}"
  if [ -e "$BIN" ]; then
    if [ "$(id -u)" -eq 0 ] || [ "$TARGET_PREFIX" = "$USER_PREFIX" ] || [ -z "$SUDO" ]; then
      spinner "strip" strip "$BIN" || true
    else
      run_root "strip" strip "$BIN" || true
    fi
  fi
fi

# Man page
if [ "$INSTALL_MAN" -eq 1 ]; then
  if [ -f "./${APP_NAME}.1" ]; then
    MAN_DIR="${TARGET_PREFIX}/share/man/man1"
    if [ "$(id -u)" -eq 0 ] || [ -z "$SUDO" ] || [ "$TARGET_PREFIX" = "$USER_PREFIX" ]; then
      spinner "man page" sh -c "mkdir -p '$MAN_DIR' && install -m 0644 './${APP_NAME}.1' '$MAN_DIR/${APP_NAME}.1'" || true
      have gzip && spinner "gzip man" gzip -f -9 "$MAN_DIR/${APP_NAME}.1" || true
    else
      run_root "man page" sh -c "mkdir -p '$MAN_DIR' && install -m 0644 './${APP_NAME}.1' '$MAN_DIR/${APP_NAME}.1'" || true
      have gzip && run_root "gzip man" gzip -f -9 "$MAN_DIR/${APP_NAME}.1" || true
    fi
  else
    warn "No man page found, skipping."
  fi
fi

# PATH update (only if user prefix bin isn't already in PATH)
if [ "$UPDATE_PATH" -eq 1 ]; then
  BIN_DIR="${TARGET_PREFIX}/bin"
  if ! printf "%s" ":$PATH:" | grep -q ":$BIN_DIR:"; then
    PROFILE=""
    [ -n "${ZDOTDIR-}" ] && [ -w "${ZDOTDIR}/.zprofile" 2>/dev/null ] && PROFILE="${ZDOTDIR}/.zprofile"
    [ -z "$PROFILE" ] && [ -w "$HOME/.zprofile" 2>/dev/null ] && PROFILE="$HOME/.zprofile"
    [ -z "$PROFILE" ] && [ -w "$HOME/.bash_profile" 2>/dev/null ] && PROFILE="$HOME/.bash_profile"
    [ -z "$PROFILE" ] && PROFILE="$HOME/.profile"

    mkdir -p "$(dirname "$PROFILE")" 2>/dev/null || :
    if ! grep -F "$BIN_DIR" "$PROFILE" >/dev/null 2>&1; then
      {
        printf "\n# Added by %s installer\n" "$APP_NAME"
        printf "export PATH=\"%s:\$PATH\"\n" "$BIN_DIR"
      } >>"$PROFILE"
      ok "Added $BIN_DIR to PATH in $PROFILE"
      say "Restart shell or run: export PATH=\"$BIN_DIR:\$PATH\""
    fi
  fi
fi

ok "$APP_NAME installed successfully."
say "Binary: ${TARGET_PREFIX}/bin/${APP_NAME}"
say "Log: $LOG"
