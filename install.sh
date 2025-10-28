#!/usr/bin/env sh
# tedit installer (POSIX sh)
# - Installs build deps if missing (apt/dnf/yum/pacman/zypper/apk/xbps/eopkg/emerge/brew)
# - Builds and installs tedit
# - Installs man page (tedit.1) if present
# - Uses sudo or doas if available; else installs to ~/.local/bin
# - Adds to PATH if needed
# Usage:
#   sh install.sh
#   VERBOSE=1 sh install.sh
#   PREFIX=/custom sh install.sh

set -eu

APP_NAME="tedit"
DEFAULT_PREFIX="/usr/local"
USER_PREFIX="${PREFIX:-$HOME/.local}"
LOG="$(mktemp -t ${APP_NAME}-install.XXXXXX.log)"

# --------- Colors ---------
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
  GREEN="$(tput setaf 2)"; YELLOW="$(tput setaf 3)"
  RED="$(tput setaf 1)"; CYAN="$(tput setaf 6)"
  BOLD="$(tput bold)"; RESET="$(tput sgr0)"
else
  GREEN=""; YELLOW=""; RED=""; CYAN=""; BOLD=""; RESET=""
fi

say() { printf "%s\n" "$*" | tee -a "$LOG"; }
die() { printf "%sERROR:%s %s\n" "$RED" "$RESET" "$*" | tee -a "$LOG" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

# --------- Privilege helper ---------
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if have sudo; then SUDO="sudo"
  elif have doas; then SUDO="doas"
  fi
fi

# --------- Package manager detection ---------
PKG=""
for p in apt dnf yum pacman zypper apk xbps-install eopkg emerge brew; do
  command -v "$p" >/dev/null 2>&1 && PKG="$p" && break
done

# --------- Progress bar (fixed 8 stages, capped at 100%) ---------
TOTAL_STEPS=8
STEP=0
BAR_WIDTH=36

progress() {
  STEP=$(( STEP + 1 ))
  [ $STEP -gt $TOTAL_STEPS ] && STEP=$TOTAL_STEPS
  pct=$(( 100 * STEP / TOTAL_STEPS ))
  filled=$(( BAR_WIDTH * STEP / TOTAL_STEPS ))
  unfilled=$(( BAR_WIDTH - filled ))
  printf "\r[%s%s] %3d%%  %s" \
    "$(printf '%0.s#' $(seq 1 $filled))" \
    "$(printf '%0.s ' $(seq 1 $unfilled))" \
    "$pct" "$1"
}

# --------- Spinner (for long-running commands) ---------
spinner() {
  msg="$1"; shift
  # Background the command, log output
  if [ "${VERBOSE-0}" = "1" ]; then
    sh -c "$*" 2>&1 | tee -a "$LOG" &
  else
    sh -c "$*" >>"$LOG" 2>&1 &
  fi
  pid=$!
  chars='|/-\'
  i=1
  while kill -0 "$pid" 2>/dev/null; do
    c=$(printf %s "$chars" | cut -c $i)
    printf "\r[%s] %s" "$c" "$msg"
    i=$(( (i % 4) + 1 ))
    sleep 0.1
  done
  wait "$pid" 2>/dev/null || true
  printf "\r\033[2K"
}

# --------- Dependency checks ---------
need_deps() {
  need=0
  have make || need=1
  if have c++; then :; elif have g++; then :; elif have clang++; then :; else need=1; fi
  return $need
}

install_deps() {
  case "$PKG" in
    apt)
      spinner "Updating APT index..."    "${SUDO:+$SUDO }apt-get update -y || ${SUDO:+$SUDO }apt update -y"
      spinner "Installing build tools..." "${SUDO:+$SUDO }apt-get install -y build-essential || ${SUDO:+$SUDO }apt install -y build-essential"
      ;;
    dnf|yum)
      spinner "Installing dev tools..."  "${SUDO:+$SUDO }$PKG -y groupinstall 'Development Tools' || ${SUDO:+$SUDO }$PKG install -y make gcc-c++"
      ;;
    pacman)
      spinner "Syncing pacman..."        "${SUDO:+$SUDO }pacman -Sy --noconfirm"
      spinner "Installing gcc & make..." "${SUDO:+$SUDO }pacman -S --needed --noconfirm gcc make"
      ;;
    zypper)
      spinner "Installing C/C++ tools..." "${SUDO:+$SUDO }zypper -n install -t pattern devel_C_C++ || ${SUDO:+$SUDO }zypper -n install make gcc-c++"
      ;;
    apk)
      spinner "Installing build-base..." "${SUDO:+$SUDO }apk add --no-cache build-base"
      ;;
    xbps-install)
      spinner "Installing gcc & make..." "${SUDO:+$SUDO }xbps-install -Sy gcc make"
      ;;
    eopkg) # Solus
      spinner "Installing system.devel..." "${SUDO:+$SUDO }eopkg -y it -c system.devel || ${SUDO:+$SUDO }eopkg -y it make gcc binutils"
      ;;
    emerge) # Gentoo (may be interactive)
      say "${YELLOW}Gentoo detected — emerging toolchain (may be interactive).${RESET}"
      spinner "Emerging gcc/make..."     "${SUDO:+$SUDO }emerge --quiet-build=y --oneshot sys-devel/gcc sys-devel/make || true"
      ;;
    brew)
      spinner "Installing make (brew)..." "brew install make || true"
      if ! have clang++; then
        say "${YELLOW}Xcode Command Line Tools required; a dialog may appear.${RESET}"
        spinner "Invoking xcode-select..." "xcode-select --install || true"
      fi
      ;;
    *)
      die "Unsupported/undetected package manager. Install make + a C++17 compiler manually."
      ;;
  esac

  # Re-check
  have make || die "make still missing after install."
  if have c++; then :; elif have g++; then :; elif have clang++; then :; else die "No C++17 compiler after install."; fi
}

