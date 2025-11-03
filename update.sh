#!/usr/bin/env sh

set -eu

APP_NAME="tedit"
DEFAULT_PREFIX="/usr/local"
USER_PREFIX="$HOME/.local"
LOG="$(mktemp -t ${APP_NAME}-update.XXXXXX.log)"

# -------- Locate repo (TEDIT_REPO, script dir, marker, fallback) --------
SCRIPT_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)

REPO_DIR="${TEDIT_REPO:-}"

# 1) explicit env
if [ -n "$REPO_DIR" ] && [ -f "$REPO_DIR/tedit.cpp" ]; then
  :
# 2) current working dir
elif [ -f "$PWD/tedit.cpp" ]; then
  REPO_DIR="$PWD"
# 3) script dir
elif [ -f "$SCRIPT_DIR/tedit.cpp" ]; then
  REPO_DIR="$SCRIPT_DIR"
# 4) system marker(s)
elif [ -f /usr/local/share/tedit/repo ]; then
  RD="$(cat /usr/local/share/tedit/repo 2>/dev/null || true)"
  [ -n "$RD" ] && [ -f "$RD/tedit.cpp" ] && REPO_DIR="$RD"
elif [ -f "$HOME/.local/share/tedit/repo" ]; then
  RD="$(cat "$HOME/.local/share/tedit/repo" 2>/dev/null || true)"
  [ -n "$RD" ] && [ -f "$RD/tedit.cpp" ] && REPO_DIR="$RD"
# 5) common fallback
elif [ -f "$HOME/tedit/tedit.cpp" ]; then
  REPO_DIR="$HOME/tedit"
fi

[ -n "$REPO_DIR" ] || { echo "ERROR: Could not locate the tedit repo. Set TEDIT_REPO=/absolute/path/to/repo"; exit 2; }
cd "$REPO_DIR" || { echo "ERROR: cannot cd to $REPO_DIR"; exit 2; }

# -------- Colors --------
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
  GREEN="$(tput setaf 2)"; YELLOW="$(tput setaf 3)"; RED="$(tput setaf 1)"; CYAN="$(tput setaf 6)"
  BOLD="$(tput bold)"; RESET="$(tput sgr0)"
else
  GREEN=""; YELLOW=""; RED=""; CYAN=""; BOLD=""; RESET=""
fi

say() { printf "%s\n" "$*" | tee -a "$LOG" >/dev/null; }
have(){ command -v "$1" >/dev/null 2>&1; }
die(){
  printf "%sERROR:%s %s\n" "$RED" "$RESET" "$1" | tee -a "$LOG" >&2
  exit 1
}

# -------- Progress bar + spinner --------
is_utf(){ echo "${LC_ALL:-${LANG:-}}" | grep -qi 'utf-8'; }
repeat(){
  n=$1; ch=$2; i=0; out=""
  while [ "$i" -lt "$n" ]; do out="$out$ch"; i=$((i+1)); done
  printf "%s" "$out"
}
term_width(){
  w=80
  if command -v tput >/dev/null 2>&1; then
    w=$(tput cols 2>/dev/null || echo 80)
  fi
  [ "$w" -gt 24 ] || w=80
  echo "$w"
}

if is_utf; then FIL="█"; EMP="░"; else FIL="#"; EMP="-"; fi

TOTAL=12
STEP=0

draw_bar(){
  n=$1; tot=$2; msg=$3
  tw=$(term_width)
  bw=$(( tw - 32 ))
  [ "$bw" -lt 12 ] && bw=12
  [ "$bw" -gt 60 ] && bw=60
  [ "$tot" -gt 0 ] || tot=1
  pct=$(( n*100 / tot ))
  fill=$(( n*bw / tot ))
  empty=$(( bw - fill ))
  bar="$(repeat "$fill" "$FIL")$(repeat "$empty" "$EMP")"
  printf "\r\033[K%s%s[%s]%s %d/%d (%d%%) %s" \
    "$CYAN" "$BOLD" "$bar" "$RESET" "$n" "$tot" "$pct" "$msg"
  printf "[%s] %d/%d (%d%%) %s\n" \
    "$(repeat "$fill" "#")$(repeat "$empty" "-")" "$n" "$tot" "$pct" "$msg" >>"$LOG"
}

next(){
  STEP=$((STEP+1))
  [ "$STEP" -gt "$TOTAL" ] && STEP="$TOTAL"
  draw_bar "$STEP" "$TOTAL" "$1"
}

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
    i=$(( (i+1) % 4 ))
    c=$(printf %s "$frames" | cut -c $((i+1)))
    printf "\r\033[K[%s] %s" "$c" "$msg"
    sleep 0.1
  done
  wait "$pid" 2>/dev/null || true
  printf "\r\033[K"
}

# -------- Cursor + finish UI --------
CURSOR_HIDE=""; CURSOR_SHOW=""
if command -v tput >/dev/null 2>&1; then
  CURSOR_HIDE="$(tput civis 2>/dev/null || true)"
  CURSOR_SHOW="$(tput cnorm 2>/dev/null || true)"
