#!/usr/bin/env sh
# tedit uninstaller (POSIX sh)
# Safely removes the tedit binary, manpage, and PATH entries added by the installer.

set -eu

APP_NAME="tedit"
LOG="$(mktemp -t ${APP_NAME}-uninstall.XXXXXX.log)"

# --- Colors (TTY-aware) ---
if [ -t 1 ] && command -v tput >/dev/null 2>&1; then
  GREEN="$(tput setaf 2)"; YELLOW="$(tput setaf 3)"
  RED="$(tput setaf 1)"; CYAN="$(tput setaf 6)"
  BOLD="$(tput bold)"; RESET="$(tput sgr0)"
else
  GREEN=""; YELLOW=""; RED=""; CYAN=""; BOLD=""; RESET=""
fi

say()   { printf "%s\n" "$*" | tee -a "$LOG"; }
warn()  { printf "%sWARNING:%s %s\n" "$YELLOW" "$RESET" "$*" | tee -a "$LOG" >&2; }
die()   { printf "%sERROR:%s %s\n" "$RED" "$RESET" "$*" | tee -a "$LOG" >&2; exit 1; }
have()  { command -v "$1" >/dev/null 2>&1; }

# --- Privilege helper ---
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
  if have sudo; then SUDO="sudo"
  elif have doas; then SUDO="doas"
  fi
fi

# --- Progress bar ---
TOTAL=4 STEP=0 WIDTH=36
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

# --- File removal helper ---
remove_file() {
  f="$1"
  [ -e "$f" ] || return 0
  if [ -w "$f" ] || [ -n "$SUDO" ]; then
    ${SUDO:+$SUDO }rm -f "$f" 2>/dev/null || true
    say "🗑️  Removed: $f"
  else
    warn "No permission to remove $f (and no sudo/doas)."
  fi
}

# --- Begin ---
say "${CYAN}${BOLD}Uninstalling ${APP_NAME}...${RESET}"

FOUND=0

progress "Removing binaries..."
for d in $(printf "%s\n" "$PATH" | tr ':' '\n') /usr/local/bin /usr/bin "$HOME/.local/bin"; do
  [ -n "$d" ] || continue
  f="$d/$APP_NAME"
  if [ -x "$f" ] && [ ! -d "$f" ]; then
    remove_file "$f"
    FOUND=1
  fi
done
sleep 0.1

progress "Removing man pages..."
for mp in \
  "/usr/local/share/man/man1/${APP_NAME}.1" \
  "/usr/share/man/man1/${APP_NAME}.1" \
  "$HOME/.local/share/man/man1/${APP_NAME}.1"; do
  [ -f "$mp" ] && remove_file "$mp" && FOUND=1
  [ -f "${mp}.gz" ] && remove_file "${mp}.gz" && FOUND=1
done
sleep 0.1

progress "Cleaning PATH entries..."
clean_profile() {
  prof="$1"
  [ -f "$prof" ] || return 0
  tmp="${prof}.tmp.$$"
  awk '
    BEGIN { skip=0 }
    /^# Added by tedit installer$/ { skip=2; next }
    { if (skip>0) { skip--; next } else { print } }
  ' "$prof" >"$tmp" && mv "$tmp" "$prof"
}
for prof in "$HOME/.zprofile" "$HOME/.bash_profile" "$HOME/.profile"; do
  clean_profile "$prof"
done
sleep 0.1

progress "Refreshing man database..."
if have mandb; then
  ${SUDO:+$SUDO }mandb -q 2>/dev/null || true
fi

printf "\n"
if [ "$FOUND" -eq 0 ]; then
  say "No ${APP_NAME} installation detected."
else
  say "All known files removed successfully."
fi

echo "${GREEN}${BOLD}✅ ${APP_NAME} uninstalled successfully!${RESET}"
echo "Log: $LOG"
echo "If installed to a custom PREFIX, remove it manually if needed."