choose_cxx() {
  if have c++; then printf %s "c++"
  elif have g++; then printf %s "g++"
  elif have clang++; then printf %s "clang++"
  else printf %s "c++"
  fi
}

# --------- Begin ---------
say "${CYAN}${BOLD}Installing ${APP_NAME}...${RESET}"

progress "Checking environment..."
[ -f "tedit.cpp" ] || die "Please run this from the ${APP_NAME} source directory."
sleep 0.1

progress "Detecting package manager..."
# (PKG already detected above)
sleep 0.1

progress "Checking build dependencies..."
if need_deps; then
  [ -z "$PKG" ] && die "No package manager found. Install make + a C++17 compiler, then re-run."
  install_deps
fi
sleep 0.1

progress "Selecting compiler..."
CXX_BIN="$(choose_cxx)"
# Don't print a separate inline message that garbles the bar; show it in the next step.
sleep 0.1

progress "Building (${CXX_BIN})..."
spinner "Running make..." "CXX='$CXX_BIN' make"

progress "Installing binary..."
TARGET_PREFIX="$DEFAULT_PREFIX"
INSTALL_CMD="${SUDO:+$SUDO }make install"
if [ -z "$SUDO" ] && [ "$(id -u)" -ne 0 ]; then
  TARGET_PREFIX="$USER_PREFIX"
  INSTALL_CMD="make PREFIX='$TARGET_PREFIX' install"
  say "${YELLOW}No sudo/doas detected — installing to ${TARGET_PREFIX}/bin${RESET}"
fi
spinner "Installing..." "$INSTALL_CMD"

# Optional: strip binary
if have strip; then
  DEST_BIN="${TARGET_PREFIX}/bin/${APP_NAME}"
  if [ -e "$DEST_BIN" ]; then
    spinner "Stripping binary..." "${SUDO:+$SUDO }strip \"$DEST_BIN\" 2>/dev/null || true"
  fi
fi

# Man page (optional)
progress "Installing man page (if present)..."
if [ -f "./${APP_NAME}.1" ]; then
  MAN_DIR="${TARGET_PREFIX}/share/man/man1"
  spinner "Installing ${APP_NAME}.1..." "${SUDO:+$SUDO }mkdir -p \"$MAN_DIR\" && ${SUDO:+$SUDO }install -m 0644 \"./${APP_NAME}.1\" \"$MAN_DIR/${APP_NAME}.1\""
  # Try gzip if man pages are normally compressed
  if have gzip; then
    spinner "Compressing man page..." "${SUDO:+$SUDO }gzip -f -9 \"$MAN_DIR/${APP_NAME}.1\" 2>/dev/null || true"
  fi
  # Refresh man database if available
  if have mandb; then
    spinner "Refreshing man database..." "${SUDO:+$SUDO }mandb -q 2>/dev/null || true"
  fi
fi

# PATH
progress "Checking PATH..."
BIN_DIR="${TARGET_PREFIX}/bin"
if ! printf "%s" ":$PATH:" | grep -q ":$BIN_DIR:"; then
  PROFILE=""
  [ -n "${ZDOTDIR-}" ] && [ -w "${ZDOTDIR}/.zprofile" 2>/dev/null ] && PROFILE="${ZDOTDIR}/.zprofile"
  [ -z "$PROFILE" ] && [ -w "$HOME/.zprofile" 2>/dev/null ] && PROFILE="$HOME/.zprofile"
  [ -z "$PROFILE" ] && [ -w "$HOME/.bash_profile" 2>/dev/null ] && PROFILE="$HOME/.bash_profile"
  [ -z "$PROFILE" ] && PROFILE="$HOME/.profile"

  mkdir -p "$(dirname "$PROFILE")" 2>/dev/null || true
  if ! grep -F "$BIN_DIR" "$PROFILE" >/dev/null 2>&1; then
    printf '\n# Added by %s installer\nexport PATH="%s:$PATH"\n' "$APP_NAME" "$BIN_DIR" >> "$PROFILE"
    say "Added ${BIN_DIR} to PATH in ${PROFILE}."
    say "Restart your shell or run: ${BOLD}export PATH=\"${BIN_DIR}:\$PATH\"${RESET}"
  fi
fi

# Finalize (no extra bar step -> no >100%)
printf "\n"
echo "${GREEN}${BOLD}✅ ${APP_NAME} installed successfully!${RESET}"
echo "Log: $LOG"
echo "Try: ${BOLD}${APP_NAME}${RESET}  (manpage: ${BOLD}man ${APP_NAME}${RESET} if installed)"
