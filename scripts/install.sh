#!/usr/bin/env sh
# Read the complete entry point before starting commands when used via curl | sh.
# shellcheck shell=sh
main() {
set -eu
set -f

have() { command -v "$1" >/dev/null 2>&1; }
say() { printf '%s\n' "$*"; printf '%s\n' "$*" >>"$LOG"; }
die() { say "ERROR: $*" >&2; say "Log: $LOG" >&2; exit 1; }
shell_quote() { printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"; }

usage() {
  cat <<'EOF'
tedit source installer

Usage:
  sh scripts/install.sh [options]
  curl -fsSL https://raw.githubusercontent.com/RobertFlexx/tedit/main/scripts/install.sh | sh -s -- [options]

Options:
  --prefix PATH      Install into PATH (also accepts PREFIX)
  --system           Install into /usr/local; elevate only when needed
  --user             Install into ~/.local (default for non-root users)
  --repo PATH        Build an existing checkout (also accepts TEDIT_REPO)
  --ref REF          Branch or tag to clone if no local source (default: main)
  --no-deps          Never install packages
  --deps-only        Check/install build dependencies without fetching or building
  --with-git         Also ensure git is installed (used by the updater)
  --no-build         Install an existing local build
  --no-install       Build without copying files or changing PATH
  --no-man           Skip the man page
  --no-strip         Keep binary debug symbols
  --no-path          Do not edit a shell profile
  --verbose          Show successful command output as well as errors
  -h, --help         Show help

Environment:
  PREFIX, TEDIT_REPO, TEDIT_REPO_URL, TEDIT_REF, CXX, MAKE, PKG_CONFIG,
  LUA_CFLAGS, LUA_LIBS, VERBOSE=1, LOG_FILE, TMPDIR
EOF
}

# Help and argument errors must not require a checkout or build dependencies.
LOG=/dev/null
TARGET_PREFIX=${PREFIX:-}
REPO_DIR=${TEDIT_REPO:-}
SOURCE_URL=${TEDIT_REPO_URL:-https://github.com/RobertFlexx/tedit.git}
SOURCE_REF=${TEDIT_REF:-main}
INSTALL_DEPS=1
DEPS_ONLY=0
REQUIRE_GIT=0
NO_BUILD=0
NO_INSTALL=0
INSTALL_MAN=1
DO_STRIP=1
UPDATE_PATH=1
VERBOSE=${VERBOSE:-0}
while [ "$#" -gt 0 ]; do
  case "$1" in
    --prefix|--repo|--ref)
      option=$1
      shift
      [ "$#" -gt 0 ] && [ -n "$1" ] || die "$option needs a value"
      case "$option" in
        --prefix) TARGET_PREFIX=$1 ;;
        --repo) REPO_DIR=$1 ;;
        --ref) SOURCE_REF=$1 ;;
      esac
      ;;
    --system) TARGET_PREFIX=/usr/local ;;
    --user) TARGET_PREFIX=${HOME:?HOME is required for --user}/.local ;;
    --no-deps) INSTALL_DEPS=0 ;;
    --deps-only) DEPS_ONLY=1 ;;
    --with-git) REQUIRE_GIT=1 ;;
    --no-build) NO_BUILD=1 ;;
    --no-install) NO_INSTALL=1 ;;
    --no-man) INSTALL_MAN=0 ;;
    --no-strip) DO_STRIP=0 ;;
    --no-path) UPDATE_PATH=0 ;;
    --verbose) VERBOSE=1 ;;
    -h|--help) usage; return 0 ;;
    *) die "Unknown option: $1 (use --help)" ;;
  esac
  shift
done

START_DIR=$(pwd -P)
USER_ID=$(id -u)
if [ -z "$TARGET_PREFIX" ]; then
  if [ "$USER_ID" -eq 0 ]; then TARGET_PREFIX=/usr/local
  else TARGET_PREFIX=${HOME:?HOME is required for a user install}/.local
  fi
