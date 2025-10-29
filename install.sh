#!/usr/bin/env sh
# tedit installer (POSIX sh) — cinematic bar + spinner, no prompt glitches

set -eu

APP_NAME="tedit"
DEFAULT_PREFIX="/usr/local"
USER_PREFIX="${PREFIX:-$HOME/.local}"
LOG="$(mktemp -t ${APP_NAME}-install.XXXXXX.log)"

# --- Enter script dir (wrappers/symlinks safe) ---
SCRIPT_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)
cd "$SCRIPT_DIR" || { echo "ERROR: cannot cd to $SCRIPT_DIR"; exit 2; }

# --- Colors ---
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
  GREEN="$(tput setaf 2)"; YELLOW="$(tput setaf 3)"; RED="$(tput setaf 1)"; CYAN="$(tput setaf 6)"
  BOLD="$(tput bold)"; RESET="$(tput sgr0)"
else
  GREEN=""; YELLOW=""; RED=""; CYAN=""; BOLD=""; RESET=""
fi

say() { printf "%s\n" "$*" | tee -a "$LOG" >/dev/null; }
die(){ finish_ui 2>/dev/null || :; printf "%sERROR:%s %s\n" "$RED" "$RESET" "$*" | tee -a "$LOG" >&2; exit 1; }
have(){ command -v "$1" >/dev/null 2>&1; }

# --- Privileges ---
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if have sudo; then SUDO="sudo"
  elif have doas; then SUDO="doas"
  fi
fi

# --- UI: progress + spinner (spinner NEVER used when a prompt may appear) ---
is_utf(){ echo "${LC_ALL:-${LANG:-}}" | grep -qi 'utf-8'; }
repeat(){ n=$1; ch=$2; i=0; out=""; while [ "$i" -lt "$n" ]; do out="$out$ch"; i=$((i+1)); done; printf "%s" "$out"; }
term_width(){ w=80; if command -v tput >/dev/null 2>&1; then w=$(tput cols 2>/dev/null || echo 80); fi; [ "$w" -gt 24 ] || w=80; echo "$w"; }

[ -f "tedit.cpp" ] || die "Please run this from the ${APP_NAME} source directory."

if is_utf; then FIL="█"; EMP="░"; else FIL="#"; EMP="-"; fi
TOTAL=12 STEP=0

draw_bar(){
  n=$1; tot=$2; msg=$3
  tw=$(term_width); bw=$(( tw - 32 )); [ "$bw" -lt 12 ] && bw=12; [ "$bw" -gt 60 ] && bw=60
  [ "$tot" -gt 0 ] || tot=1
  pct=$(( n*100 / tot )); fill=$(( n*bw / tot )); empty=$(( bw - fill ))
  bar="$(repeat "$fill" "$FIL")$(repeat "$empty" "$EMP")"
  printf "\r\033[K%s%s[%s]%s %d/%d (%d%%) %s" "$CYAN" "$BOLD" "$bar" "$RESET" "$n" "$tot" "$pct" "$msg"
  printf "[%s] %d/%d (%d%%) %s\n" "$(repeat "$fill" "#")$(repeat "$empty" "-")" "$n" "$tot" "$pct" "$msg" >>"$LOG"
}
next(){ STEP=$((STEP+1)); [ "$STEP" -gt "$TOTAL" ] && STEP="$TOTAL"; draw_bar "$STEP" "$TOTAL" "$1"; }

spinner(){
  msg="$1"; shift
  if [ "${VERBOSE-0}" = "1" ] || [ ! -t 1 ]; then
    sh -c "$*" 2>&1 | tee -a "$LOG"
    return $?
  fi
  sh -c "$*" >>"$LOG" 2>&1 &
  pid=$!
  frames='-\|/.'; i=0
  while kill -0 "$pid" 2>/dev/null; do
    i=$(( (i+1) % 4 )); c=$(printf %s "$frames" | cut -c $((i+1)))
    printf "\r\033[K[%s] %s" "$c" "$msg"
    sleep 0.1
  done
  wait "$pid" 2>/dev/null || true
  printf "\r\033[K"
}

# Pre-auth in FOREGROUND (clean line; no spinner), so prompts don’t glitch.
auth_once(){
  [ -z "$SUDO" ] && return 0
  printf "\r\033[K%s%sElevating privileges (may prompt once)...%s\n" "$CYAN" "$BOLD" "$RESET"
  if [ "$SUDO" = "sudo" ]; then sudo -v
  else $SUDO true
  fi
}

