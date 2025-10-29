#!/usr/bin/env sh
# tedit uninstaller — POSIX sh, single pretty progress bar, no hangs

set -u

APP_NAME="tedit"
LOG="/tmp/${APP_NAME}-uninstall.$$".log

PURGE_REPO=0
PURGE_USER=0
ASSUME_YES=0

# ---------- Parse args ----------
while [ $# -gt 0 ]; do
  case "$1" in
    --purge-repo)      PURGE_REPO=1 ;;
    --purge-user-data) PURGE_USER=1 ;;
    --purge)           PURGE_REPO=1; PURGE_USER=1 ;;
    -y|--yes)          ASSUME_YES=1 ;;
    -h|--help)
      cat <<EOF
Usage: $0 [options]
  --purge-repo        Remove this git repository after uninstall (asks unless -y)
  --purge-user-data   Remove ~/.teditrc, ~/.tedit_banner, ~/.tedit/hooks, ~/.tedit-recover-*
  --purge             Do both --purge-repo and --purge-user-data
  -y, --yes           Non-interactive (assume yes)
EOF
      exit 0
      ;;
    *) printf "Unknown option: %s\n" "$1" >&2; exit 2 ;;
  esac
  shift
done

# ---------- Colors ----------
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
  GREEN="$(tput setaf 2)"; YELLOW="$(tput setaf 3)"; RED="$(tput setaf 1)"; CYAN="$(tput setaf 6)"; BOLD="$(tput bold)"; RESET="$(tput sgr0)"
else
  GREEN=""; YELLOW=""; RED=""; CYAN=""; BOLD=""; RESET=""
fi

log() { printf "%s\n" "$*" >>"$LOG"; }

warn() {
  log "WARNING: $*"
  printf "\n%sWARNING:%s %s\n" "$YELLOW" "$RESET" "$*"
}

have() { command -v "$1" >/dev/null 2>&1; }

# ---------- Repo dir ----------
SCRIPT_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)

# ---------- Privileges ----------
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if have doas; then SUDO="doas"
  elif have sudo; then SUDO="sudo"
  fi
fi

# authenticate once; later, we use -n to avoid mid-bar prompts
if [ -n "$SUDO" ]; then
  printf "%s" "$CYAN$BOLD"
  printf "Uninstalling %s... (authenticating) " "$APP_NAME"
  printf "%s" "$RESET"
  $SUDO -v 2>/dev/null || $SUDO true || :
  printf "\n"
else
  printf "%s" "$CYAN$BOLD"
  printf "Uninstalling %s..." "$APP_NAME"
  printf "%s\n" "$RESET"
fi

SUDO_NONINT=""
if [ -n "$SUDO" ]; then SUDO_NONINT="$SUDO -n"; fi

# ---------- Pretty one-line progress bar ----------
is_utf() { echo "${LC_ALL:-${LANG:-}}" | grep -qi 'utf-8'; }
if is_utf; then FIL="█"; EMP="░"; else FIL="#"; EMP="-"; fi

repeat() { n=$1; ch=$2; i=0; out=""; while [ "$i" -lt "$n" ]; do out="$out$ch"; i=$((i+1)); done; printf "%s" "$out"; }
term_width() { w=80; if command -v tput >/dev/null 2>&1; then w=$(tput cols 2>/dev/null || echo 80); fi; [ "$w" -gt 20 ] || w=80; echo "$w"; }

draw_bar() {
  n=$1 tot=$2 msg=$3
  tw=$(term_width)
  [ "$tot" -gt 0 ] || tot=1
  bw=$(( tw - 32 )); [ "$bw" -lt 10 ] && bw=10; [ "$bw" -gt 60 ] && bw=60
  pct=$(( n*100 / tot ))
  fill=$(( n*bw / tot ))
  empty=$(( bw - fill ))
  bar="$(repeat "$fill" "$FIL")$(repeat "$empty" "$EMP")"
  line="$(printf "%s%s[%s]%s %d/%d (%d%%) %s" "$CYAN" "$BOLD" "$bar" "$RESET" "$n" "$tot" "$pct" "$msg")"
  # clear the whole line to kill leftovers, then print
  printf "\r\033[K%s" "$line"
  log "$(printf '[%s] %d/%d (%d%%) %s' "$(repeat "$fill" "#")$(repeat "$empty" "-")" "$n" "$tot" "$pct" "$msg")"
}

