# Shared support for the checkout-based tools. install.sh stays self-contained
# so the public curl | sh entry point needs only one download.

tedit_have() { command -v "$1" >/dev/null 2>&1; }
tedit_say() { printf '%s\n' "$*"; printf '%s\n' "$*" >>"$TEDIT_LOG"; }
tedit_die() { tedit_say "ERROR: $*" >&2; tedit_say "Log: $TEDIT_LOG" >&2; exit 1; }
tedit_quote() { printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\\\\''/g")"; }

tedit_setup() {
  TEDIT_START=$(pwd -P)
  TEDIT_UID=$(id -u)
  TEDIT_VERBOSE=${VERBOSE:-0}
  TEDIT_ELEVATE=
  TEDIT_AUTH=0
  TEDIT_PENDING=
  TEDIT_WORK=$(mktemp -d "${TMPDIR:-/tmp}/tedit-$1.XXXXXX") || exit 1
  TEDIT_WORK=$(CDPATH= cd -P -- "$TEDIT_WORK" && pwd)
  trap tedit_cleanup 0
  trap 'exit 129' HUP
  trap 'exit 130' INT
  trap 'exit 143' TERM
  if [ -n "${LOG_FILE:-}" ]; then TEDIT_LOG=$LOG_FILE
  else TEDIT_LOG=$(mktemp "${TMPDIR:-/tmp}/tedit-$1.log.XXXXXX") || exit 1
  fi
  case "$TEDIT_LOG" in /*) ;; *) TEDIT_LOG=$TEDIT_START/$TEDIT_LOG ;; esac
  : >>"$TEDIT_LOG" || { printf 'ERROR: cannot write log: %s\n' "$TEDIT_LOG" >&2; exit 1; }
}

tedit_cleanup() {
  if [ -n "$TEDIT_PENDING" ]; then tedit_root rm -f -- "$TEDIT_PENDING" || :; fi
  rm -rf -- "$TEDIT_WORK"
}

tedit_run() {
  if "$@" </dev/null >"$TEDIT_WORK/output" 2>&1; then
    cat "$TEDIT_WORK/output" >>"$TEDIT_LOG"
    if [ "$TEDIT_VERBOSE" = 1 ]; then cat "$TEDIT_WORK/output"; fi
  else
    tedit_status=$?
    cat "$TEDIT_WORK/output" >>"$TEDIT_LOG"
    cat "$TEDIT_WORK/output" >&2
    return "$tedit_status"
  fi
}

tedit_authenticate() {
  [ "$TEDIT_UID" -ne 0 ] || return 0
  [ "$TEDIT_AUTH" -eq 0 ] || return 0
  if tedit_have sudo; then
    TEDIT_ELEVATE=sudo
    tedit_say ':: Authenticating with sudo' >&2
    sudo -v </dev/null || tedit_die 'sudo authentication failed'
  elif tedit_have doas; then TEDIT_ELEVATE=doas
  else tedit_die 'This path needs root. Use sudo/doas or choose a writable destination.'
  fi
  TEDIT_AUTH=1
}

tedit_root() {
  case "$TEDIT_ELEVATE" in
    sudo) sudo -n "$@" </dev/null ;;
    doas) doas "$@" </dev/null ;;
    *) "$@" </dev/null ;;
  esac
}

tedit_needs_root() {
  [ "$TEDIT_UID" -ne 0 ] || return 1
  tedit_parent=$(dirname -- "$1")
  while [ ! -e "$tedit_parent" ]; do tedit_parent=$(dirname -- "$tedit_parent"); done
  [ ! -w "$tedit_parent" ]
}

# The destination is separate from the argv; never build a command string.
tedit_write() {
  tedit_destination=$1
  shift
  if tedit_needs_root "$tedit_destination"; then
    tedit_authenticate
    tedit_run tedit_root "$@"
  else tedit_run "$@"
  fi
}

# Place wrappers and markers via rename in their destination filesystem.
tedit_place() {
  tedit_src=$1
  tedit_dst=$2
  tedit_mode=$3
  [ ! -d "$tedit_dst" ] || tedit_die "Refusing to replace a directory: $tedit_dst"
  tedit_dir=$(dirname -- "$tedit_dst")
  tedit_write "$tedit_dst" mkdir -p "$tedit_dir" || return $?
  if tedit_needs_root "$tedit_dst"; then
    tedit_authenticate
    TEDIT_PENDING=$(tedit_root mktemp "$tedit_dir/.tedit.XXXXXX") || return $?
  else TEDIT_PENDING=$(mktemp "$tedit_dir/.tedit.XXXXXX") || return $?
  fi
  if [ "$#" -ge 4 ]; then
    # Preserve profile ownership and metadata, including when called as root.
    tedit_write "$tedit_dst" cp -p "$4" "$TEDIT_PENDING" || return $?
    tedit_write "$tedit_dst" chmod u+w "$TEDIT_PENDING" || return $?
    tedit_write "$tedit_dst" sh -c 'cat "$1" >"$2"' sh "$tedit_src" "$TEDIT_PENDING" || return $?
    tedit_write "$tedit_dst" chmod "$tedit_mode" "$TEDIT_PENDING" || return $?
  else
    tedit_write "$tedit_dst" install -m "$tedit_mode" "$tedit_src" "$TEDIT_PENDING" || return $?
  fi
  tedit_write "$tedit_dst" mv -f -- "$TEDIT_PENDING" "$tedit_dst" || return $?
  TEDIT_PENDING=
}

tedit_source_exists() {
  [ -f "$1/src/tedit.cpp" ] && [ -f "$1/Makefile" ] && [ -f "$1/scripts/install.sh" ]
}

tedit_find_repo() {
  if [ -n "$1" ]; then
    tedit_source_exists "$1" || return 1
    (CDPATH= cd -P -- "$1" && pwd)
    return
  fi
  for tedit_repo in "$SCRIPT_DIR/.." "$TEDIT_START"; do
    if tedit_source_exists "$tedit_repo"; then (CDPATH= cd -P -- "$tedit_repo" && pwd); return; fi
  done
  for tedit_marker in "${XDG_DATA_HOME:-${HOME:-}/.local/share}/tedit/repo" /usr/local/share/tedit/repo; do
    [ -r "$tedit_marker" ] || continue
    IFS= read -r tedit_repo <"$tedit_marker" || continue
    if tedit_source_exists "$tedit_repo"; then (CDPATH= cd -P -- "$tedit_repo" && pwd); return; fi
  done
  return 1
}

tedit_absolute() {
  case "$1" in /*) printf '%s\n' "$1" ;; *) printf '%s/%s\n' "$TEDIT_START" "$1" ;; esac
}

tedit_profile() {
  case "${SHELL:-}" in
    */zsh) printf '%s/.zprofile\n' "${ZDOTDIR:-$HOME}" ;;
    */bash)
      if [ -f "$HOME/.bash_profile" ]; then printf '%s/.bash_profile\n' "$HOME"
      elif [ -f "$HOME/.bash_login" ]; then printf '%s/.bash_login\n' "$HOME"
      else printf '%s/.profile\n' "$HOME"
      fi ;;
    */fish) return 1 ;;
    *) printf '%s/.profile\n' "$HOME" ;;
  esac
}

tedit_add_path() {
  case ":$PATH:" in *":$1:"*) return 0 ;; esac
  tedit_path_line="export PATH=$(tedit_quote "$1"):\"\$PATH\""
  if [ "$TEDIT_UID" -ne 0 ] && [ -n "${HOME:-}" ]; then
    if tedit_prof=$(tedit_profile); then
      if ! grep -Fqx "$tedit_path_line" "$tedit_prof" 2>/dev/null; then
        if mkdir -p "$(dirname -- "$tedit_prof")" && printf '\n# Added by tedit %s\n%s\n' "$2" "$tedit_path_line" >>"$tedit_prof"; then
          tedit_say "Added $1 to PATH in $tedit_prof"
        else tedit_say "Warning: could not update $tedit_prof; add $1 to PATH manually."
        fi
      fi
    fi
  fi
  case "${SHELL:-}" in
    */fish) tedit_say "Add to PATH with: fish_add_path $(tedit_quote "$1")" ;;
    *) tedit_say "Add to PATH for this shell: $tedit_path_line" ;;
  esac
}
