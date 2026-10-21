#!/usr/bin/env sh
set -eu

usage() {
  cat <<'EOF'
Usage: sh scripts/uninstall.sh [options]
  --prefix PATH      Remove this installation (also accepts PREFIX)
  --system           Remove the /usr/local installation
  --user             Remove the ~/.local installation
  --bin-dir PATH     Also remove generated wrappers in this directory
  --repo PATH        Source checkout to purge (also accepts TEDIT_REPO)
  --purge-user-data  Remove tedit configuration, plugins and recovery files
  --purge-repo       Remove the source checkout; requires confirmation or --yes
  --purge            Purge both user data and the source checkout
  --force            Allow removal of package-owned files/foreign wrappers
  --dry-run          Show changes without modifying installed files or profiles
  -y, --yes          Confirm repository deletion without a prompt
  -h, --help         Show help
Default prefix: the tedit executable on PATH, or /usr/local for root, ~/.local otherwise.
EOF
}

TARGET_PREFIX=${PREFIX:-}
WRAPPER_BIN=${TEDIT_WRAPPER_BIN:-}
REPO_DIR=${TEDIT_REPO:-}
PURGE_USER=0
PURGE_REPO=0
YES=0
DRY_RUN=0
FORCE=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --prefix|--bin-dir|--repo)
      option=$1
      shift
      [ "$#" -gt 0 ] && [ -n "$1" ] || { printf 'ERROR: %s needs a path\n' "$option" >&2; exit 1; }
      case "$option" in
        --prefix) TARGET_PREFIX=$1 ;;
        --bin-dir) WRAPPER_BIN=$1 ;;
        --repo) REPO_DIR=$1 ;;
      esac ;;
    --system) TARGET_PREFIX=/usr/local ;;
    --user) TARGET_PREFIX=${HOME:?HOME is required for --user}/.local ;;
    --purge-user-data) PURGE_USER=1 ;;
    --purge-repo) PURGE_REPO=1 ;;
    --purge) PURGE_USER=1; PURGE_REPO=1 ;;
    --force) FORCE=1 ;;
    --dry-run) DRY_RUN=1 ;;
    -y|--yes) YES=1 ;;
    -h|--help) usage; exit 0 ;;
    *) printf 'ERROR: unknown option: %s\n' "$1" >&2; exit 1 ;;
  esac
  shift
done
SCRIPT_DIR=$(CDPATH= cd -P -- "$(dirname -- "$0")" && pwd)
# shellcheck source=scripts/common.sh
. "$SCRIPT_DIR/common.sh"
tedit_setup uninstall
if [ -z "$TARGET_PREFIX" ]; then
  installed=$(command -v tedit 2>/dev/null || :)
  case "$installed" in
    /*/bin/tedit) TARGET_PREFIX=$(dirname -- "$(dirname -- "$installed")") ;;
    *)
      if [ "$TEDIT_UID" -eq 0 ]; then TARGET_PREFIX=/usr/local
      else TARGET_PREFIX=${HOME:?HOME is required for a user uninstall}/.local
      fi ;;
  esac
fi
TARGET_PREFIX=$(tedit_absolute "$TARGET_PREFIX")
WRAPPER_BIN=$(tedit_absolute "${WRAPPER_BIN:-$TARGET_PREFIX/bin}")
case "${TEDIT_WRAPPER_MARKER:-}" in
  ''|*/tedit/repo) ;;
  *) tedit_die 'Invalid wrapper marker path.' ;;
esac

