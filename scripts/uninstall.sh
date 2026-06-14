#!/usr/bin/env sh
set -eu

APP_NAME="tedit"

# -----------------------------
# temp/log helpers (portable-ish)
# -----------------------------
mktemp_file() {
  if tmp=$(mktemp "${TMPDIR:-/tmp}/${APP_NAME}-uninstall.XXXXXX" 2>/dev/null); then
    printf "%s" "$tmp"; return 0
  fi
  if tmp=$(mktemp -t "${APP_NAME}-uninstall" 2>/dev/null); then
    printf "%s" "$tmp"; return 0
  fi
  printf "%s" "${TMPDIR:-/tmp}/${APP_NAME}-uninstall.$(date +%s).$$"
}

LOG="${LOG_FILE:-$(mktemp_file)}.log"
PLAN="$(mktemp_file).plan"
PLANU="$(mktemp_file).plan.unique"

: >"$LOG" 2>/dev/null || { echo "ERROR: cannot write log: $LOG" >&2; exit 2; }
: >"$PLAN" 2>/dev/null || { echo "ERROR: cannot write plan: $PLAN" >&2; exit 2; }

have(){ command -v "$1" >/dev/null 2>&1; }
TTY=0; [ -t 1 ] && TTY=1

# -----------------------------
# options
# -----------------------------
PURGE_REPO=0
PURGE_USER=0
ASSUME_YES=0
DRY_RUN=0
FORCE=0

usage(){
  cat <<EOF
$APP_NAME uninstaller

Usage:
  ./tedit-uninstall [options]

Options:
  --purge-repo         Remove the *repo directory this script is in* (asks unless -y)
  --purge-user-data    Remove user config/data (see below)
  --purge              Both --purge-repo and --purge-user-data
  --force              Remove even if files appear package-owned (dangerous, bruh)
  --dry-run            Show what would be removed, do nothing
  -y, --yes            Non-interactive (assume yes)
  -h, --help           Show help

--purge-user-data removes (best-effort):
  ~/.teditrc ~/.tedit_banner ~/.tedit/ ~/tedit-config
  ~/.tedit-recover-* ~/.config/tedit ~/.cache/tedit ~/.local/state/tedit
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --purge-repo)      PURGE_REPO=1 ;;
    --purge-user-data) PURGE_USER=1 ;;
    --purge)           PURGE_REPO=1; PURGE_USER=1 ;;
    --force)           FORCE=1 ;;
    --dry-run)         DRY_RUN=1 ;;
    -y|--yes)          ASSUME_YES=1 ;;
    -h|--help)         usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
  shift
done

# -----------------------------
# colors + logging
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
info(){ say "${CYAN}${BOLD}::${RESET} $*"; }
ok(){   say "${GREEN}${BOLD}✓${RESET} $*"; }
warn(){ say "${YELLOW}${BOLD}!${RESET} $*"; }
err(){  say "${RED}${BOLD}x${RESET} $*"; }

cleanup_tmp(){
  rm -f "$PLAN" "$PLANU" 2>/dev/null || :
  # keep LOG
}
trap 'cleanup_tmp' EXIT INT TERM

# -----------------------------
# UI: progress bar (category-level)
# -----------------------------
is_utf(){ echo "${LC_ALL:-${LANG:-}}" | grep -qi 'utf-8'; }
repeat(){ n=$1; ch=$2; i=0; out=""; while [ "$i" -lt "$n" ]; do out="$out$ch"; i=$((i+1)); done; printf "%s" "$out"; }
term_width(){ w=80; if have tput; then w=$(tput cols 2>/dev/null || echo 80); fi; [ "$w" -gt 24 ] || w=80; echo "$w"; }

if is_utf; then FIL="█"; EMP="░"; else FIL="#"; EMP="-"; fi