fi
case "$TARGET_PREFIX" in /*) ;; *) TARGET_PREFIX=$START_DIR/$TARGET_PREFIX ;; esac
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/tedit-install.XXXXXX") || {
  printf '%s\n' 'ERROR: cannot create a temporary build directory' >&2
  exit 1
}
WORK_DIR=$(CDPATH= cd -P -- "$WORK_DIR" && pwd)
DEST_TEMP=
cleanup() {
  if [ -n "$DEST_TEMP" ]; then root_command rm -f -- "$DEST_TEMP" || :; fi
  rm -rf -- "$WORK_DIR"
}
trap cleanup 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
if [ -n "${LOG_FILE:-}" ]; then LOG=$LOG_FILE
else LOG=$(mktemp "${TMPDIR:-/tmp}/tedit-install.log.XXXXXX") || exit 1
fi
case "$LOG" in /*) ;; *) LOG=$START_DIR/$LOG ;; esac
: >>"$LOG" || { printf 'ERROR: cannot write log: %s\n' "$LOG" >&2; exit 1; }
say ':: Installing tedit from source'
say "Log: $LOG"
say "Prefix: $TARGET_PREFIX"

run() {
  # Never let a child consume the script's stdin, including in non-TTY runs.
  if "$@" </dev/null >"$WORK_DIR/output" 2>&1; then
    cat "$WORK_DIR/output" >>"$LOG"
    if [ "$VERBOSE" = 1 ]; then cat "$WORK_DIR/output"; fi
  else
    command_status=$?
    cat "$WORK_DIR/output" >>"$LOG"
    cat "$WORK_DIR/output" >&2
    return "$command_status"
  fi
}

ELEVATE=
AUTHENTICATED=0
authenticate() {
  [ "$USER_ID" -ne 0 ] || return 0
  if [ "$AUTHENTICATED" -eq 0 ]; then
    if have sudo; then
      ELEVATE=sudo
      say ':: Authenticating with sudo'
      # sudo reads from the terminal; do not hide authentication errors/prompts.
      sudo -v </dev/null || die 'sudo authentication failed'
    elif have doas; then ELEVATE=doas
    else die 'This step needs root. Install dependencies as root, or use a writable --prefix with --no-deps.'
    fi
    AUTHENTICATED=1
  fi
}
root_command() {
  case "$ELEVATE" in
    sudo) sudo -n "$@" </dev/null ;;
    doas) doas "$@" </dev/null ;;
    *) "$@" </dev/null ;;
  esac
}
as_root() {
  authenticate
  run root_command "$@"
}

# Prefer the native package manager over unrelated tools on PATH. ID_LIKE lists
# distro families in order of relevance; do not treat it as one literal ID.
OS=$(uname -s)
ID=
ID_LIKE=
PKG=
if [ "$OS" = Linux ]; then
  OS_RELEASE=${TEDIT_OS_RELEASE:-/etc/os-release}
  if [ -r "$OS_RELEASE" ]; then
    # shellcheck disable=SC1090
    . "$OS_RELEASE"
  elif [ -r /usr/lib/os-release ]; then
    # shellcheck disable=SC1091
    . /usr/lib/os-release
  fi
  for family in "$ID" $ID_LIKE; do
    case "$family" in
      alpine|postmarketos|chimera) PKG=apk ;;
      debian|ubuntu) PKG=apt-get ;;
      fedora|rhel|centos|rocky|almalinux|amzn|ol)
        if have dnf; then PKG=dnf; else PKG=yum; fi ;;
      arch|manjaro|artix) PKG=pacman ;;
      opensuse*|suse|sles) PKG=zypper ;;
      void) PKG=xbps-install ;;
      solus) PKG=eopkg ;;
      gentoo) PKG=emerge ;;
      nixos) PKG=nix ;;
      guix) PKG=guix ;;
    esac
    [ -z "$PKG" ] || break
  done
else
  case "$OS" in
    Darwin) if have brew; then PKG=brew; else PKG=port; fi ;;
    FreeBSD|DragonFly) PKG=pkg ;;
    OpenBSD) PKG=pkg_add ;;
    NetBSD) PKG=pkgin ;;
  esac
fi
if [ -z "$PKG" ]; then
  for candidate in apt-get dnf yum pacman zypper apk xbps-install eopkg emerge brew port; do
    if have "$candidate"; then PKG=$candidate; break; fi
  done
fi
say "Package manager: ${PKG:-not detected} (${ID:-$OS})"

source_exists() { [ -f "$1/Makefile" ] && [ -f "$1/src/tedit.cpp" ]; }
NEED_SOURCE=0
if [ "$DEPS_ONLY" -eq 0 ]; then
  if [ -n "$REPO_DIR" ]; then
    source_exists "$REPO_DIR" || die "Not a tedit source directory: $REPO_DIR"
  else
    # A local script builds its own checkout even when called from elsewhere.
    if [ -f "$0" ]; then
      SCRIPT_DIR=$(CDPATH= cd -P -- "$(dirname -- "$0")" && pwd)
      if source_exists "$SCRIPT_DIR/.."; then REPO_DIR=$SCRIPT_DIR/..; fi
    fi
    if [ -z "$REPO_DIR" ] && source_exists "$START_DIR"; then REPO_DIR=$START_DIR; fi
    if [ -z "$REPO_DIR" ]; then NEED_SOURCE=1; fi
  fi
fi

if [ "$NO_BUILD" -eq 1 ] && [ "$NEED_SOURCE" -eq 1 ]; then
  die '--no-build requires an existing local source checkout and binary.'
fi

cat >"$WORK_DIR/cxx.cpp" <<'EOF'
#include <filesystem>
#include <optional>
int main() {
  std::optional<std::filesystem::path> p = std::filesystem::current_path();
  return p->empty();
}
EOF
cat >"$WORK_DIR/lua.cpp" <<'EOF'
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}
int main() {
  lua_State *L = luaL_newstate();
  luaL_openlibs(L);
  int rc = luaL_loadstring(L, "return 1");
  lua_close(L);
  return rc == LUA_OK ? 0 : 1;
}
EOF

REQUESTED_CXX=${CXX:-}
REQUESTED_MAKE=${MAKE:-}
REQUESTED_PC=${PKG_CONFIG:-}
REQUESTED_LUA_CFLAGS=${LUA_CFLAGS:-}
REQUESTED_LUA_LIBS=${LUA_LIBS:-}
CXX_BIN=
MAKE_BIN=
PC_BIN=
FS_LIBS=
LUA_CFLAGS=
LUA_LIBS=

probe_cxx() {
  for FS_LIBS in '' -lstdc++fs; do
    # Intentional splitting of conventional compiler/linker flag lists.
    # shellcheck disable=SC2086
    if "$1" ${CPPFLAGS:-} ${CXXFLAGS:-} -std=c++17 "$WORK_DIR/cxx.cpp" \
      ${LDFLAGS:-} $FS_LIBS -o "$WORK_DIR/cxx" >>"$LOG" 2>&1 </dev/null; then return 0; fi
  done
  return 1
}

probe_lua() {
  # shellcheck disable=SC2086
  "$CXX_BIN" ${CPPFLAGS:-} ${CXXFLAGS:-} -std=c++17 $1 "$WORK_DIR/lua.cpp" \
    ${LDFLAGS:-} -o "$WORK_DIR/lua" $2 >>"$LOG" 2>&1 </dev/null
}

detect_lua() {
  LUA_CFLAGS=$REQUESTED_LUA_CFLAGS
  LUA_LIBS=$REQUESTED_LUA_LIBS
  if [ -n "$LUA_CFLAGS$LUA_LIBS" ]; then probe_lua "$LUA_CFLAGS" "$LUA_LIBS"; return; fi
  if [ -n "$PC_BIN" ]; then
    for module in lua5.5 lua-5.5 lua55 lua5.4 lua-5.4 lua54 \
      lua5.3 lua-5.3 lua53 lua5.2 lua-5.2 lua52 lua luajit; do
      "$PC_BIN" --exists "$module" 2>/dev/null || continue
      LUA_CFLAGS=$("$PC_BIN" --cflags "$module") || continue
      LUA_LIBS=$("$PC_BIN" --libs "$module") || continue
      if probe_lua "$LUA_CFLAGS" "$LUA_LIBS"; then return 0; fi
    done
  fi
  # Solus uses /usr/include/lua.h. BSD/macOS may use versioned directories or
  # prefixes outside the compiler's default header/library search paths.
  for lua_prefix in /usr /usr/local /opt/local /opt/homebrew /home/linuxbrew/.linuxbrew; do
    for lua_suffix in '' /lua5.5 /lua-5.5 /lua55 /lua5.4 /lua-5.4 /lua54 \
      /lua5.3 /lua-5.3 /lua53 /lua5.2 /lua-5.2 /lua52 /luajit-2.1 /luajit-2.0; do
      lua_include=$lua_prefix/include$lua_suffix
      [ -f "$lua_include/lua.h" ] || continue
      LUA_CFLAGS=-I$lua_include
      for lua_library in lua5.5 lua55 lua5.4 lua54 lua5.3 lua53 lua5.2 lua52 lua luajit-5.1; do
        for lua_extra in '' '-lm -ldl'; do
          LUA_LIBS="-L$lua_prefix/lib -l$lua_library $lua_extra"
          if probe_lua "$LUA_CFLAGS" "$LUA_LIBS"; then return 0; fi
        done
      done
    done
  done
  LUA_CFLAGS=
  LUA_LIBS=
  return 1
}

check_build_deps() {
  MAKE_BIN=
  CXX_BIN=
  PC_BIN=
  if [ -n "$REQUESTED_MAKE" ]; then set -- "$REQUESTED_MAKE"; else set -- gmake make; fi
  for candidate do
    if have "$candidate" && "$candidate" --version 2>/dev/null | grep -q 'GNU Make'; then
      MAKE_BIN=$candidate
      break
    fi
  done
  if [ -n "$REQUESTED_CXX" ]; then set -- "$REQUESTED_CXX"; else set -- c++ g++ clang++; fi
  for candidate do
    if have "$candidate" && probe_cxx "$candidate"; then CXX_BIN=$candidate; break; fi
  done
  if [ -n "$REQUESTED_PC" ]; then set -- "$REQUESTED_PC"; else set -- pkg-config pkgconf; fi
  for candidate do
    if have "$candidate"; then PC_BIN=$candidate; break; fi
  done
  [ -n "$MAKE_BIN" ] && [ -n "$CXX_BIN" ] && detect_lua
}

install_deps() {
  say ":: Installing dependencies with ${PKG:-unknown}"
  case "$PKG" in
    nix) die 'Enter nix-shell -p gnumake gcc pkg-config lua git, then rerun with --no-deps.' ;;
    guix) die 'Enter guix shell make gcc-toolchain pkg-config lua git, then rerun with --no-deps.' ;;
    '') die 'Install GNU make, a C++17 compiler, Lua development headers/libraries, and git; then rerun with --no-deps.' ;;
  esac
  have "$PKG" || die "Detected $PKG, but it is not installed. Install build dependencies manually and use --no-deps."
  set --
  case "$PKG" in
    apt-get)
      as_root apt-get update || return $?
      if [ "$NEED_BUILD" -eq 1 ]; then
        lua_dev=liblua5.4-dev
        if have apt-cache; then
          lua_dev=
          for candidate in liblua5.4-dev liblua5.3-dev liblua5.2-dev; do
            if apt-cache show "$candidate" >/dev/null 2>&1; then lua_dev=$candidate; break; fi
          done
          [ -n "$lua_dev" ] || die 'No supported Lua development package is available in the configured apt repositories.'
        fi
        set -- build-essential pkg-config "$lua_dev"
      fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca-certificates; fi
      as_root apt-get install -y --no-install-recommends "$@"
      ;;
    dnf|yum)
      if [ "$NEED_BUILD" -eq 1 ]; then set -- make gcc-c++ pkgconf-pkg-config lua-devel; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca-certificates; fi
      as_root "$PKG" install -y "$@"
      ;;
    pacman)
      if [ "$NEED_BUILD" -eq 1 ]; then set -- base-devel pkgconf lua; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca-certificates; fi
      # Refreshing with -Sy alone can cause partial upgrades on Arch.
      as_root pacman -S --needed --noconfirm "$@"
      ;;
    zypper)
      if [ "$NEED_BUILD" -eq 1 ]; then set -- make gcc-c++ pkgconf-pkg-config lua54-devel; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca-certificates; fi
      as_root zypper --non-interactive install "$@"
      ;;
    apk)
      if [ "$NEED_BUILD" -eq 1 ]; then
        if [ "$ID" = chimera ]; then set -- base-devel gmake pkgconf lua5.4-devel
        else set -- build-base pkgconf lua5.4-dev
        fi
      fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca-certificates; fi
      as_root apk add --no-cache "$@"
      ;;
    xbps-install)
      if [ "$NEED_BUILD" -eq 1 ]; then set -- base-devel pkgconf lua54-devel; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca-certificates; fi
      as_root xbps-install -Sy "$@"
      ;;
    eopkg)
      if [ "$NEED_BUILD" -eq 1 ]; then
        as_root eopkg install -y -c system.devel || return $?
        set -- pkgconf lua-devel
      fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca-certs; fi
      as_root eopkg install -y "$@"
      ;;
    emerge)
      say ':: Gentoo: emerging build dependencies may take a while.'
      if [ "$NEED_BUILD" -eq 1 ]; then set -- dev-build/make sys-devel/gcc dev-util/pkgconf dev-lang/lua:5.4; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" dev-vcs/git app-misc/ca-certificates; fi
      as_root emerge --oneshot --noreplace --quiet-build=y "$@"
      ;;
    pkg)
      if [ "$NEED_BUILD" -eq 1 ]; then set -- gmake pkgconf lua54; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca_root_nss; fi
      as_root pkg install -y "$@"
      ;;
    pkg_add)
      if [ "$NEED_BUILD" -eq 1 ]; then set -- gmake pkgconf 'lua%5.4'; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git; fi
      as_root pkg_add "$@"
      ;;
    pkgin)
      if [ "$NEED_BUILD" -eq 1 ]; then set -- gmake pkgconf lua54; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git mozilla-rootcerts; fi
      as_root pkgin -y update || return $?
      as_root pkgin -y install "$@"
      ;;
    brew)
      [ "$USER_ID" -ne 0 ] || die 'Homebrew must run as your regular user. Rerun without sudo.'
      if [ "$OS" = Darwin ] && ! xcode-select -p >/dev/null 2>&1; then
        die 'Install Xcode Command Line Tools with xcode-select --install, then rerun.'
      fi
      if [ "$NEED_BUILD" -eq 1 ]; then set -- make pkgconf lua; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git ca-certificates; fi
      run brew install "$@"
      ;;
    port)
      if [ "$NEED_BUILD" -eq 1 ]; then set -- gmake pkgconfig lua54; fi
      if [ "$NEED_GIT" -eq 1 ]; then set -- "$@" git curl-ca-bundle; fi
      as_root port install "$@"
      ;;
  esac
}

NEED_BUILD=0
NEED_GIT=0
if [ "$NO_BUILD" -eq 0 ] || [ "$DEPS_ONLY" -eq 1 ]; then
  check_build_deps || NEED_BUILD=1
fi
if { [ "$NEED_SOURCE" -eq 1 ] || [ "$REQUIRE_GIT" -eq 1 ]; } && ! have git; then NEED_GIT=1; fi
if [ "$NEED_BUILD" -eq 1 ] || [ "$NEED_GIT" -eq 1 ]; then
  [ "$INSTALL_DEPS" -eq 1 ] || die 'Missing build dependencies or git with --no-deps set. Need GNU make, a C++17 compiler with filesystem support, and Lua development headers/libraries. See the log for compiler errors.'
  install_deps || die 'Dependency installation failed. Fix the package-manager error above and rerun.'
  hash -r 2>/dev/null || :
  check_build_deps || die 'Build dependencies are still unusable. Need GNU make, C++17 filesystem support, and Lua development headers/libraries. See the log; CXX and LUA_CFLAGS/LUA_LIBS can override detection.'
  if [ "$NEED_GIT" -eq 1 ]; then have git || die 'git is still unavailable after installing dependencies'; fi
fi
if [ "$DEPS_ONLY" -eq 1 ]; then say 'Dependencies verified.'; return 0; fi

if [ "$NEED_SOURCE" -eq 1 ]; then
  REPO_DIR=$WORK_DIR/source
  say ":: Cloning $SOURCE_URL ($SOURCE_REF)"
  export GIT_TERMINAL_PROMPT=0
  run git clone --depth 1 --branch "$SOURCE_REF" -- "$SOURCE_URL" "$REPO_DIR" || die 'Could not fetch tedit source.'
fi
source_exists "$REPO_DIR" || die "Not a tedit source directory: $REPO_DIR"
REPO_DIR=$(CDPATH= cd -P -- "$REPO_DIR" && pwd)
cd "$REPO_DIR"
# Force a source rebuild even if local objects use a different compiler/Lua ABI.
if [ "$NO_BUILD" -eq 0 ]; then
  say ":: Building with $MAKE_BIN and $CXX_BIN"
  run "$MAKE_BIN" -B CXX="$CXX_BIN" LUA_CFLAGS="$LUA_CFLAGS" LUA_LIBS="$LUA_LIBS" \
    LDFLAGS="${LDFLAGS:-} $FS_LIBS" all || die 'Build failed.'
fi
[ -x "$REPO_DIR/tedit" ] || die 'Build did not produce an executable tedit binary.'
if [ "$NO_INSTALL" -eq 1 ]; then say "Build ready: $REPO_DIR/tedit"; return 0; fi
if [ "$DO_STRIP" -eq 1 ] && have strip; then
  run strip "$REPO_DIR/tedit" || say 'Warning: could not strip the binary; installing with debug symbols.'
fi

# Elevate file copies only when the target's nearest existing parent is not
# writable. A custom user prefix must not trigger sudo just because it is custom.
prefix_probe=$TARGET_PREFIX
while [ ! -e "$prefix_probe" ]; do prefix_probe=$(dirname -- "$prefix_probe"); done
INSTALL_AS_ROOT=0
if [ "$USER_ID" -ne 0 ] && [ ! -w "$prefix_probe" ]; then INSTALL_AS_ROOT=1; fi
install_file() {
  if [ "$INSTALL_AS_ROOT" -eq 1 ]; then as_root "$@"
  else run "$@"
  fi
}
place_file() {
  place_source=$1
  place_destination=$2
  place_mode=$3
  [ ! -d "$place_destination" ] || die "Refusing to replace a directory: $place_destination"
  place_dir=$(dirname -- "$place_destination")
  install_file mkdir -p "$place_dir" || return $?
  if [ "$INSTALL_AS_ROOT" -eq 1 ]; then
    authenticate
    DEST_TEMP=$(root_command mktemp "$place_dir/.tedit.XXXXXX") || return $?
  else DEST_TEMP=$(mktemp "$place_dir/.tedit.XXXXXX") || return $?
  fi
  install_file install -m "$place_mode" "$place_source" "$DEST_TEMP" || return $?
  install_file mv -f -- "$DEST_TEMP" "$place_destination" || return $?
  DEST_TEMP=
}
say ":: Installing into $TARGET_PREFIX"
place_file "$REPO_DIR/tedit" "$TARGET_PREFIX/bin/tedit" 0755 || die 'Could not install the binary.'
if [ "$INSTALL_MAN" -eq 1 ]; then
  if [ -f "$REPO_DIR/mandoc/tedit.1" ]; then
    place_file "$REPO_DIR/mandoc/tedit.1" "$TARGET_PREFIX/share/man/man1/tedit.1" 0644 || die 'Could not install the manual.'
    # Older installers compressed this page; remove that stale duplicate.
    install_file rm -f "$TARGET_PREFIX/share/man/man1/tedit.1.gz" || die 'Could not remove the old compressed manual.'
  else say 'Warning: no man page found in the source checkout.'
  fi
fi

# Only a successful build of a clean Git revision can satisfy the updater's
# up-to-date check. A failed build/install must leave the old receipt intact.
build_revision=unverified
if [ "$NO_BUILD" -eq 0 ] && have git && { [ -d .git ] || [ -f .git ]; }; then
  if revision=$(git rev-parse HEAD 2>/dev/null) && changes=$(git status --porcelain 2>/dev/null) && [ -z "$changes" ]; then
    build_revision=$revision
  fi
fi
printf '%s\n' "$build_revision" >"$WORK_DIR/revision"
place_file "$WORK_DIR/revision" "$TARGET_PREFIX/share/tedit/build-revision" 0644 || die 'Could not record the installed revision.'

BIN_DIR=$TARGET_PREFIX/bin
case ":$PATH:" in
  *":$BIN_DIR:"*) ;;
  *)
    PATH_LINE="export PATH=$(shell_quote "$BIN_DIR"):\"\$PATH\""
    if [ "$UPDATE_PATH" -eq 1 ] && [ "$USER_ID" -ne 0 ] && [ -n "${HOME:-}" ]; then
      case "${SHELL:-}" in
        */zsh) PROFILE=${ZDOTDIR:-$HOME}/.zprofile ;;
        */bash)
          if [ -f "$HOME/.bash_profile" ]; then PROFILE=$HOME/.bash_profile
          elif [ -f "$HOME/.bash_login" ]; then PROFILE=$HOME/.bash_login
          else PROFILE=$HOME/.profile
          fi ;;
        */fish) PROFILE= ;;
        *) PROFILE=$HOME/.profile ;;
      esac
      if [ -n "$PROFILE" ]; then
        if ! grep -Fqx "$PATH_LINE" "$PROFILE" 2>/dev/null; then
          if mkdir -p "$(dirname -- "$PROFILE")" && printf '\n# Added by tedit installer\n%s\n' "$PATH_LINE" >>"$PROFILE"; then
            say "Added $BIN_DIR to PATH in $PROFILE"
          else say "Warning: could not update $PROFILE; add $BIN_DIR to PATH manually."
          fi
        fi
      fi
    fi
    case "${SHELL:-}" in
      */fish) say "Add to PATH with: fish_add_path $(shell_quote "$BIN_DIR")" ;;
      *) say "Add to PATH for this shell: $PATH_LINE" ;;
    esac
    ;;
esac
say "tedit installed successfully: $TARGET_PREFIX/bin/tedit"
}

main "$@"
