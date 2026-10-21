#!/usr/bin/env sh
# Parse the updater before a Git merge can replace files in this checkout.
main() {
set -eu

usage() {
  cat <<'EOF'
Usage: sh scripts/update.sh [options]
  --repo PATH        Source checkout (also accepts TEDIT_REPO)
  --prefix PATH      Install prefix (also accepts PREFIX; otherwise inferred)
  --system           Install into /usr/local
  --user             Install into ~/.local
  --no-git           Rebuild local source without Git
  --no-pull          Skip fetching/merging and rebuild local source
  --deps-only        Verify/install build dependencies without updating source
  --no-deps          Never install packages
  --no-build         Install the existing build
  --no-install       Update/build source without changing installed files
  --no-man           Skip the man page
  --no-strip         Keep binary debug symbols
  --no-path          Do not edit a shell profile
  --verbose          Show command output
  -h, --help         Show help

Git updates are fast-forward only. Commit or stash local changes before pulling
new commits, or use --no-pull to build the current working tree.
EOF
}

REPO_DIR=${TEDIT_REPO:-}
TARGET_PREFIX=${PREFIX:-}
NO_GIT=0
NO_PULL=0
NO_BUILD=0
NO_INSTALL=0
NO_DEPS=0
DEPS_ONLY=0
NO_MAN=0
NO_STRIP=0
NO_PATH=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --repo|--prefix)
      option=$1
      shift
      [ "$#" -gt 0 ] && [ -n "$1" ] || { printf 'ERROR: %s needs a path\n' "$option" >&2; exit 1; }
      if [ "$option" = --repo ]; then REPO_DIR=$1; else TARGET_PREFIX=$1; fi ;;
    --system) TARGET_PREFIX=/usr/local ;;
    --user) TARGET_PREFIX=${HOME:?HOME is required for --user}/.local ;;
    --no-git) NO_GIT=1 ;;
    --no-pull) NO_PULL=1 ;;
    --no-build) NO_BUILD=1 ;;
    --no-install) NO_INSTALL=1 ;;
    --no-deps) NO_DEPS=1 ;;
    --deps-only) DEPS_ONLY=1 ;;
    --no-man) NO_MAN=1 ;;
    --no-strip) NO_STRIP=1 ;;
    --no-path) NO_PATH=1 ;;
    --verbose) VERBOSE=1 ;;
    -h|--help) usage; return 0 ;;
    *) printf 'ERROR: unknown option: %s\n' "$1" >&2; exit 1 ;;
  esac
  shift
