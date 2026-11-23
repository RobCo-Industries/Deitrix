#!/bin/sh
# Checks the card from make sdimage with mtools: PIPBOY/ROM1.ROM is the
# ROM just built, every file under content/ sits at the same path on the
# card with the same bytes, and the Pip-OS kernel and PIPOS.CFG are there.
#
#   tests/host/sdimage_test.sh <card> <rom>
set -eu
[ $# -eq 2 ] || { echo "usage: $0 <card> <rom>" >&2; exit 2; }
root=$(cd "$(dirname "$0")/../.." && pwd)
card="$1@@1M"
rom=$2
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
export MTOOLS_SKIP_CHECK=1

fail() {
    echo "sdimage_test: $*" >&2
    exit 1
}

mcopy -n -i "$card" ::/PIPBOY/ROM1.ROM "$tmp/rom" 2>/dev/null || fail "no PIPBOY/ROM1.ROM on the card"
cmp -s "$tmp/rom" "$rom" || fail "PIPBOY/ROM1.ROM is not $rom"
mdir -b -i "$card" ::/PIPBOY | grep -q 'PIPOS\.CFG$' || fail "no PIPBOY/PIPOS.CFG on the card"
mdir -b -i "$card" :: | grep -q 'PIPOS\.IMG$' || fail "no PIPOS.IMG kernel on the card"
if mdir -b -i "$card" ::/PIPBOY/HOLO 2>/dev/null | grep -q 'HTP$'; then
    fail "the card has a holotape; Deitrix ships none yet"
fi

n=0
cd "$root/content"
for f in $(find . -type f | sort); do
    p=${f#./}
    mcopy -n -i "$card" "::/$p" "$tmp/f" 2>/dev/null || fail "content/$p is not on the card"
    cmp -s "$tmp/f" "$p" || fail "content/$p differs on the card"
    n=$((n + 1))
done
[ "$n" -gt 0 ] || fail "content/ holds no file"
echo "sdimage_test: passed, ROM1.ROM and $n content files"
