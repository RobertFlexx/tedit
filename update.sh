#!/usr/bin/env sh
# tedit updater (POSIX sh)
# - Checks for git updates; rebuilds & reinstalls if newer
# - Handles dependencies per distro
# - Adds progress bar and smart logging

set -eu

APP_NAME="tedit"
DEFAULT_PREFIX="/usr/local"
USER_PREFIX="$HOME/.local"
LOG="$(mktemp -t ${APP_NAME}-update.XXXXXX.log)"

# --- Colors ---
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

# --- Privilege helper ---
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if have sudo; then SUDO="sudo"
  elif have doas; then SUDO="doas"
  fi
fi

# --- Progress bar ---
TOTAL=7 STEP=0 WIDTH=36
progress() {
  STEP=$((STEP + 1))
  [ "$STEP" -gt "$TOTAL" ] && STEP="$TOTAL"
  pct=$((100 * STEP / TOTAL))
  filled=$((WIDTH * STEP / TOTAL))
  unfilled=$((WIDTH - filled))
  printf "\r[%s%s] %3d%%  %s" \
    "$(printf '%0.s#' $(seq 1 $filled))" \
    "$(printf '%0.s ' $(seq 1 $unfilled))" \
    "$pct" "$1"
}

# --- Spinner ---
spinner() {
  msg="$1"; shift
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

# --- Sanity checks ---
[ -f "tedit.cpp" ] || die "Run from the tedit source directory (tedit.cpp not found)."
[ -d ".git" ] || die "This is not a git repository."

# --- Dependency check ---
need_deps() {
  need=0
  have make || need=1
  have git || need=1
  (have c++ || have g++ || have clang++) || need=1
  return $need
}

# --- Install missing deps ---
install_deps() {
  PKG=""
  for p in apt dnf yum pacman zypper apk xbps-install eopkg emerge brew; do
    command -v "$p" >/dev/null 2>&1 && PKG="$p" && break
  done

  say "${CYAN}Installing missing dependencies using ${BOLD}${PKG:-manual}${RESET}..."
  case "$PKG" in
    apt)
      spinner "Installing deps (APT)..." "${SUDO:+$SUDO }apt-get update -y && ${SUDO:+$SUDO }apt-get install -y git make build-essential"
      ;;
    dnf|yum)
      spinner "Installing deps (DNF/YUM)..." "${SUDO:+$SUDO }$PKG install -y git make gcc-c++"
      ;;
    pacman)
      spinner "Installing deps (Pacman)..." "${SUDO:+$SUDO }pacman -Sy --noconfirm git gcc make"
      ;;
    zypper)
      spinner "Installing deps (Zypper)..." "${SUDO:+$SUDO }zypper -n install git make gcc-c++"
      ;;
    apk)
      spinner "Installing deps (Alpine)..." "${SUDO:+$SUDO }apk add --no-cache git build-base"
      ;;
    xbps-install)
      spinner "Installing deps (Void)..." "${SUDO:+$SUDO }xbps-install -Sy git gcc make"
      ;;
    eopkg)
      spinner "Installing deps (Solus)..." "${SUDO:+$SUDO }eopkg -y it -c system.devel git"
      ;;
    emerge)
      say "${YELLOW}Gentoo detected — emerging toolchain (may be interactive).${RESET}"
      spinner "Emerging deps..." "${SUDO:+$SUDO }emerge --quiet-build=y --oneshot sys-devel/gcc sys-devel/make dev-vcs/git || true"
      ;;
    brew)
      spinner "Installing deps (Homebrew)..." "brew install git make gcc || true"
      ;;
    *)
      die "Unsupported/undetected package manager. Install git, make, and a C++17 compiler manually."
      ;;
  esac
}

# --- Start ---
say "${CYAN}${BOLD}Checking ${APP_NAME} environment...${RESET}"
progress "Checking dependencies..."
if need_deps; then
  say "${YELLOW}Missing dependencies — attempting to install...${RESET}"
  install_deps
else
  say "Dependencies look good."
fi
sleep 0.2

# --- Check for uncommitted changes ---
progress "Checking local changes..."
if ! git diff --quiet || ! git diff --cached --quiet; then
  printf "\n${YELLOW}⚠ You have uncommitted local changes.${RESET}\n"
  echo "Please commit or stash them before updating."
  echo "Example:"
  echo "  git add . && git commit -m 'save work'"
  echo "  sh update.sh"
  exit 1
fi
sleep 0.2

# --- Compiler selection ---
progress "Selecting compiler..."
choose_cxx() {
  for c in c++ g++ clang++; do have "$c" && printf "%s" "$c" && return; done
  die "No C++ compiler found (need c++/g++/clang++)."
}
CXX_BIN="$(choose_cxx)"
sleep 0.2

# --- Git update check ---
progress "Checking remote updates..."
branch="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || true)"
[ -n "$branch" ] || die "Unable to determine current branch."
upstream="$(git rev-parse --abbrev-ref --symbolic-full-name @{u} 2>/dev/null || true)"
[ -n "$upstream" ] || upstream="origin/$branch"

git fetch --quiet || say "${YELLOW}Warning:${RESET} git fetch failed (offline?)."
local_rev="$(git rev-parse HEAD)"
remote_rev="$(git rev-parse "$upstream" 2>/dev/null || echo "")"

if [ -n "$remote_rev" ] && [ "$local_rev" = "$remote_rev" ]; then
  printf "\n${GREEN}${BOLD}${APP_NAME} is already up to date.${RESET}\n"
  exit 0
fi

say "${GREEN}Updates found on branch ${BOLD}$branch${RESET}."
sleep 0.2

# --- Pull updates ---
progress "Pulling latest changes..."
if git rev-parse --verify "$upstream" >/dev/null 2>&1; then
  spinner "Rebasing..." "git pull --rebase --stat || git pull"
else
  spinner "Pulling..." "git pull || true"
fi
sleep 0.2

# --- Build ---
progress "Rebuilding ${APP_NAME}..."
spinner "Building with ${CXX_BIN}..." "make clean >/dev/null 2>&1 || true && CXX='$CXX_BIN' make"
sleep 0.2

# --- Install ---
progress "Installing ${APP_NAME}..."
TARGET_PREFIX="$DEFAULT_PREFIX"
INSTALL_CMD="${SUDO:+$SUDO }make install"
if [ -z "$SUDO" ] && [ "$(id -u)" -ne 0 ]; then
  TARGET_PREFIX="$USER_PREFIX"
  INSTALL_CMD="make PREFIX='$TARGET_PREFIX' install"
  say "${YELLOW}No sudo/doas detected — installing to ${TARGET_PREFIX}/bin${RESET}"
fi
spinner "Installing..." "$INSTALL_CMD"

# --- Optional strip ---
progress "Optimizing binary..."
if have strip; then
  BIN="${TARGET_PREFIX}/bin/${APP_NAME}"
  if [ -e "$BIN" ]; then
    ${SUDO:+$SUDO }strip "$BIN" 2>/dev/null || true
  fi
fi

# --- Finish ---
printf "\n${GREEN}${BOLD}✅ ${APP_NAME} updated successfully!${RESET}\n"
echo "Log: $LOG"
echo "Run '${BOLD}${APP_NAME}${RESET}' to start."