# Root runner: try non-interactive first; if auth needed, run FOREGROUND w/ prompt.
run_root(){
  msg="$1"; cmd="$2"
  if [ -z "$SUDO" ]; then
    spinner "$msg" "$cmd"
    return
  fi
  if $SUDO -n true 2>/dev/null; then
    spinner "$msg" "$SUDO $cmd"
  else
    # clear spinner/progress line and prompt cleanly
    printf "\r\033[K%s%s%s %s\n" "$YELLOW" "[auth]" "$RESET" "$msg"
    $SUDO sh -c "$cmd" 2>&1 | tee -a "$LOG"
    # redraw last progress step to keep the bar pretty
    draw_bar "$STEP" "$TOTAL" "$msg"
  fi
}

# Hide cursor (restore on exit)
CURSOR_HIDE=""; CURSOR_SHOW=""
if command -v tput >/dev/null 2>&1; then CURSOR_HIDE="$(tput civis 2>/dev/null || true)"; CURSOR_SHOW="$(tput cnorm 2>/dev/null || true)"; fi
printf "%s" "$CURSOR_HIDE"
finish_ui(){ printf "\r\033[K\n%s" "$CURSOR_SHOW"; }
trap 'finish_ui' EXIT INT TERM

# --- Detect package manager (keep apk for Chimera/Alpine) ---
PKG=""
if [ -r /etc/os-release ]; then . /etc/os-release || true; fi
case "${ID:-}" in
  alpine|postmarketos|chimera) PKG="apk" ;;
  arch|manjaro|endeavouros|arco|artix) PKG="pacman" ;;
  debian|ubuntu|pop|elementary|linuxmint|zorin) PKG="apt" ;;
  fedora|rhel|centos|rocky|almalinux) PKG="dnf" ;;
  opensuse*|sles) PKG="zypper" ;;
  gentoo) PKG="emerge" ;;
  void) PKG="xbps-install" ;;
  solus) PKG="eopkg" ;;
esac
[ -z "$PKG" ] && { have apk && PKG="apk" || :; }
[ -z "$PKG" ] && { have apt && PKG="apt" || :; }
[ -z "$PKG" ] && { have dnf && PKG="dnf" || :; }
[ -z "$PKG" ] && { have yum && PKG="yum" || :; }
[ -z "$PKG" ] && { have pacman && PKG="pacman" || :; }
[ -z "$PKG" ] && { have zypper && PKG="zypper" || :; }
[ -z "$PKG" ] && { have xbps-install && PKG="xbps-install" || :; }
[ -z "$PKG" ] && { have eopkg && PKG="eopkg" || :; }
[ -z "$PKG" ] && { have emerge && PKG="emerge" || :; }
[ -z "$PKG" ] && { have brew && [ "$(uname -s)" = "Darwin" ] && PKG="brew" || :; }

need_deps(){
  need=0
  have make || need=1
  if have c++; then :; elif have g++; then :; elif have clang++; then :; else need=1; fi
  return $need
}

install_deps(){
  # Gentoo heads-up (colored)
  if [ "${ID:-}" = "gentoo" ] || [ "$PKG" = "emerge" ]; then
    printf "%sGentoo detected — emerging toolchain (may be interactive).%s\n" "$YELLOW" "$RESET" | tee -a "$LOG" >/dev/null
  fi
  case "$PKG" in
    apk)
      run_root "Installing build-base..." "apk add --no-cache --update-cache build-base"
      ;;
    apt)
      run_root "Updating APT index..."     "sh -c 'apt-get update -y || apt update -y'"
      run_root "Installing build tools..." "sh -c 'apt-get install -y build-essential make || apt install -y build-essential make'"
      ;;
    dnf|yum)
      run_root "Installing dev tools..."   "$PKG install -y make gcc gcc-c++"
      ;;
    pacman)
      run_root "Syncing pacman..."         "pacman -Sy --noconfirm"
      run_root "Installing gcc & make..."  "pacman -S --needed --noconfirm gcc make"
      ;;
    zypper)
      run_root "Installing C/C++ tools..." "zypper -n install make gcc-c++"
      ;;
    xbps-install)
      run_root "Installing gcc & make..."  "xbps-install -Sy gcc make"
      ;;
    eopkg)
      run_root "Installing system.devel..." "sh -c 'eopkg -y it -c system.devel || eopkg -y it make gcc binutils'"
      ;;
    emerge)
      run_root "Emerging gcc & make..."    "emerge --quiet-build=y --oneshot sys-devel/gcc sys-devel/make || true"
      ;;
    brew)
      spinner "Installing make..." "brew install make || true"
      if ! have clang++; then
        printf "%sXcode Command Line Tools may be required.%s\n" "$YELLOW" "$RESET" | tee -a "$LOG" >/dev/null
        spinner "Invoking xcode-select..." "xcode-select --install || true"
      fi
      ;;
    *)
      die "Unsupported/undetected package manager. Install make + a C++17 compiler, then re-run."
      ;;
  esac

  # verify post-install
  have make || die "make still missing after install."
  if have c++; then :; elif have g++; then :; elif have clang++; then :; else die "No C++17 compiler after install."; fi
}

