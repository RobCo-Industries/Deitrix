#!/usr/bin/env python3
"""Checks a Deitrix content tree against docs/CONTENT.md.

    tools/check_content.py <dir>

<dir> is the card root: it holds PIPBOY/DATA. Prints one
"<path>:<line>: <message>" per problem, line 0 for the whole file, and
exits 1. Exits 0 and prints nothing when the tree is valid. Standard
library only, Python 3.9+.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import tun2notes  # noqa: E402

# ---- Limits, as docs/CONTENT.md states them -------------------------------

FORMAT_VERSION = "1"
MAX_BYTES = {"PROFILE.INI": 4096, "ITEMS.INI": 32768, "MAP.INI": 32768, "RADIO.INI": 2048}

OWNER_NAME_MAX = 24
OWNER_LINE_MAX = 38
GAUGES_MAX = 6
GAUGE_NAME_MAX = 16
GAUGE_VALUE_MAX = 9999
ATTRS_MAX = 12
ATTR_NAME_MAX = 16
ATTR_VALUE_MAX = 99
ABOUT_MAX = 8
ABOUT_LINE_MAX = 38

CATEGORIES_MAX = 5
CAT_LABEL_MAX = 5
ITEMS_MAX = 64
ID_MAX = 12
ITEM_NAME_MAX = 30
COUNT_MAX = 999
UNIT_MAX = 6
WEIGHT_MAX = 999999
NOTE_MAX = 200

MAPS = ("local", "world")
MAP_TITLE_MAX = 24
COORD_MAX = 999
LINES_MAX = 64
POINTS_MIN, POINTS_MAX = 2, 16
PLACES_MAX = 24
PLACE_NAME_MAX = 24
PLACE_KINDS = ("home", "water", "shelter", "marker", "hazard")

STATIONS_MAX = 6
STATION_NAME_MAX = 20
PCM_MIN, PCM_MAX = 8000, 2 * 1024 * 1024
PCM_LOOP_STEP = 16

NOTES_MAX = 32
NOTE_FILE_MAX = 4096
NOTE_TITLE_MAX = 30
NOTE_COLS = 38
NOTE_LINES_MAX = 128

# RobCo-Core's ini.c limits.
INI_MAX_LINE, INI_MAX_SECTION, INI_MAX_KEY, INI_MAX_VALUE = 255, 31, 31, 127

NAME83 = re.compile(r"^[A-Z0-9_]{1,8}(\.[A-Z0-9_]{1,3})?$")
ID_RE = re.compile(r"^[a-z0-9_]{1,12}$")
LABEL_RE = re.compile(r"^[A-Z]{1,5}$")
DIGITS = re.compile(r"^[0-9]{1,6}$")


class Report:
    def __init__(self):
        self.errors = []

    def add(self, path, line, msg):
        self.errors.append(f"{path}:{line}: {msg}")


# ---- The INI reader: RobCo-Core's ini.c, line for line --------------------

NAME_CHARS = set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")


class IniError(Exception):
    def __init__(self, line, msg):
        super().__init__(msg)
        self.line = line
        self.msg = msg


def _parse_value(p, n):
    if p.startswith('"'):
        out = []
        i = 1
        while True:
            if i >= len(p):
                raise IniError(n, "no closing quote")
            c = p[i]
            i += 1
            if c == '"':
                break
            if c == "\\":
                if i >= len(p) or p[i] not in 'n"\\':
                    raise IniError(n, "bad escape in quoted value")
                c = "\n" if p[i] == "n" else p[i]
                i += 1
            out.append(c)
            if len(out) > INI_MAX_VALUE:
                raise IniError(n, f"value above {INI_MAX_VALUE} bytes")
        rest = p[i:].lstrip(" \t")
        if rest and rest[0] not in "#;":
            raise IniError(n, "text after the closing quote")
        return "".join(out)
    stop = len(p)
    for i, c in enumerate(p):
        if c in "#;" and (i == 0 or p[i - 1] in " \t"):
            stop = i
            break
    v = p[:stop].rstrip(" \t")
    if len(v) > INI_MAX_VALUE:
        raise IniError(n, f"value above {INI_MAX_VALUE} bytes")
    return v


def parse_ini(data):
    """Returns [(kind, section, key, value, line)], kind "section" or "key"."""
    out = []
    section = ""
    lines = data.split(b"\n")
    if lines and lines[-1] == b"" and data:
        lines.pop()
    for n, raw in enumerate(lines, 1):
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        if len(raw) > INI_MAX_LINE:
            raise IniError(n, f"line above {INI_MAX_LINE} bytes")
        if any((b < 0x20 and b != 9) or b == 0x7F for b in raw):
            raise IniError(n, "control byte")
        if any(b > 0x7E for b in raw):
            raise IniError(n, "byte outside printable ASCII")
        s = raw.decode("ascii").lstrip(" \t")
        if not s or s[0] in "#;":
            continue
        if s[0] == "[":
            close = s.find("]")
            if close < 0:
                raise IniError(n, "section without ']'")
            name = s[1:close].strip(" \t")
            if not name or any(c not in NAME_CHARS and c not in " \t" for c in name):
                raise IniError(n, "bad section name")
            if len(name) > INI_MAX_SECTION:
                raise IniError(n, f"section name above {INI_MAX_SECTION} bytes")
            rest = s[close + 1:].lstrip(" \t")
            if rest and rest[0] not in "#;":
                raise IniError(n, "text after the section")
            section = name
            out.append(("section", section, None, None, n))
            continue
        k = 0
        while k < len(s) and s[k] in NAME_CHARS:
            k += 1
        if k == 0:
            raise IniError(n, "not a section, key or comment")
        if k > INI_MAX_KEY:
            raise IniError(n, f"key above {INI_MAX_KEY} bytes")
        key = s[:k]
        rest = s[k:].lstrip(" \t")
        if not rest.startswith("="):
            raise IniError(n, "no '=' after the key")
        value = _parse_value(rest[1:].lstrip(" \t"), n)
        out.append(("key", section, key, value, n))
    return out


# ---- Shared helpers --------------------------------------------------------

def is_text(s):
    return all(0x20 <= ord(c) <= 0x7E for c in s)


def number(s, lo, hi):
    """An int in lo..hi from decimal digits, else None."""
    s = s.strip(" ")
    if not DIGITS.match(s):
        return None
    v = int(s)
    return v if lo <= v <= hi else None


def read_ini(rep, root, rel):
    """Reads and parses an INI file. Returns its items, or None."""
    name = os.path.basename(rel)
    with open(os.path.join(root, rel), "rb") as f:
        data = f.read()
    if len(data) > MAX_BYTES[name]:
        rep.add(rel, 0, f"file is {len(data)} bytes, above {MAX_BYTES[name]}")
        return None
    try:
        items = parse_ini(data)
    except IniError as e:
        rep.add(rel, e.line, e.msg)
        return None
    version = [i for i in items if i[0] == "key" and i[1] == "" and i[2] == "version"]
    if not version:
        rep.add(rel, 0, "no 'version = 1' before the first section")
        return None
    if version[-1][3] != FORMAT_VERSION:
        rep.add(rel, version[-1][4], f"version '{version[-1][3]}' is not {FORMAT_VERSION}")
        return None
    seen_sec = {}
    seen_key = {}
    for kind, sec, key, value, n in items:
        if kind == "section":
            if sec in seen_sec:
                rep.add(rel, n, f"section [{sec}] repeats line {seen_sec[sec]}")
            seen_sec.setdefault(sec, n)
        else:
            if (sec, key) in seen_key:
                rep.add(rel, n, f"key '{key}' repeats line {seen_key[(sec, key)]}")
            seen_key.setdefault((sec, key), n)
            if sec == "" and key != "version":
                rep.add(rel, n, f"unknown key '{key}' before the first section")
    return items


def keys_of(items, section):
    """The keys of a section in file order: [(key, value, line)]."""
    return [(k, v, n) for kind, s, k, v, n in items if kind == "key" and s == section]


def numbered(key, prefix, count):
    """N for a key '<prefix><N>' with N in 1..count, else None."""
    m = re.match(r"^" + prefix + r"([1-9][0-9]?)$", key)
    if m and int(m.group(1)) <= count:
        return int(m.group(1))
    return None


def check_text(rep, rel, n, what, value, lo, hi):
    if not is_text(value):
        rep.add(rel, n, f"{what} has a byte outside printable ASCII")
    elif len(value) < lo:
        rep.add(rel, n, f"{what} is empty")
    elif len(value) > hi:
        rep.add(rel, n, f"{what} is {len(value)} characters, above {hi}")


# ---- PROFILE.INI -----------------------------------------------------------

def check_profile(rep, root, rel):
    items = read_ini(rep, root, rel)
    if items is None:
        return
    known = {"", "owner", "gauges", "attrs", "about"}
    for kind, sec, _, _, n in items:
        if kind == "section" and sec not in known:
            rep.add(rel, n, f"unknown section [{sec}]")
    for key, value, n in keys_of(items, "owner"):
        if key == "name":
            check_text(rep, rel, n, "name", value, 1, OWNER_NAME_MAX)
        elif key == "line":
            check_text(rep, rel, n, "line", value, 0, OWNER_LINE_MAX)
        else:
            rep.add(rel, n, f"unknown key '{key}' in [owner]")
    for key, value, n in keys_of(items, "gauges"):
        if numbered(key, "g", GAUGES_MAX) is None:
            rep.add(rel, n, f"unknown key '{key}' in [gauges]: use g1 to g{GAUGES_MAX}")
            continue
        parts = value.split(",", 2)
        if len(parts) != 3:
            rep.add(rel, n, "a gauge is 'value,max,Name'")
            continue
        mx = number(parts[1], 1, GAUGE_VALUE_MAX)
        v = number(parts[0], 0, GAUGE_VALUE_MAX)
        if mx is None:
            rep.add(rel, n, f"gauge max '{parts[1].strip()}' is not 1 to {GAUGE_VALUE_MAX}")
        elif v is None or v > mx:
            rep.add(rel, n, f"gauge value '{parts[0].strip()}' is not 0 to {mx}")
        check_text(rep, rel, n, "gauge name", parts[2].strip(" "), 1, GAUGE_NAME_MAX)
    for key, value, n in keys_of(items, "attrs"):
        if numbered(key, "a", ATTRS_MAX) is None:
            rep.add(rel, n, f"unknown key '{key}' in [attrs]: use a1 to a{ATTRS_MAX}")
            continue
        parts = value.split(",", 1)
        if len(parts) != 2:
            rep.add(rel, n, "an attribute is 'value,Name'")
            continue
        if number(parts[0], 0, ATTR_VALUE_MAX) is None:
            rep.add(rel, n, f"attribute value '{parts[0].strip()}' is not 0 to {ATTR_VALUE_MAX}")
        check_text(rep, rel, n, "attribute name", parts[1].strip(" "), 1, ATTR_NAME_MAX)
    for key, value, n in keys_of(items, "about"):
        if numbered(key, "l", ABOUT_MAX) is None:
            rep.add(rel, n, f"unknown key '{key}' in [about]: use l1 to l{ABOUT_MAX}")
            continue
        check_text(rep, rel, n, "about line", value, 0, ABOUT_LINE_MAX)


# ---- ITEMS.INI -------------------------------------------------------------

def check_items(rep, root, rel):
    items = read_ini(rep, root, rel)
    if items is None:
        return
    cats = {}
    for key, value, n in keys_of(items, "categories"):
        if not ID_RE.match(key):
            rep.add(rel, n, f"category key '{key}' is not 1 to {ID_MAX} of a-z, 0-9, _")
            continue
        if not LABEL_RE.match(value):
            rep.add(rel, n, f"category label '{value}' is not 1 to {CAT_LABEL_MAX} of A-Z")
        cats[key] = n
        if len(cats) == CATEGORIES_MAX + 1:
            rep.add(rel, n, f"more than {CATEGORIES_MAX} categories")
    count = 0
    for kind, sec, _, _, n in items:
        if kind != "section" or sec in ("", "categories"):
            continue
        if not sec.startswith("item."):
            rep.add(rel, n, f"unknown section [{sec}]")
            continue
        iid = sec[5:]
        if not ID_RE.match(iid):
            rep.add(rel, n, f"item id '{iid}' is not 1 to {ID_MAX} of a-z, 0-9, _")
            continue
        count += 1
        if count == ITEMS_MAX + 1:
            rep.add(rel, n, f"more than {ITEMS_MAX} items")
        keys = {}
        for key, value, kn in keys_of(items, sec):
            keys[key] = (value, kn)
            if key == "name":
                check_text(rep, rel, kn, "name", value, 1, ITEM_NAME_MAX)
            elif key == "cat":
                if value not in cats:
                    rep.add(rel, kn, f"category '{value}' is not in [categories]")
            elif key == "count":
                if number(value, 0, COUNT_MAX) is None:
                    rep.add(rel, kn, f"count '{value}' is not 0 to {COUNT_MAX}")
            elif key == "unit":
                check_text(rep, rel, kn, "unit", value, 1, UNIT_MAX)
            elif key == "weight":
                if number(value, 0, WEIGHT_MAX) is None:
                    rep.add(rel, kn, f"weight '{value}' is not 0 to {WEIGHT_MAX} grams")
            elif key in ("note", "note2"):
                check_text(rep, rel, kn, key, value, 1, INI_MAX_VALUE)
            else:
                rep.add(rel, kn, f"unknown key '{key}' in [{sec}]")
        for req in ("name", "cat"):
            if req not in keys:
                rep.add(rel, n, f"[{sec}] has no '{req}'")
        if "note2" in keys and "note" not in keys:
            rep.add(rel, keys["note2"][1], "note2 without note")
        if "note" in keys and "note2" in keys:
            total = len(keys["note"][0]) + 1 + len(keys["note2"][0])
            if total > NOTE_MAX:
                rep.add(rel, keys["note2"][1], f"note and note2 make {total} characters, above {NOTE_MAX}")


# ---- MAP.INI ---------------------------------------------------------------

def _point(s):
    xy = s.split(",")
    if len(xy) != 2:
        return None
    x, y = number(xy[0], 0, COORD_MAX), number(xy[1], 0, COORD_MAX)
    return None if x is None or y is None else (x, y)


def _inside(p, box):
    return box[0] <= p[0] <= box[2] and box[1] <= p[1] <= box[3]


def check_map(rep, root, rel):
    items = read_ini(rep, root, rel)
    if items is None:
        return
    known = {""}
    for m in MAPS:
        known |= {f"map.{m}", f"map.{m}.lines", f"map.{m}.places"}
    sections = {s for kind, s, _, _, _ in items if kind == "section"}
    for kind, sec, _, _, n in items:
        if kind != "section":
            continue
        if sec not in known:
            rep.add(rel, n, f"unknown section [{sec}]")
        elif sec.count(".") == 2 and sec.rsplit(".", 1)[0] not in sections:
            rep.add(rel, n, f"[{sec}] without [{sec.rsplit('.', 1)[0]}]")
    if "map.local" not in sections and "map.world" not in sections:
        rep.add(rel, 0, "no [map.local] or [map.world]")
    for m in MAPS:
        head = f"map.{m}"
        if head not in sections:
            continue
        box = None
        head_line = [n for kind, s, _, _, n in items if kind == "section" and s == head][0]
        for key, value, n in keys_of(items, head):
            if key == "title":
                check_text(rep, rel, n, "title", value, 1, MAP_TITLE_MAX)
            elif key == "box":
                b = [number(v, 0, COORD_MAX) for v in value.split(",")]
                if len(b) != 4 or None in b or b[0] >= b[2] or b[1] >= b[3]:
                    rep.add(rel, n, f"box '{value}' is not x0,y0,x1,y1 with 0 <= x0 < x1 <= {COORD_MAX} and the same for y")
                else:
                    box = b
            else:
                rep.add(rel, n, f"unknown key '{key}' in [{head}]")
        if box is None:
            if not any(k == "box" for k, _, _ in keys_of(items, head)):
                rep.add(rel, head_line, f"[{head}] has no box")
            continue
        for key, value, n in keys_of(items, head + ".lines"):
            if numbered(key, "l", LINES_MAX) is None:
                rep.add(rel, n, f"unknown key '{key}' in [{head}.lines]: use l1 to l{LINES_MAX}")
                continue
            pts = value.split()
            if not POINTS_MIN <= len(pts) <= POINTS_MAX:
                rep.add(rel, n, f"a line has {len(pts)} points, not {POINTS_MIN} to {POINTS_MAX}")
                continue
            for s in pts:
                p = _point(s)
                if p is None:
                    rep.add(rel, n, f"point '{s}' is not x,y in 0 to {COORD_MAX}")
                    break
                if not _inside(p, box):
                    rep.add(rel, n, f"point '{s}' is outside the box")
                    break
        for key, value, n in keys_of(items, head + ".places"):
            if numbered(key, "p", PLACES_MAX) is None:
                rep.add(rel, n, f"unknown key '{key}' in [{head}.places]: use p1 to p{PLACES_MAX}")
                continue
            parts = value.split(",", 3)
            if len(parts) != 4:
                rep.add(rel, n, "a place is 'x,y,kind,Name'")
                continue
            p = _point(parts[0] + "," + parts[1])
            if p is None:
                rep.add(rel, n, f"place '{parts[0]},{parts[1]}' is not x,y in 0 to {COORD_MAX}")
            elif not _inside(p, box):
                rep.add(rel, n, "place is outside the box")
            if parts[2].strip(" ") not in PLACE_KINDS:
                rep.add(rel, n, f"kind '{parts[2].strip()}' is not one of {', '.join(PLACE_KINDS)}")
            check_text(rep, rel, n, "place name", parts[3].strip(" "), 1, PLACE_NAME_MAX)


# ---- RADIO.INI and its files -----------------------------------------------

def check_pcm(rep, rel, path, loop):
    size = os.path.getsize(path)
    if not PCM_MIN <= size <= PCM_MAX:
        rep.add(rel, 0, f"PCM is {size} bytes, not {PCM_MIN} to {PCM_MAX}")
        return
    if loop:
        with open(path, "rb") as f:
            first = f.read(1)[0]
            f.seek(-1, os.SEEK_END)
            last = f.read(1)[0]
        if abs(last - first) > PCM_LOOP_STEP:
            rep.add(rel, 0, f"looped PCM ends at {last} and starts at {first}: a step above {PCM_LOOP_STEP}")


def check_tun(rep, rel, path):
    with open(path, "rb") as f:
        data = f.read()
    try:
        tun2notes.parse(data)
    except tun2notes.TunError as e:
        rep.add(rel, e.line, e.msg)


def check_radio(rep, root, rel, radio_dir, generated=()):
    items = read_ini(rep, root, rel)
    if items is None:
        return set()
    used = set()
    for kind, sec, _, _, n in items:
        if kind != "section" or sec == "":
            continue
        m = re.match(r"^station\.([1-9])$", sec)
        if not m or int(m.group(1)) > STATIONS_MAX:
            rep.add(rel, n, f"unknown section [{sec}]: use [station.1] to [station.{STATIONS_MAX}]")
            continue
        keys = {k: (v, kn) for k, v, kn in keys_of(items, sec)}
        for key, (value, kn) in keys.items():
            if key not in ("name", "kind", "file", "loop"):
                rep.add(rel, kn, f"unknown key '{key}' in [{sec}]")
        for req in ("name", "kind", "file"):
            if req not in keys:
                rep.add(rel, n, f"[{sec}] has no '{req}'")
        if "name" in keys:
            check_text(rep, rel, keys["name"][1], "name", keys["name"][0], 1, STATION_NAME_MAX)
        kind_v = keys.get("kind", (None, n))[0]
        if kind_v is not None and kind_v not in ("tune", "pcm"):
            rep.add(rel, keys["kind"][1], f"kind '{kind_v}' is not tune or pcm")
            continue
        loop = True
        if "loop" in keys:
            lv, ln = keys["loop"]
            if kind_v != "pcm":
                rep.add(rel, ln, "loop is only for kind = pcm")
            elif lv not in ("yes", "no"):
                rep.add(rel, ln, f"loop '{lv}' is not yes or no")
            loop = lv != "no"
        if "file" not in keys or kind_v is None:
            continue
        fv, fn_line = keys["file"]
        ext = ".TUN" if kind_v == "tune" else ".PCM"
        if not NAME83.match(fv) or not fv.endswith(ext):
            rep.add(rel, fn_line, f"file '{fv}' is not an upper-case 8.3 name ending in {ext}")
            continue
        path = os.path.join(radio_dir, fv)
        frel = os.path.join(os.path.dirname(rel), "RADIO", fv)
        if not os.path.isfile(path):
            if kind_v == "pcm" and fv in generated:
                continue
            rep.add(rel, fn_line, f"file '{fv}' is not in RADIO/")
            continue
        used.add(fv)
        if kind_v == "tune":
            check_tun(rep, frel, path)
        else:
            check_pcm(rep, frel, path, loop)
    return used


# ---- Notes -----------------------------------------------------------------

def wrap_count(s, cols):
    """The lines dx_wrap in src/ui/text.c makes of s, without a line limit."""
    n = 0
    p = 0
    fresh = True
    end_s = len(s)
    while p < end_s:
        if not fresh:
            while p < end_s and s[p] in " \t":
                p += 1
            if p >= end_s:
                break
        start = p
        brk = -1
        while p < end_s and s[p] != "\n" and p - start < cols:
            if s[p] in " \t":
                brk = p
            p += 1
        if p >= end_s or s[p] == "\n":
            fresh = p < end_s
            if fresh:
                p += 1
        elif s[p] in " \t":
            fresh = False
        elif brk >= 0:
            p = brk
            fresh = False
        else:
            fresh = False
        n += 1
    return n


def check_note(rep, rel, path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) > NOTE_FILE_MAX:
        rep.add(rel, 0, f"file is {len(data)} bytes, above {NOTE_FILE_MAX}")
        return
    lines = data.split(b"\n")
    for n, raw in enumerate(lines, 1):
        if raw.endswith(b"\r"):
            raw = raw[:-1]
        if any((b < 0x20 and b != 9) or b > 0x7E for b in raw):
            rep.add(rel, n, "byte outside printable ASCII, tab and line ends")
            return
    text = data.replace(b"\r\n", b"\n").decode("ascii")
    title, _, body = text.partition("\n")
    if not title.strip(" \t"):
        rep.add(rel, 1, "the first line, the title, is empty")
    elif len(title) > NOTE_TITLE_MAX:
        rep.add(rel, 1, f"title is {len(title)} characters, above {NOTE_TITLE_MAX}")
    wrapped = wrap_count(body, NOTE_COLS)
    if wrapped > NOTE_LINES_MAX:
        rep.add(rel, 0, f"text wraps to {wrapped} lines at {NOTE_COLS} columns, above {NOTE_LINES_MAX}")


# ---- The tree --------------------------------------------------------------

def check_names(rep, root, data_rel):
    for dirpath, dirs, files in os.walk(os.path.join(root, data_rel)):
        dirs.sort()
        for name in sorted(dirs + files):
            if not NAME83.match(name):
                rel = os.path.relpath(os.path.join(dirpath, name), root)
                rep.add(rel, 0, "name is not upper-case 8.3: 1 to 8 of A-Z, 0-9, _ and an optional .EXT of 1 to 3")


def check_tree(root, generated=()):
    """Returns the list of problems in the tree at root.

    generated: .PCM names that the build adds later. A station may name
    one of them although the file is not in the tree yet. Used on content/.
    """
    rep = Report()
    data_rel = os.path.join("PIPBOY", "DATA")
    data = os.path.join(root, data_rel)
    if not os.path.isdir(data):
        rep.add(data_rel, 0, "no such folder")
        return rep.errors
    check_names(rep, root, data_rel)
    files = {"PROFILE.INI": check_profile, "ITEMS.INI": check_items, "MAP.INI": check_map}
    used = set()
    for name in sorted(os.listdir(data)):
        rel = os.path.join(data_rel, name)
        path = os.path.join(data, name)
        if name in files and os.path.isfile(path):
            files[name](rep, root, rel)
        elif name == "RADIO.INI" and os.path.isfile(path):
            used = check_radio(rep, root, rel, os.path.join(data, "RADIO"), generated)
        elif name in ("NOTES", "RADIO") and os.path.isdir(path):
            pass
        else:
            rep.add(rel, 0, "unknown file: Deitrix ignores it")
    notes = os.path.join(data, "NOTES")
    if os.path.isdir(notes):
        txt = 0
        for name in sorted(os.listdir(notes)):
            rel = os.path.join(data_rel, "NOTES", name)
            if not name.endswith(".TXT") or not os.path.isfile(os.path.join(notes, name)):
                rep.add(rel, 0, "unknown file: NOTES holds .TXT files only")
                continue
            txt += 1
            if txt == NOTES_MAX + 1:
                rep.add(rel, 0, f"more than {NOTES_MAX} notes: this one and later ones are not shown")
            check_note(rep, rel, os.path.join(notes, name))
    radio = os.path.join(data, "RADIO")
    if os.path.isdir(radio):
        for name in sorted(os.listdir(radio)):
            if name not in used:
                rep.add(os.path.join(data_rel, "RADIO", name), 0, "not used by RADIO.INI")
    return rep.errors


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    generated = []
    while len(argv) >= 2 and argv[0] == "--generated":
        generated.append(argv[1])
        argv = argv[2:]
    if len(argv) != 1:
        print("usage: check_content.py [--generated NAME.PCM]... <dir>", file=sys.stderr)
        return 2
    errors = check_tree(argv[0], generated)
    for e in errors:
        print(e)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
