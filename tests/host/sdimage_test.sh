#!/bin/sh
# Checks the card from make sdimage with mtools: PIPBOY/ROM1.ROM is the
# ROM just built, every file of the card tree sits at the same path on
# the card with the same bytes, the tree holds every file of content/
# and the two generated radio tracks, and the Pip-OS kernel and
# PIPOS.CFG are there.
#
#   tests/host/sdimage_test.sh <card> <rom> <card tree>
set -eu
[ $# -eq 3 ] || { echo "usage: $0 <card> <rom> <card tree>" >&2; exit 2; }
root=$(cd "$(dirname "$0")/../.." && pwd)
card="$1@@1M"
rom=$2
tree=$(cd "$3" && pwd)
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

cd "$root/content"
for f in $(find . -type f | sort); do
    cmp -s "$f" "$tree/${f#./}" || fail "content/${f#./} is not in the card tree $tree"
done
for f in HEARTH.PCM DRIFT.PCM; do
    [ -s "$tree/PIPBOY/DATA/RADIO/$f" ] || fail "the card tree has no PIPBOY/DATA/RADIO/$f"
done

n=0
cd "$tree"
for f in $(find . -type f | sort); do
    p=${f#./}
    mcopy -n -i "$card" "::/$p" "$tmp/f" 2>/dev/null || fail "$p is not on the card"
    cmp -s "$tmp/f" "$p" || fail "$p differs on the card"
    n=$((n + 1))
done
[ "$n" -gt 0 ] || fail "the card tree holds no file"
echo "sdimage_test: passed, ROM1.ROM and $n content files"