choose_cxx(){
  if   have c++; then printf %s "c++"
  elif have g++; then printf %s "g++"
  elif have clang++; then printf %s "clang++"
  else printf %s "c++"
  fi
}

# --- Banner + Steps ---
printf "%s%s>> Installing %s <<%s\n" "$CYAN" "$BOLD" "$APP_NAME" "$RESET" | tee -a "$LOG" >/dev/null

next "Checking environment"

next "Preparing privileges"
auth_once
draw_bar "$STEP" "$TOTAL" "Preparing privileges"

next "Detecting package manager"
[ -n "$PKG" ] || die "No supported package manager detected."

next "Checking build dependencies"
if need_deps; then install_deps; fi

next "Selecting compiler"
CXX_BIN="$(choose_cxx)"; printf "Using %s\n" "$CXX_BIN" >>"$LOG"

next "Building (${CXX_BIN})"
spinner "make ..." "CXX='$CXX_BIN' make"

next "Installing binary"
TARGET_PREFIX="$DEFAULT_PREFIX"
INSTALL_CMD="make install"
if [ -n "$SUDO" ]; then INSTALL_CMD="$SUDO $INSTALL_CMD"; fi
if [ -z "$SUDO" ] && [ "$(id -u)" -ne 0 ]; then
  TARGET_PREFIX="$USER_PREFIX"
  INSTALL_CMD="make PREFIX='$TARGET_PREFIX' install"
  printf "%sNote:%s installing to %s/bin\n" "$YELLOW" "$RESET" "$TARGET_PREFIX" | tee -a "$LOG" >/dev/null
fi
# Use root-aware runner (no spinner if it needs a prompt)
if [ -n "$SUDO" ]; then run_root "Installing..." "${INSTALL_CMD#"$SUDO "}"
else spinner "Installing..." "$INSTALL_CMD"
fi

next "Optimizing binary"
if have strip; then
  BIN="${TARGET_PREFIX}/bin/${APP_NAME}"
  [ -e "$BIN" ] && spinner "Stripping binary..." "${SUDO:+$SUDO }strip '$BIN' 2>/dev/null || true"
fi

next "Installing man page (if present)"
if [ -f "./${APP_NAME}.1" ]; then
  MAN_DIR="${TARGET_PREFIX}/share/man/man1"
  run_root "Copying man page..." "mkdir -p '$MAN_DIR' && install -m 0644 './${APP_NAME}.1' '$MAN_DIR/${APP_NAME}.1'"
  if have gzip; then run_root "Compressing man page..." "gzip -f -9 '$MAN_DIR/${APP_NAME}.1' 2>/dev/null || true"; fi
  if have mandb; then run_root "Refreshing man database..." "mandb -q 2>/dev/null || true"
  elif have makewhatis; then run_root "Refreshing man database..." "makewhatis 2>/dev/null || true"; fi
else
  printf "No local man page; skipping.\n" >>"$LOG"
fi

next "Checking PATH"
BIN_DIR="${TARGET_PREFIX}/bin"
if ! printf "%s" ":$PATH:" | grep -q ":$BIN_DIR:"; then
  PROFILE=""
  [ -n "${ZDOTDIR-}" ] && [ -w "${ZDOTDIR}/.zprofile" 2>/dev/null ] && PROFILE="${ZDOTDIR}/.zprofile"
  [ -z "$PROFILE" ] && [ -w "$HOME/.zprofile" 2>/dev/null ] && PROFILE="$HOME/.zprofile"
  [ -z "$PROFILE" ] && [ -w "$HOME/.bash_profile" 2>/dev/null ] && PROFILE="$HOME/.bash_profile"
  [ -z "$PROFILE" ] && PROFILE="$HOME/.profile"
  mkdir -p "$(dirname "$PROFILE")" 2>/dev/null || :
  if ! grep -F "$BIN_DIR" "$PROFILE" >/dev/null 2>&1; then
    printf '\n# Added by %s installer\nexport PATH="%s:$PATH"\n' "$APP_NAME" "$BIN_DIR" >> "$PROFILE"
    say "Added ${BIN_DIR} to PATH in ${PROFILE}."
    say "Restart shell or run: ${BOLD}export PATH=\"${BIN_DIR}:\$PATH\"${RESET}"
  fi
fi

next "Done"
finish_ui
printf "%s%s✅ %s installed successfully.%s\n" "$GREEN" "$BOLD" "$APP_NAME" "$RESET"
printf "Log: %s\n" "$LOG"