CURSOR_HIDE=""; CURSOR_SHOW=""
if [ "$TTY" -eq 1 ] && have tput; then
  CURSOR_HIDE="$(tput civis 2>/dev/null || true)"
  CURSOR_SHOW="$(tput cnorm 2>/dev/null || true)"
fi
finish_ui(){ [ "$TTY" -eq 1 ] && printf "\r\033[K\n%s" "$CURSOR_SHOW"; }
trap 'finish_ui; cleanup_tmp' EXIT INT TERM

[ "$TTY" -eq 1 ] && printf "%s" "$CURSOR_HIDE"

TOTAL=9
[ "$PURGE_USER" -eq 1 ] && TOTAL=$((TOTAL+1))
[ "$PURGE_REPO" -eq 1 ] && TOTAL=$((TOTAL+1))
STEP=0

draw_bar(){
  n=$1; tot=$2; msg=$3
  [ "$TTY" -eq 1 ] || return 0
  tw=$(term_width); bw=$(( tw - 32 )); [ "$bw" -lt 12 ] && bw=12; [ "$bw" -gt 60 ] && bw=60
  [ "$tot" -gt 0 ] || tot=1
  pct=$(( n*100 / tot )); fill=$(( n*bw / tot )); empty=$(( bw - fill ))
  bar="$(repeat "$fill" "$FIL")$(repeat "$empty" "$EMP")"
  printf "\r\033[K%s%s[%s]%s %d/%d (%d%%) %s" "$CYAN" "$BOLD" "$bar" "$RESET" "$n" "$tot" "$pct" "$msg"
  log_line "$(printf '[%s] %d/%d (%d%%) %s' "$(repeat "$fill" "#")$(repeat "$empty" "-")" "$n" "$tot" "$pct" "$msg")"
}
next(){ STEP=$((STEP+1)); [ "$STEP" -gt "$TOTAL" ] && STEP="$TOTAL"; draw_bar "$STEP" "$TOTAL" "$1"; }

# -----------------------------
# privileges
# -----------------------------
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if have sudo; then SUDO="sudo"
  elif have doas; then SUDO="doas"
  fi
fi

auth_once(){
  [ -z "$SUDO" ] && return 0
  # clear line so prompt doesn't fight the bar
  [ "$TTY" -eq 1 ] && printf "\r\033[K"
  info "Elevating privileges (may prompt once)..."
  if [ "$SUDO" = "sudo" ]; then sudo -v
  else $SUDO true
  fi
}

# -----------------------------
# package-ownership check (best-effort)
# -----------------------------
pkg_owner(){
  p="$1"
  # returns owner string or empty
  if have pacman; then pacman -Qo "$p" 2>/dev/null | sed 's/^/pacman: /' && return 0; fi
  if have dpkg; then dpkg -S "$p" 2>/dev/null | sed 's/^/dpkg: /' && return 0; fi
  if have rpm; then rpm -qf "$p" 2>/dev/null | sed 's/^/rpm: /' && return 0; fi
  if have apk; then apk info -W "$p" 2>/dev/null | sed 's/^/apk: /' && return 0; fi
  if have xbps-query; then xbps-query -o "$p" 2>/dev/null | sed 's/^/xbps: /' && return 0; fi
  if have pkg; then pkg which -q "$p" 2>/dev/null | sed 's/^/pkg: /' && return 0; fi
  return 1
}

