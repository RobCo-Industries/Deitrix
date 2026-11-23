#!/bin/sh
# Runs tools/check_pipos_links.sh on fake trees with no git: a matching
# link passes, a stale link fails, a submodule .git file and packed refs
# resolve, CHANGELOG.md is skipped, and a missing submodule fails.
set -eu
script=$(cd "$(dirname "$0")/../.." && pwd)/tools/check_pipos_links.sh
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
good=1111111111111111111111111111111111111111
old=2222222222222222222222222222222222222222
url=https://github.com/RobCo-Industries/Pip-OS/blob

fail() {
    echo "check_pipos_links_test: $*" >&2
    exit 1
}

# A tree whose submodule HEAD is detached at $good.
mk() {
    rm -rf "$tmp/r"
    mkdir -p "$tmp/r/external/pip-os/.git" "$tmp/r/docs"
    echo "$good" > "$tmp/r/external/pip-os/.git/HEAD"
}

mk
echo "[x]($url/$good/docs/API.md)" > "$tmp/r/docs/A.md"
"$script" "$tmp/r" >/dev/null 2>&1 || fail "a matching link failed"

echo "[y]($url/$old/README.md)" >> "$tmp/r/docs/A.md"
if "$script" "$tmp/r" >/dev/null 2>"$tmp/err"; then
    fail "a stale link passed"
fi
grep -q 'docs/A.md:2 links Pip-OS at 2222' "$tmp/err" || fail "no file and line in: $(cat "$tmp/err")"

mk
echo "[y]($url/$old/README.md)" > "$tmp/r/CHANGELOG.md"
"$script" "$tmp/r" >/dev/null 2>&1 || fail "CHANGELOG.md was not skipped"

# A .git file pointing at a git directory with the commit in packed-refs.
mk
rm -rf "$tmp/r/external/pip-os/.git"
mkdir -p "$tmp/r/mod"
echo "gitdir: ../../mod" > "$tmp/r/external/pip-os/.git"
echo "ref: refs/heads/master" > "$tmp/r/mod/HEAD"
echo "$good refs/heads/master" > "$tmp/r/mod/packed-refs"
echo "[x]($url/$good/docs/API.md)" > "$tmp/r/docs/A.md"
"$script" "$tmp/r" >/dev/null 2>&1 || fail "a gitdir file with packed refs failed"

mk
rm -rf "$tmp/r/external/pip-os/.git"
if "$script" "$tmp/r" >/dev/null 2>&1; then
    fail "a missing submodule passed"
fi
echo "check_pipos_links_test: passed"