# Validate and confirm a repository purge before removing any installed files.
if [ "$PURGE_REPO" -eq 1 ]; then
  REPO_DIR=$(tedit_find_repo "$REPO_DIR") || tedit_die 'Cannot locate the source checkout to purge.'
  [ -d "$REPO_DIR/.git" ] || [ -f "$REPO_DIR/.git" ] || tedit_die 'Refusing to purge a directory without Git metadata.'
  case "$REPO_DIR" in
    /|/home|/root|/usr|/usr/local|/opt|/tmp|/var) tedit_die "Refusing to purge $REPO_DIR" ;;
  esac
  if [ -n "${HOME:-}" ]; then
    home_dir=$(CDPATH= cd -P -- "$HOME" && pwd)
    [ "$REPO_DIR" != "$home_dir" ] || tedit_die 'Refusing to purge HOME.'
  fi
  case "$TEDIT_LOG" in "$REPO_DIR"/*) tedit_die 'The uninstall log must be outside the checkout being purged.' ;; esac
  if [ "$DRY_RUN" -eq 0 ] && [ "$YES" -eq 0 ]; then
    [ -t 0 ] || tedit_die 'Repository deletion needs --yes in a non-interactive session.'
    printf 'Delete source checkout %s permanently? [y/N] ' "$REPO_DIR" >&2
    IFS= read -r answer || answer=
    case "$answer" in y|Y|yes|YES) ;; *) tedit_die 'Repository purge cancelled.' ;; esac
  fi
fi
if [ "$PURGE_USER" -eq 1 ]; then
  : "${HOME:?HOME is required to purge user data}"
  home_dir=$(CDPATH= cd -P -- "$HOME" && pwd)
  [ "$home_dir" != / ] || tedit_die 'Refusing to purge user data with HOME=/.'
fi

tedit_say ":: Removing tedit from $TARGET_PREFIX"
tedit_say "Log: $TEDIT_LOG"
FAILURES=0
package_owned() {
  if tedit_have dpkg-query && dpkg-query -S "$1" >"$TEDIT_WORK/owner" 2>/dev/null; then return 0; fi
  if tedit_have rpm && rpm -qf "$1" >"$TEDIT_WORK/owner" 2>/dev/null; then return 0; fi
  if tedit_have pacman && pacman -Qo "$1" >"$TEDIT_WORK/owner" 2>/dev/null; then return 0; fi
  if tedit_have apk && apk info -W "$1" >"$TEDIT_WORK/owner" 2>/dev/null; then return 0; fi
  if tedit_have xbps-query && xbps-query -o "$1" >"$TEDIT_WORK/owner" 2>/dev/null; then return 0; fi
  if tedit_have pkg && pkg which -q "$1" >"$TEDIT_WORK/owner" 2>/dev/null; then return 0; fi
  return 1
}

remove_file() {
  [ -e "$1" ] || [ -L "$1" ] || return 0
  if [ -d "$1" ] && [ ! -L "$1" ]; then
    tedit_say "Cannot remove unexpected directory: $1"
    FAILURES=1
    return
  fi
  if [ "$FORCE" -eq 0 ] && package_owned "$1"; then
    tedit_say "Skipping package-owned file: $1; use its package manager or --force."
    FAILURES=1
    return
  fi
  if [ "$DRY_RUN" -eq 1 ]; then tedit_say "DRY-RUN: would remove $1"; return; fi
  if tedit_write "$1" rm -f -- "$1"; then tedit_say "Removed $1"
  else FAILURES=1
  fi
}

remove_dir() {
  [ -e "$1" ] || [ -L "$1" ] || return 0
  if [ "$DRY_RUN" -eq 1 ]; then tedit_say "DRY-RUN: would remove $1"; return; fi
  if tedit_write "$1" rm -rf -- "$1"; then tedit_say "Removed $1"
  else FAILURES=1
  fi
}

remove_file "$TARGET_PREFIX/bin/tedit"
for wrapper_dir in "$TARGET_PREFIX/bin" "$WRAPPER_BIN"; do
  for name in tedit-install tedit-update tedit-uninstall; do
    wrapper=$wrapper_dir/$name
    [ -e "$wrapper" ] || [ -L "$wrapper" ] || continue
    if [ "$FORCE" -eq 0 ] && ! grep -q '^# Generated by tedit init' "$wrapper"; then
      tedit_say "Skipping unrecognized wrapper: $wrapper"
      FAILURES=1
      continue
    fi
    remove_file "$wrapper"
  done
done
for suffix in '' .gz .bz2 .xz; do remove_file "$TARGET_PREFIX/share/man/man1/tedit.1$suffix"; done
remove_file "$TARGET_PREFIX/share/tedit/repo"
remove_file "$TARGET_PREFIX/share/tedit/build-revision"
if [ -n "${TEDIT_WRAPPER_MARKER:-}" ]; then remove_file "$TEDIT_WRAPPER_MARKER"; fi
if [ -n "${HOME:-}" ] && [ "$TARGET_PREFIX" = "$HOME/.local" ]; then
  remove_file "${XDG_DATA_HOME:-$HOME/.local/share}/tedit/repo"
fi

clean_profile() {
  [ -f "$1" ] || return 0
  # Resolve profile symlinks so an atomic replacement preserves the link itself.
  profile_file=$1
  link_count=0
  while [ -L "$profile_file" ]; do
    link_count=$((link_count + 1))
    [ "$link_count" -lt 40 ] || tedit_die 'Too many profile symlinks.'
    link_target=$(readlink "$profile_file") || tedit_die "Cannot read $profile_file"
    case "$link_target" in
      /*) profile_file=$link_target ;;
      *) profile_file=$(dirname -- "$profile_file")/$link_target ;;
    esac
  done
  for path_bin in "$TARGET_PREFIX/bin" "$WRAPPER_BIN"; do
    TEDIT_NEW_PATH="export PATH=$(tedit_quote "$path_bin"):\"\$PATH\"" \
      TEDIT_OLD_PATH="export PATH=\"$path_bin:\$PATH\"" \
      awk '
        /^# Added by tedit (installer|updater|init)$/ {
          if (pending != "") print pending
          pending = $0
          next
        }
        {
          if (pending != "") {
            if ($0 == ENVIRON["TEDIT_NEW_PATH"] || $0 == ENVIRON["TEDIT_OLD_PATH"]) {
              pending = ""
              next
            }
            print pending
            pending = ""
          }
          print
        }
        END { if (pending != "") print pending }
      ' "$profile_file" >"$TEDIT_WORK/profile" || tedit_die "Cannot read $profile_file"
    if ! cmp -s "$profile_file" "$TEDIT_WORK/profile"; then
      if [ "$DRY_RUN" -eq 1 ]; then tedit_say "DRY-RUN: would clean PATH in $profile_file"; continue; fi
      profile_mode=$(stat -c %a "$profile_file" 2>/dev/null || stat -f %Lp "$profile_file") || tedit_die 'Cannot read profile permissions.'
      tedit_place "$TEDIT_WORK/profile" "$profile_file" "$profile_mode" "$profile_file" || tedit_die "Cannot update $profile_file"
      tedit_say "Cleaned PATH in $profile_file"
    fi
  done
}
if [ -n "${HOME:-}" ]; then
  for profile in "$HOME/.profile" "$HOME/.bash_profile" "$HOME/.bash_login" "$HOME/.zprofile"; do clean_profile "$profile"; done
  if [ -n "${ZDOTDIR:-}" ]; then clean_profile "$ZDOTDIR/.zprofile"; fi
fi

if [ "$PURGE_USER" -eq 1 ]; then
  remove_file "$HOME/.teditrc"
  remove_file "$HOME/.tedit_banner"
  for user_dir in "$HOME/.tedit" "$HOME/tedit-config" \
    "${XDG_CONFIG_HOME:-$HOME/.config}/tedit" "${XDG_CACHE_HOME:-$HOME/.cache}/tedit" \
    "${XDG_STATE_HOME:-$HOME/.local/state}/tedit"; do remove_dir "$user_dir"; done
  for recovery in "$HOME"/.tedit-recover-*; do remove_file "$recovery"; done
fi
if [ "$PURGE_REPO" -eq 1 ]; then
  [ "$FAILURES" -eq 0 ] || tedit_die 'Uninstall is incomplete; keeping the source checkout.'
  remove_dir "$REPO_DIR"
fi
[ "$FAILURES" -eq 0 ] || tedit_die 'Some files could not be removed.'
if [ "$DRY_RUN" -eq 1 ]; then tedit_say 'Dry run complete.'
else tedit_say 'tedit uninstalled. Run hash -r or restart your shell.'
fi
