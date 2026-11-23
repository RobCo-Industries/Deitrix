#!/bin/sh
# Checks the card from make sdimage with mtools: PIPBOY/ROM1.ROM is the
# ROM just built, every file of the card tree sits at the same path on
# the card with the same bytes, the tree holds every file of content/
# and the two generated radio tracks, the Pip-OS kernel and PIPOS.CFG
# are there, and PIPBOY/HOLO holds exactly the tapes given, same bytes.
#
#   tests/host/sdimage_test.sh <card> <rom> <card tree> [<tape>...]
set -eu
[ $# -ge 3 ] || { echo "usage: $0 <card> <rom> <card tree> [<tape>...]" >&2; exit 2; }
root=$(cd "$(dirname "$0")/../.." && pwd)
card="$1@@1M"
rom=$2
tree=$(cd "$3" && pwd)
shift 3
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
want=""
for t in "$@"; do
    b=$(basename "$t")
    mcopy -n -i "$card" "::/PIPBOY/HOLO/$b" "$tmp/tape" 2>/dev/null || fail "no PIPBOY/HOLO/$b on the card"
    cmp -s "$tmp/tape" "$t" || fail "PIPBOY/HOLO/$b is not $t"
    want="$want$b
"
done
got=$(mdir -b -i "$card" ::/PIPBOY/HOLO 2>/dev/null | grep 'HTP$' | sed 's|.*/||' | sort)
[ "$got" = "$(printf '%s' "$want" | sort)" ] || fail "PIPBOY/HOLO holds: $got"

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
echo "sdimage_test: passed, ROM1.ROM, $# tapes and $n content files"
