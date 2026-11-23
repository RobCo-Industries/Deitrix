#!/usr/bin/env bash
# Checks that every Pip-OS link with a commit hash in the Markdown files
# points at the commit the Pip-OS submodule has checked out:
#
#   tools/check_pipos_links.sh [root]
#
# A link is github.com/RobCo-Industries/Pip-OS/(blob|tree|commit)/<hash>.
# It is Pip-OS's tools/check_doc_links.sh, which checks RobCo-Core links,
# with the repository name and the submodule path changed. Like that
# script it reads the commit from the submodule's git files and never
# calls git, so it works in the CI container. CHANGELOG.md, CLAUDE.md,
# AGENTS.md and the .git, build, .venv, external and .claude folders are
# skipped. The submodule is root/external/pip-os, or PIP_OS if set.
# Exits 0 when every link matches, 1 when one differs or the commit is unknown.
set -euo pipefail

die() {
    echo "check-pipos-links: $*" >&2
    exit 1
}

root=${1:-$(cd "$(dirname "$0")/.." && pwd)}
cd "$root"
sub=${PIP_OS:-external/pip-os}

# The submodule's git directory: .git is a directory, or a file with
# "gitdir: <path>", relative to the submodule.
gitdir=$sub/.git
if [ -f "$gitdir" ]; then
    line=$(head -n 1 "$gitdir")
    case "$line" in
    gitdir:*) ;;
    *) die "$gitdir does not start with gitdir:" ;;
    esac
    gitdir=${line#gitdir:}
    gitdir=${gitdir#"${gitdir%%[![:space:]]*}"}
    case "$gitdir" in
    /*) ;;
    *) gitdir=$sub/$gitdir ;;
    esac
fi
[ -f "$gitdir/HEAD" ] || die "no HEAD in $gitdir: run git submodule update --init --recursive"

head=$(tr -d '\r' < "$gitdir/HEAD" | head -n 1)
case "$head" in
ref:*)
    ref=${head#ref:}
    ref=${ref#"${ref%%[![:space:]]*}"}
    commit=
    common=$gitdir
    [ -f "$gitdir/commondir" ] && common=$gitdir/$(head -n 1 "$gitdir/commondir")
    for d in "$gitdir" "$common"; do
        if [ -f "$d/$ref" ]; then
            commit=$(head -n 1 "$d/$ref" | tr -d '\r')
            break
        fi
        if [ -f "$d/packed-refs" ]; then
            commit=$(awk -v r="$ref" '$2 == r { print $1; exit }' "$d/packed-refs")
            [ -n "$commit" ] && break
        fi
    done
    [ -n "$commit" ] || die "cannot resolve $ref in $gitdir"
    ;;
*) commit=$head ;;
esac
case "$commit" in
*[!0-9a-f]* | '') die "bad commit '$commit' in $gitdir" ;;
esac

status=0
while IFS= read -r -d '' f; do
    case "${f#./}" in
    CHANGELOG.md | CLAUDE.md | AGENTS.md) continue ;;
    esac
    while IFS=: read -r n hash; do
        if [ "$hash" != "$commit" ]; then
            echo "check-pipos-links: ${f#./}:$n links Pip-OS at $hash, the submodule is at $commit" >&2
            status=1
        fi
    done < <(grep -noE 'github\.com/RobCo-Industries/Pip-OS/(blob|tree|commit)/[0-9a-fA-F]+' "$f" |
        sed -E 's#^([0-9]+):.*/(blob|tree|commit)/#\1:#')
done < <(find . \( -name .git -o -path ./build -o -path ./.venv -o -path ./external \
    -o -name .claude \) -prune -o -name '*.md' -type f -print0)

[ "$status" -eq 0 ] && echo "check-pipos-links: ok ($commit)"
exit "$status"