fi
printf "%s" "$CURSOR_HIDE"

finish_ui(){
  printf "\r\033[K\n%s" "$CURSOR_SHOW"
}
trap 'finish_ui' EXIT INT TERM

printf "%s%s>> Updating %s <<%s\n" "$CYAN" "$BOLD" "$APP_NAME" "$RESET" | tee -a "$LOG" >/dev/null

next "Locating repository"
[ -f "tedit.cpp" ] || die "This does not look like the tedit repo."

# -------- Privileges --------
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  # Prefer sudo, but fall back to doas (cuz doas is sigma)
  if have sudo; then
    SUDO="sudo"
  elif have doas; then
    SUDO="doas"
  fi
fi

auth_once(){
  [ -n "$SUDO" ] || return 0
  printf "\r\033[K%s%sElevating privileges (may prompt once)...%s\n" "$CYAN" "$BOLD" "$RESET"
  if [ "$SUDO" = "sudo" ]; then
    sudo -v || true
  else
    $SUDO true || true
  fi
}

run_root(){
  msg="$1"; cmd="$2"
  if [ -z "$SUDO" ]; then
    spinner "$msg" "$cmd"
    return
  fi
  if $SUDO -n true 2>/dev/null; then
    spinner "$msg" "$SUDO $cmd"
  else
    printf "\r\033[K%s%s%s %s\n" "$YELLOW" "[auth]" "$RESET" "$msg"
    $SUDO sh -c "$cmd" 2>&1 | tee -a "$LOG"
    draw_bar "$STEP" "$TOTAL" "$msg"
  fi
}

# -------- Detect package manager --------
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

# -------- Dependencies (git, make, compiler) --------
need_deps(){
  need=0
  have git  || need=1
  have make || need=1
  if have c++; then :; elif have g++; then :; elif have clang++; then :; else need=1; fi
  return $need
}

install_deps(){
  if [ "${ID:-}" = "gentoo" ] || [ "$PKG" = "emerge" ]; then
    printf "%sGentoo detected — emerging toolchain (may be interactive).%s\n" \
      "$YELLOW" "$RESET" | tee -a "$LOG" >/dev/null
  fi

  case "$PKG" in
    apk)
      run_root "Installing deps (apk)..." \
        "apk add --no-cache --update-cache git build-base"
      ;;
    apt)
      run_root "Updating APT index..." \
        "sh -c 'apt-get update -y || apt update -y'"
      run_root "Installing deps (apt)..." \
        "sh -c 'apt-get install -y git make build-essential || apt install -y git make build-essential'"
      ;;
    dnf|yum)
      run_root "Installing deps ($PKG)..." \
        "$PKG install -y git make gcc gcc-c++"
      ;;
    pacman)
      run_root "Syncing pacman..." "pacman -Sy --noconfirm"
      run_root "Installing deps (pacman)..." \
        "pacman -S --needed --noconfirm git gcc make"
      ;;
    zypper)
      run_root "Installing deps (zypper)..." \
        "zypper -n install git make gcc-c++"
      ;;
    xbps-install)
      run_root "Installing deps (xbps)..." \
        "xbps-install -Sy git gcc make"
      ;;
    eopkg)
      run_root "Installing deps (eopkg)..." \
        "sh -c 'eopkg -y it -c system.devel git || eopkg -y it make gcc binutils git'"
      ;;
    emerge)
      run_root "Emerging deps (gentoo)..." \
        "emerge --quiet-build=y --oneshot sys-devel/gcc sys-devel/make dev-vcs/git || true"
      ;;
    brew)
      spinner "Installing deps (brew)..." \
        "brew install git make gcc || true"
      ;;
    "")
      die "Unsupported/undetected package manager. Install git, make, and a C++17 compiler, then re-run."
      ;;
  esac

  have git  || die "git still missing after install."
  have make || die "make still missing after install."
  if have c++; then :; elif have g++; then :; elif have clang++; then :; else
    die "No C++17 compiler after install."
  fi
}

next "Detecting package manager"
[ -n "$PKG" ] || say "No package manager detected; assuming deps are already installed."

next "Checking dependencies"
if need_deps; then install_deps; fi

next "Preparing privileges"
auth_once
draw_bar "$STEP" "$TOTAL" "Preparing privileges"

# -------- Git helpers --------
quiet_git(){
  GIT_TERMINAL_PROMPT=0 git \
    -c advice.statusHints=false \
    -c advice.detachedHead=false "$@" >>"$LOG" 2>&1
}

IN_GIT=0
if have git && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  IN_GIT=1
fi

# -------- Check remote for updates --------
next "Checking for updates"