done
SCRIPT_DIR=$(CDPATH= cd -P -- "$(dirname -- "$0")" && pwd)
# shellcheck source=scripts/common.sh
. "$SCRIPT_DIR/common.sh"
tedit_setup update
REPO_DIR=$(tedit_find_repo "$REPO_DIR") || tedit_die 'Cannot locate the checkout. Use --repo or TEDIT_REPO.'
if [ -z "$TARGET_PREFIX" ]; then
  installed=$(command -v tedit 2>/dev/null || :)
  case "$installed" in
    /*/bin/tedit) TARGET_PREFIX=$(dirname -- "$(dirname -- "$installed")") ;;
    *)
      if [ "$TEDIT_UID" -eq 0 ]; then TARGET_PREFIX=/usr/local
      else TARGET_PREFIX=${HOME:?HOME is required for a user install}/.local
      fi ;;
  esac
fi
TARGET_PREFIX=$(tedit_absolute "$TARGET_PREFIX")
cd "$REPO_DIR"
tedit_say ":: Updating tedit in $REPO_DIR"
tedit_say "Log: $TEDIT_LOG"

# Build one argv for the installer. All dependency/build/install flags retain
# their meaning; the updater never repeats package-manager logic.
set -- --repo "$REPO_DIR" --prefix "$TARGET_PREFIX"
if [ "$NO_DEPS" -eq 1 ]; then set -- "$@" --no-deps; fi
if [ "$NO_BUILD" -eq 1 ]; then set -- "$@" --no-build; fi
if [ "$NO_INSTALL" -eq 1 ]; then set -- "$@" --no-install; fi
if [ "$NO_MAN" -eq 1 ]; then set -- "$@" --no-man; fi
if [ "$NO_STRIP" -eq 1 ]; then set -- "$@" --no-strip; fi
if [ "$NO_PATH" -eq 1 ]; then set -- "$@" --no-path; fi
if [ "$TEDIT_VERBOSE" = 1 ]; then set -- "$@" --verbose; fi
if [ "$DEPS_ONLY" -eq 1 ]; then
  if [ "$NO_GIT" -eq 0 ] && [ "$NO_PULL" -eq 0 ] && { [ -d .git ] || [ -f .git ]; }; then set -- "$@" --with-git; fi
  LOG_FILE=$TEDIT_LOG sh "$REPO_DIR/scripts/install.sh" "$@" --deps-only
  return $?
fi

if [ "$NO_GIT" -eq 0 ] && [ "$NO_PULL" -eq 0 ] && { [ -d .git ] || [ -f .git ]; }; then
  if ! tedit_have git; then
    LOG_FILE=$TEDIT_LOG sh "$REPO_DIR/scripts/install.sh" "$@" --deps-only --with-git || tedit_die 'Could not install update dependencies.'
  fi
  git rev-parse --is-inside-work-tree >/dev/null 2>&1 || tedit_die 'Invalid Git checkout.'
  branch=$(git symbolic-ref --quiet --short HEAD) || tedit_die 'Detached HEAD. Check out a branch or use --no-pull.'
  upstream=$(git rev-parse --symbolic-full-name '@{upstream}' 2>/dev/null) || tedit_die 'No upstream configured. Set the branch upstream or use --no-pull.'
  remote=$(git config --get "branch.$branch.remote") || tedit_die 'No upstream remote configured.'
  export GIT_TERMINAL_PROMPT=0
  tedit_say ":: Fetching $remote"
  tedit_run git fetch --prune -- "$remote" || tedit_die 'Git fetch failed; source and installed files were not updated.'
  current=$(git rev-parse HEAD)
  incoming=$(git rev-parse "$upstream")
  installed_revision=
  if [ -r "$TARGET_PREFIX/share/tedit/build-revision" ]; then
    IFS= read -r installed_revision <"$TARGET_PREFIX/share/tedit/build-revision" || :
  fi
  changes=$(git status --porcelain) || tedit_die 'Could not inspect the working tree.'
  if [ "$current" != "$incoming" ]; then
    if git merge-base --is-ancestor HEAD "$upstream"; then
      [ -z "$changes" ] || tedit_die 'Local changes are present. Commit/stash them before updating, or use --no-pull to rebuild them.'
      tedit_run git merge --ff-only -- "$upstream" || tedit_die 'Git fast-forward failed.'
    elif git merge-base --is-ancestor "$upstream" HEAD; then
      tedit_say ':: Local branch is ahead of upstream; building the current checkout.'
    else tedit_die 'Local and upstream history diverged. Resolve the branch before updating, or use --no-pull.'
    fi
  elif [ -z "$changes" ] && [ "$installed_revision" = "$current" ] && [ -x "$TARGET_PREFIX/bin/tedit" ] && [ "$NO_BUILD" -eq 0 ] && [ "$NO_INSTALL" -eq 0 ]; then
    tedit_say 'Up to date.'
    return 0
  fi
fi
if [ "$NO_BUILD" -eq 1 ] && [ "$NO_INSTALL" -eq 1 ]; then
  tedit_say 'Source update complete.'
  return 0
fi
LOG_FILE=$TEDIT_LOG sh "$REPO_DIR/scripts/install.sh" "$@"
}

main "$@"