finish_bar() { printf "\r\033[K\n"; }

# Hide cursor if possible; restore on exit
CURSOR_HIDE=""; CURSOR_SHOW=""
if command -v tput >/dev/null 2>&1; then
  CURSOR_HIDE="$(tput civis 2>/dev/null || true)"
  CURSOR_SHOW="$(tput cnorm 2>/dev/null || true)"
fi
printf "%s" "$CURSOR_HIDE"
trap 'finish_bar; printf "%s" "$CURSOR_SHOW"' EXIT INT TERM

# ---------- Helpers ----------
SUDO_RUN() {
  if [ -n "$SUDO" ]; then
    # try non-interactive to avoid prompts during the bar
    $SUDO_NONINT "$@" 2>/dev/null || $SUDO "$@" 2>/dev/null || :
  else
    "$@" 2>/dev/null || :
  fi
}

rm_file() {
  P="$1"; [ -e "$P" ] || return 0
  case "$P" in
    /usr/*|/etc/*|/opt/*|/lib*/*|/var/*) SUDO_RUN rm -f -- "$P" ;;
    *) rm -f -- "$P" 2>/dev/null || : ;;
  esac
}

rm_dir() {
  D="$1"; [ -d "$D" ] || return 0
  case "$D" in
    /usr/*|/etc/*|/opt/*|/lib*/*|/var/*) SUDO_RUN rm -rf -- "$D" ;;
    *) rm -rf -- "$D" 2>/dev/null || : ;;
  esac
}

remove_named_everywhere() {
  NAME="$1"
  for D in /usr/local/bin /usr/bin "$HOME/.local/bin"; do
    [ -f "$D/$NAME" ] && rm_file "$D/$NAME"
  done
  OLDIFS=$IFS; IFS=:
  for D in $PATH; do [ -n "$D" ] && [ -f "$D/$NAME" ] && rm_file "$D/$NAME"; done
  IFS=$OLDIFS
  RES="$(command -v "$NAME" 2>/dev/null || true)"
  [ -n "$RES" ] && [ -f "$RES" ] && rm_file "$RES"
  hash -r 2>/dev/null || :
}

remove_manpages_for() {
  NAME="$1"
  for MP in "/usr/local/share/man/man1/${NAME}.1" "/usr/share/man/man1/${NAME}.1" "$HOME/.local/share/man/man1/${NAME}.1"; do
    rm_file "$MP"; rm_file "${MP}.gz"; rm_file "${MP}.bz2"; rm_file "${MP}.xz"
  done
}

clean_profile_file() {
  PROF="$1"; [ -f "$PROF" ] || return 0
  TMP="${PROF}.tmp.$$"
  awk '
    BEGIN { skip=0 }
    /^# Added by tedit installer$/ { skip=2; next }
    /^# Added by tedit init$/      { skip=2; next }
    { if (skip>0) { skip--; next } else { print } }
  ' "$PROF" >"$TMP" && mv "$TMP" "$PROF"
}

confirm() {
  [ "$ASSUME_YES" -eq 1 ] && return 0
  finish_bar
  printf "%s%s?%s [y/N] " "$BOLD" "$1" "$RESET" 1>&2
  read -r A || A=""
  case "$A" in y|Y|yes|YES) return 0 ;; *) return 1 ;; esac
}

# ---------- Plan steps ----------
# Order: 1..6 ops, optional 7 (user purge), 8 post-check, optional 9 (repo purge & exit)
TOTAL=7  # base ops(6) + post-check
[ "$PURGE_USER" -eq 1 ] && TOTAL=$((TOTAL+1))
[ "$PURGE_REPO" -eq 1 ] && TOTAL=$((TOTAL+1))

STEP=0
next() { STEP=$((STEP+1)); draw_bar "$STEP" "$TOTAL" "$1"; }

# 1) remove binary
remove_named_everywhere "$APP_NAME";           next "Remove installed binary"

# 2) wrappers
for W in tedit-install tedit-update tedit-uninstall; do remove_named_everywhere "$W"; done
next "Remove wrappers"

# 3) man pages
remove_manpages_for "$APP_NAME";               next "Remove man pages"

# 4) PATH profile lines
for PROF in "$HOME/.zprofile" "$HOME/.bash_profile" "$HOME/.profile"; do clean_profile_file "$PROF"; done
next "Clean PATH lines"

# 5) repo build droppings (but not the repo dir itself)
if [ -d "$SCRIPT_DIR" ]; then
  ( cd "$SCRIPT_DIR" 2>/dev/null || :
    if [ -f Makefile ] && have make; then make clean >/dev/null 2>&1 || :; fi
    rm_file "./${APP_NAME}"
    rm -f ./*.o ./*.obj ./*.d ./*.a ./*.so ./*.dSYM 2>/dev/null || :
    rm -rf ./build ./out ./dist ./CMakeFiles 2>/dev/null || :
    rm -f ./CMakeCache.txt ./compile_commands.json 2>/dev/null || :
  )
fi
next "Remove local build artifacts"

# 6) refresh man db (non-interactive; skip if creds expired)
if have mandb; then
  if [ -n "$SUDO" ]; then $SUDO -n mandb -q 2>/dev/null || :; else mandb -q 2>/dev/null || :; fi
elif have makewhatis; then
  if [ -n "$SUDO" ]; then $SUDO -n makewhatis 2>/dev/null || :; else makewhatis 2>/dev/null || :; fi
fi
next "Refresh man database"

# 7) optional: user data
if [ "$PURGE_USER" -eq 1 ]; then
  rm -f "$HOME/.teditrc" "$HOME/.tedit_banner" 2>/dev/null || :
  rm -rf "$HOME/.tedit/hooks" 2>/dev/null || :
  for F in "$HOME"/.tedit-recover-*; do [ -e "$F" ] && rm -f -- "$F" 2>/dev/null || :; done
  next "Purge user data"
fi

# 8) post-check BEFORE repo deletion
LEFT_BIN="$(command -v "$APP_NAME" 2>/dev/null || true)"
LEFT_WRAP=""; for W in tedit-install tedit-update tedit-uninstall; do command -v "$W" >/dev/null 2>&1 && LEFT_WRAP="$LEFT_WRAP $W"; done
LEFT_MAN=""; command -v man >/dev/null 2>&1 && LEFT_MAN="$(man -w "$APP_NAME" 2>/dev/null || true)"

if [ -z "$LEFT_BIN$LEFT_WRAP$LEFT_MAN" ]; then
  next "Post-check for leftovers"
else
  next "Post-check detected traces"
  finish_bar
  [ -n "$LEFT_BIN" ]  && printf "  - binary:  %s\n" "$LEFT_BIN"
  [ -n "$LEFT_WRAP" ] && printf "  - wrappers:%s\n" "$LEFT_WRAP"
  [ -n "$LEFT_MAN" ]  && printf "  - manpage: %s\n" "$LEFT_MAN"
  printf "You can remove the above paths manually, then run: hash -r\n"
  printf "%s" "$CURSOR_SHOW"
  exit 1
fi

# 9) optional: purge repo dir LAST, from parent, then exit immediately
if [ "$PURGE_REPO" -eq 1 ]; then
  if [ -d "$SCRIPT_DIR/.git" ] && [ -f "$SCRIPT_DIR/tedit.cpp" ]; then
    PARENT="$(dirname "$SCRIPT_DIR")"; BASE="$(basename "$SCRIPT_DIR")"
    if [ "$ASSUME_YES" -eq 1 ] || confirm "Remove repository directory '$SCRIPT_DIR' (cannot be undone)"; then
      ( cd "$PARENT" 2>/dev/null || exit 0
        rm_dir "$BASE"
      )
    fi
  else
    warn "This directory does not look like the tedit repo; skipped --purge-repo."
  fi
  next "Purge repository directory"
  finish_bar
  printf "%s%s✅ %s uninstalled successfully.%s\n" "$GREEN" "$BOLD" "$APP_NAME" "$RESET"
  printf "Log: %s\n" "$LOG"
  exit 0
fi

finish_bar
printf "%s%s✅ %s uninstalled successfully.%s\n" "$GREEN" "$BOLD" "$APP_NAME" "$RESET"
printf "Log: %s\n" "$LOG"