if [ "$IN_GIT" -eq 1 ]; then
  spinner "Fetching remote..." "quiet_git fetch --quiet --all --tags --prune || true"

  branch="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "")"
  upstream="$(git rev-parse --abbrev-ref --symbolic-full-name '@{u}' 2>/dev/null || echo "")"

  # If no explicit upstream, assume origin/<branch> if that exists
  if [ -z "$upstream" ] && [ -n "$branch" ]; then
    if git rev-parse "origin/$branch" >/dev/null 2>&1; then
      upstream="origin/$branch"
    fi
  fi

  if [ -n "$upstream" ]; then
    ahead="$(git rev-list --count "HEAD..$upstream" 2>/dev/null || echo 0)"
    if [ "$ahead" -eq 0 ]; then
      finish_ui
      printf "Up to date.\n"
      printf "Log: %s\n" "$LOG"
      exit 0
    fi
  else
    say "No upstream configured; rebuilding current checkout."
  fi
else
  say "Not a git repo; rebuilding current directory."
fi

# -------- Prepare working tree (stash if dirty) --------
next "Preparing working tree"
STASHED=0
if [ "$IN_GIT" -eq 1 ]; then
  quiet_git update-index -q --refresh || :
  DIRTY=0
  git diff --quiet --ignore-submodules -- || DIRTY=1
  git diff --cached --quiet --ignore-submodules -- || DIRTY=1
  if [ "$DIRTY" -eq 1 ]; then
    spinner "Stashing local changes..." \
      "quiet_git stash push -u -m 'tedit-autostash-$(date +%s)' || true"
    STASHED=1
  fi
fi

# -------- Pull changes (if in git) --------
next "Fetching latest changes"
if [ "$IN_GIT" -eq 1 ] && [ -n "${upstream:-}" ]; then
  spinner "Updating sources..." \
    "quiet_git pull --rebase --autostash --stat || quiet_git pull --rebase || quiet_git pull || true"
fi

if [ "$STASHED" -eq 1 ]; then
  spinner "Restoring stash..." "quiet_git stash pop || true"
fi

# -------- Compiler selection --------
choose_cxx(){
  if   have c++;   then printf %s "c++"
  elif have g++;   then printf %s "g++"
  elif have clang++; then printf %s "clang++"
  else printf %s "c++"
  fi
}

next "Selecting compiler"
CXX_BIN="$(choose_cxx)"
printf "Using %s\n" "$CXX_BIN" >>"$LOG"

# -------- Build --------
next "Building"
spinner "make ..." "CXX='$CXX_BIN' make || { echo BUILD_FAIL >>'$LOG'; exit 1; }" \
  || die "Build failed."

# -------- Install (respect existing prefix if possible) --------
next "Installing"

# Infer where tedit currently lives to choose prefix
TARGET_PREFIX="$DEFAULT_PREFIX"
CUR_BIN="$(command -v "$APP_NAME" 2>/dev/null || true)"
case "$CUR_BIN" in
  "$HOME/.local/bin/"*|*/.local/bin/"$APP_NAME")
    TARGET_PREFIX="$USER_PREFIX"
    ;;
esac

INSTALL_CMD="make PREFIX='$TARGET_PREFIX' install"

if [ "$TARGET_PREFIX" = "$DEFAULT_PREFIX" ]; then
  # System-wide install: root when needed
  if [ "$(id -u)" -eq 0 ]; then
    spinner "Installing (system)..." "$INSTALL_CMD"
  else
    run_root "Installing (system)..." "$INSTALL_CMD"
  fi
else
  printf "%sNote:%s installing to %s/bin (user-level)\n" \
    "$YELLOW" "$RESET" "$TARGET_PREFIX" | tee -a "$LOG" >/dev/null
  spinner "Installing (user)..." "$INSTALL_CMD"
fi

# -------- Strip binary (optional) --------
next "Optimizing binary"
if have strip; then
  BIN="${TARGET_PREFIX}/bin/${APP_NAME}"
  if [ -e "$BIN" ]; then
    spinner "Stripping binary..." \
      "${SUDO:+$SUDO }strip '$BIN' 2>/dev/null || true"
  fi
fi

# -------- Man page (best-effort, like installer) --------
next "Updating man page"
if [ -f "./${APP_NAME}.1" ]; then
  MAN_DIR="${TARGET_PREFIX}/share/man/man1"
  run_root "Copying man page..." \
    "mkdir -p '$MAN_DIR' && install -m 0644 './${APP_NAME}.1' '$MAN_DIR/${APP_NAME}.1'"
  if have gzip; then
    run_root "Compressing man page..." \
      "gzip -f -9 '$MAN_DIR/${APP_NAME}.1' 2>/dev/null || true"
  fi
  if have mandb; then
    run_root "Refreshing man database..." "mandb -q 2>/dev/null || true"
  elif have makewhatis; then
    run_root "Refreshing man database..." "makewhatis 2>/dev/null || true"
  fi
else
  printf "No local man page; skipping.\n" >>"$LOG"
fi

next "Done"
finish_ui
printf "%s%s✅ %s updated successfully.%s\n" "$GREEN" "$BOLD" "$APP_NAME" "$RESET"
printf "Log: %s\n" "$LOG"