# -----------------------------
# safe removers
# -----------------------------
is_system_path(){
  case "$1" in
    /bin/*|/sbin/*|/lib/*|/lib64/*|/usr/*|/etc/*|/opt/*|/var/*) return 0 ;;
    *) return 1 ;;
  esac
}

rm_path(){
  p="$1"
  [ -e "$p" ] || return 0

  if [ "$DRY_RUN" -eq 1 ]; then
    say "DRY-RUN: would remove $p"
    return 0
  fi

  # if package-owned and system path, skip unless forced
  if is_system_path "$p" && [ "$FORCE" -eq 0 ]; then
    if owner="$(pkg_owner "$p" 2>/dev/null || true)"; [ -n "${owner:-}" ]; then
      warn "Skipping package-owned file: $p ($owner). Use your package manager, or rerun with --force."
      log_line "SKIP(pkg-owned): $p ($owner)"
      return 0
    fi
  fi

  # writable -> normal rm; otherwise root rm if possible
  if [ -w "$p" ] || [ "$(id -u)" -eq 0 ]; then
    rm -f -- "$p" >>"$LOG" 2>&1 || :
    log_line "RM: $p"
    return 0
  fi

  if [ -n "$SUDO" ]; then
    $SUDO rm -f -- "$p" >>"$LOG" 2>&1 || :
    log_line "RM(root): $p"
    return 0
  fi

  warn "No sudo/doas, cannot remove: $p"
  log_line "FAIL(no-priv): $p"
  return 0
}

rm_dir(){
  d="$1"
  [ -d "$d" ] || return 0

  if [ "$DRY_RUN" -eq 1 ]; then
    say "DRY-RUN: would remove dir $d"
    return 0
  fi

  if [ -w "$d" ] || [ "$(id -u)" -eq 0 ]; then
    rm -rf -- "$d" >>"$LOG" 2>&1 || :
    log_line "RMDIR: $d"
    return 0
  fi

  if [ -n "$SUDO" ]; then
    $SUDO rm -rf -- "$d" >>"$LOG" 2>&1 || :
    log_line "RMDIR(root): $d"
    return 0
  fi

  warn "No sudo/doas, cannot remove dir: $d"
  log_line "FAIL(no-priv): $d"
  return 0
}

# -----------------------------
# plan builder
# format: TYPE<TAB>PATH
# -----------------------------
plan_add(){
  t="$1"; p="$2"
  [ -n "$p" ] || return 0
  printf "%s\t%s\n" "$t" "$p" >>"$PLAN"
}

plan_unique(){
  # stable unique
  awk '!seen[$0]++' "$PLAN" >"$PLANU"
}

plan_show(){
  plan_unique
  info "Removal plan:"
  awk -F '\t' '
    { items[$1] = items[$1] "\n  - " $2; count[$1]++ }
    END {
      for (t in count) {
        printf "  %s (%d):%s\n", t, count[t], items[t]
      }
    }
  ' "$PLANU"
}

confirm(){
  prompt="$1"
  [ "$ASSUME_YES" -eq 1 ] && return 0
  [ "$TTY" -eq 1 ] || return 0
  printf "\r\033[K%s%s?%s %s [y/N] " "$BOLD" "" "$RESET" "$prompt" 1>&2
  read -r A || A=""
  case "$A" in y|Y|yes|YES) return 0 ;; *) return 1 ;; esac
}

# -----------------------------
# locate script dir (repo purge safety)
# -----------------------------
SCRIPT_DIR=$(
  CDPATH= cd -P -- "$(dirname -- "$0")" 2>/dev/null && pwd
)
REPO_DIR=$(CDPATH= cd -P -- "$SCRIPT_DIR/.." 2>/dev/null && pwd)

# -----------------------------
# CLEAN profile helper
# -----------------------------
clean_profile_file(){
  prof="$1"
  [ -f "$prof" ] || return 0
  tmp="${prof}.tmp.$$"

  awk '
    BEGIN { skip=0 }
    /^# Added by tedit installer$/ { skip=2; next }
    /^# Added by tedit updater$/   { skip=2; next }
    /^# Added by tedit init$/      { skip=2; next }
    { if (skip>0) { skip--; next } else { print } }
  ' "$prof" >"$tmp" 2>/dev/null || { rm -f "$tmp" 2>/dev/null || :; return 0; }

  # only replace if changed
  if cmp -s "$prof" "$tmp" 2>/dev/null; then
    rm -f "$tmp" 2>/dev/null || :
  else
    mv "$tmp" "$prof" 2>/dev/null || rm -f "$tmp" 2>/dev/null || :
    log_line "CLEAN_PROFILE: $prof"
  fi
}

# -----------------------------
# Discover targets (aggressive)
# -----------------------------
info ">> Uninstalling $APP_NAME <<"
say "Log: $LOG"

auth_once || true
next "Scanning install locations"

# binaries + wrappers: scan PATH + common dirs
scan_name(){
  name="$1"
  # command -v target
  cv="$(command -v "$name" 2>/dev/null || true)"
  [ -n "$cv" ] && [ -f "$cv" ] && plan_add "bin" "$cv"

  # common dirs
  for d in /usr/local/bin /usr/bin /bin "$HOME/.local/bin"; do
    [ -f "$d/$name" ] && plan_add "bin" "$d/$name"
  done

  # PATH scan (duplicates ok; uniqued later)
  oldifs=$IFS; IFS=:
  for d in $PATH; do
    [ -n "$d" ] || continue
    [ -f "$d/$name" ] && plan_add "bin" "$d/$name"
  done
  IFS=$oldifs
}

scan_name "$APP_NAME"
for w in tedit-install tedit-update tedit-uninstall; do
  scan_name "$w"
done

# man pages (common + compression variants)
add_man_variants(){
  base="$1"
  plan_add "man" "$base"
  plan_add "man" "${base}.gz"
  plan_add "man" "${base}.bz2"
  plan_add "man" "${base}.xz"
}

for mp in \
  "/usr/local/share/man/man1/${APP_NAME}.1" \
  "/usr/share/man/man1/${APP_NAME}.1" \
  "$HOME/.local/share/man/man1/${APP_NAME}.1"
do
  add_man_variants "$mp"
done

# markers + metadata dirs
plan_add "marker" "/usr/local/share/tedit"
plan_add "marker" "/usr/share/tedit"
plan_add "marker" "$HOME/.local/share/tedit"

# completions (nice cleanup)
for c in \
  "/usr/share/bash-completion/completions/${APP_NAME}" \
  "/usr/local/share/bash-completion/completions/${APP_NAME}" \
  "$HOME/.local/share/bash-completion/completions/${APP_NAME}" \
  "/usr/share/zsh/site-functions/_${APP_NAME}" \
  "/usr/local/share/zsh/site-functions/_${APP_NAME}" \
  "$HOME/.local/share/zsh/site-functions/_${APP_NAME}"
do
  plan_add "completion" "$c"
done

plan_unique

# If nothing found, still clean PATH lines + optional purge if requested
FOUND_ANY=0
if [ -s "$PLANU" ]; then FOUND_ANY=1; fi

if [ "$FOUND_ANY" -eq 1 ]; then
  plan_show
  if [ "$DRY_RUN" -eq 0 ] && [ "$ASSUME_YES" -eq 0 ] && [ "$TTY" -eq 1 ]; then
    if ! confirm "Proceed with uninstall"; then
      finish_ui
      warn "Cancelled."
      say "Log: $LOG"
      exit 1
    fi
  fi
else
  warn "No installed $APP_NAME files found in common locations/PATH. Will still clean profile lines + optional purges."
fi

# -----------------------------
# Execute removals
# -----------------------------
next "Removing binaries & wrappers"
awk -F '\t' '$1=="bin"{print $2}' "$PLANU" | while IFS= read -r p; do rm_path "$p"; done

next "Removing man pages"
awk -F '\t' '$1=="man"{print $2}' "$PLANU" | while IFS= read -r p; do rm_path "$p"; done

next "Removing markers"
awk -F '\t' '$1=="marker"{print $2}' "$PLANU" | while IFS= read -r d; do rm_dir "$d"; done

next "Removing shell completions"
awk -F '\t' '$1=="completion"{print $2}' "$PLANU" | while IFS= read -r p; do rm_path "$p"; done

next "Cleaning PATH injections"
# common profile targets (zsh/bash/sh)
for prof in \
  "${ZDOTDIR-}/.zprofile" \
  "$HOME/.zprofile" \
  "$HOME/.bash_profile" \
  "$HOME/.profile"
do
  [ -n "$prof" ] || continue
  clean_profile_file "$prof"
done

next "Cleaning local build artifacts (if this is the repo)"
# only touch obvious build outputs; do NOT delete repo unless --purge-repo
if [ -d "$REPO_DIR" ] && [ -f "$REPO_DIR/src/tedit.cpp" ]; then
  if [ "$DRY_RUN" -eq 1 ]; then
    say "DRY-RUN: would clean build outputs in $REPO_DIR"
  else
    (
      cd "$REPO_DIR" 2>/dev/null || exit 0
      # best-effort "make clean"
      if [ -f Makefile ]; then
        if have gmake; then gmake clean >/dev/null 2>&1 || :
        elif have make; then make clean >/dev/null 2>&1 || :
        fi
      fi
      rm -f "./${APP_NAME}" ./*.o ./*.obj ./*.d ./*.a ./*.so 2>/dev/null || :
      rm -rf ./build ./out ./dist ./CMakeFiles ./*.dSYM 2>/dev/null || :
      rm -f ./CMakeCache.txt ./compile_commands.json 2>/dev/null || :
    ) >>"$LOG" 2>&1 || :
  fi
fi

next "Refreshing man database (best-effort)"
if [ "$DRY_RUN" -eq 1 ]; then
  say "DRY-RUN: would refresh man database"
else
  if have mandb; then
    if [ -n "$SUDO" ]; then $SUDO -n mandb -q >>"$LOG" 2>&1 || :; else mandb -q >>"$LOG" 2>&1 || :; fi
  elif have makewhatis; then
    if [ -n "$SUDO" ]; then $SUDO -n makewhatis >>"$LOG" 2>&1 || :; else makewhatis >>"$LOG" 2>&1 || :; fi
  fi
fi

if [ "$PURGE_USER" -eq 1 ]; then
  next "Purging user data"
  rm_path "$HOME/.teditrc"
  rm_path "$HOME/.tedit_banner"
  rm_dir  "$HOME/.tedit"
  rm_dir  "$HOME/tedit-config"
  rm_dir  "$HOME/.config/tedit"
  rm_dir  "$HOME/.cache/tedit"
  rm_dir  "$HOME/.local/state/tedit"

  # recover files (safe loop)
  for f in "$HOME"/.tedit-recover-*; do
    [ -e "$f" ] && rm_path "$f"
  done
fi

next "Post-check (leftovers?)"
LEFT="$(command -v "$APP_NAME" 2>/dev/null || true)"
if [ -n "$LEFT" ] && [ -f "$LEFT" ]; then
  warn "Still found '$APP_NAME' on PATH: $LEFT"
  if [ "$FORCE" -eq 0 ]; then
    warn "If that's a package-managed install, uninstall via your package manager."
  fi
  finish_ui
  say "Log: $LOG"
  exit 1
fi

if [ "$PURGE_REPO" -eq 1 ]; then
  next "Purging repository directory"
  if [ -d "$REPO_DIR/.git" ] && [ -f "$REPO_DIR/src/tedit.cpp" ]; then
    # safety rails: don't nuke / or ~ by accident
    case "$REPO_DIR" in
      "/"|"$HOME"|"/home"|"/root") warn "Refusing to purge suspicious dir: $REPO_DIR";;
      *)
        if [ "$ASSUME_YES" -eq 1 ] || confirm "Delete repo dir '$REPO_DIR' (cannot be undone)"; then
          rm_dir "$REPO_DIR"
        else
          warn "Skipped repo deletion."
        fi
        ;;
    esac
  else
    warn "This script directory doesn't look like the tedit repo; skipping --purge-repo."
  fi
fi

finish_ui
ok "$APP_NAME uninstalled successfully :D"
say "Tip: restart your shell or run: hash -r"
say "Log: $LOG"
