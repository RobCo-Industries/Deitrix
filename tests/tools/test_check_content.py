"""Tests for tools/check_content.py. Run by make tools-test.

Each broken tree is the sample content with one rule broken. The checker
must report exactly that rule.
"""

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))

import check_content  # noqa: E402
import mkpcm  # noqa: E402

PCMS = {name: fn() for name, fn in mkpcm.TRACKS.items()}
D = os.path.join("PIPBOY", "DATA")


def make_tree(dst):
    """The card tree: content/ plus the generated tracks, as make builds it."""
    shutil.copytree(os.path.join(ROOT, "content"), dst)
    for name, data in PCMS.items():
        with open(os.path.join(dst, D, "RADIO", name + ".PCM"), "wb") as f:
            f.write(data)


def items_ini(n):
    return "version = 1\n[categories]\nm = MISC\n" + "".join(
        f"[item.i{i}]\nname = Thing\ncat = m\n" for i in range(n))


def notes_ini(note, note2):
    return f"version = 1\n[categories]\nm = MISC\n[item.a]\nname = A\ncat = m\nnote = {note}\nnote2 = {note2}\n"


def map_ini(extra, box="0,0,100,100"):
    return f"version = 1\n[map.local]\ntitle = T\nbox = {box}\n{extra}"


def radio_ini(body):
    return "version = 1\n[station.1]\n" + body


# (what, file relative to PIPBOY/DATA, how, expected message).
# how: a string replaces the file, a (old, new) pair edits it, a bytes
# value replaces it as is, None deletes it.
BROKEN = [
    # The tree.
    ("8.3 name", "NOTES/walk.txt", "Walk\n", "NOTES/walk.txt:0: name is not upper-case 8.3"),
    ("long name", "NOTES/LONGNAME1.TXT", "Walk\n", "name is not upper-case 8.3"),
    ("unknown file", "EXTRA.DAT", "x", "EXTRA.DAT:0: unknown file"),
    ("not a note", "NOTES/LIST.DAT", "x", "NOTES/LIST.DAT:0: unknown file"),
    ("unused radio file", "RADIO/SPARE.TUN", "version 1\nC4\n", "RADIO/SPARE.TUN:0: not used by RADIO.INI"),
    # The INI rules shared by every file.
    ("INI syntax", "PROFILE.INI", ("[owner]", "[owner]\n!bad"), "PROFILE.INI:5: not a section, key or comment"),
    ("INI line length", "PROFILE.INI", ("[about]", "[about]\n; " + "x" * 260), "line above 255 bytes"),
    ("INI value length", "PROFILE.INI", ("[about]", "[about]\nl9 = " + "x" * 130), "value above 127 bytes"),
    ("INI control byte", "PROFILE.INI", ("[owner]", "[owner]\x01"), "control byte"),
    ("non-ASCII", "PROFILE.INI", b"version = 1\n[owner]\nname = Caf\xc3\xa9\n", "byte outside printable ASCII"),
    ("no version", "PROFILE.INI", ("version = 1", ""), "PROFILE.INI:0: no 'version = 1'"),
    ("wrong version", "ITEMS.INI", ("version = 1", "version = 2"), "version '2' is not 1"),
    ("repeated key", "PROFILE.INI", ("g1 = 82,100,Condition", "g1 = 82,100,Condition\ng1 = 1,2,X"), "key 'g1' repeats line"),
    ("repeated section", "PROFILE.INI", ("[about]", "[owner]\n[about]"), "section [owner] repeats line"),
    ("key before a section", "PROFILE.INI", ("version = 1", "version = 1\ncolor = 2"), "unknown key 'color' before the first section"),
    ("file size", "PROFILE.INI", ("[about]", "[about]\n" + "; padding\n" * 420), "above 4096"),
    # PROFILE.INI.
    ("unknown section", "PROFILE.INI", ("[about]", "[stats]\n[about]"), "unknown section [stats]"),
    ("owner name", "PROFILE.INI", ("Field kit 7", "A name that is far too long here"), "name is 32 characters, above 24"),
    ("owner line", "PROFILE.INI", ("Carried on walks, kept in the workshop", "x" * 39), "line is 39 characters, above 38"),
    ("owner key", "PROFILE.INI", ("[owner]", "[owner]\nage = 3"), "unknown key 'age' in [owner]"),
    ("gauge count", "PROFILE.INI", ("g4 = 2,3,Food", "g4 = 2,3,Food\ng7 = 1,2,More"), "unknown key 'g7' in [gauges]"),
    ("gauge form", "PROFILE.INI", ("g4 = 2,3,Food", "g4 = 2"), "a gauge is 'value,max,Name'"),
    ("gauge max", "PROFILE.INI", ("g4 = 2,3,Food", "g4 = 2,0,Food"), "gauge max '0' is not 1 to 9999"),
    ("gauge value", "PROFILE.INI", ("g4 = 2,3,Food", "g4 = 4,3,Food"), "gauge value '4' is not 0 to 3"),
    ("gauge name", "PROFILE.INI", ("g4 = 2,3,Food", "g4 = 2,3,"), "gauge name is empty"),
    ("attr value", "PROFILE.INI", ("a1 = 6,Patience", "a1 = 100,Patience"), "attribute value '100' is not 0 to 99"),
    ("attr count", "PROFILE.INI", ("a1 = 6,Patience", "a13 = 6,Patience"), "unknown key 'a13' in [attrs]"),
    ("attr name", "PROFILE.INI", ("a1 = 6,Patience", "a1 = 6,Patience and more words"), "attribute name is 23 characters, above 16"),
    ("about line", "PROFILE.INI", ("l1 = This kit", "l1 = This kit is a kit that keeps"), "about line is 54 characters, above 38"),
    ("about count", "PROFILE.INI", ("l1 = ", "l9 = "), "unknown key 'l9' in [about]"),
    # ITEMS.INI.
    ("category label", "ITEMS.INI", ("tools = TOOLS", "tools = Tools"), "category label 'Tools' is not 1 to 5 of A-Z"),
    ("category label length", "ITEMS.INI", ("tools = TOOLS", "tools = TOOLKIT"), "category label 'TOOLKIT'"),
    ("category key", "ITEMS.INI", ("misc = MISC", "Misc = MISC"), "category key 'Misc'"),
    ("category count", "ITEMS.INI", ("misc = MISC", "misc = MISC\na = A\nb = B"), "more than 5 categories"),
    ("item id", "ITEMS.INI", ("[item.knife]", "[item.Knife]"), "item id 'Knife'"),
    ("item id length", "ITEMS.INI", ("[item.knife]", "[item.a_very_long_id]"), "item id 'a_very_long_id'"),
    ("unknown section", "ITEMS.INI", ("[item.knife]", "[things]\n[item.knife]"), "unknown section [things]"),
    ("item count limit", "ITEMS.INI", items_ini(65), "more than 64 items"),
    ("item name missing", "ITEMS.INI", ("name = Folding knife\n", ""), "[item.knife] has no 'name'"),
    ("item name length", "ITEMS.INI", ("Folding knife", "A folding knife with a long name"), "name is 32 characters, above 30"),
    ("item category", "ITEMS.INI", ("cat = tools\ncount = 1\nweight = 90", "cat = toys\ncount = 1\nweight = 90"), "category 'toys' is not in [categories]"),
    ("item count", "ITEMS.INI", ("count = 40", "count = 1000"), "count '1000' is not 0 to 999"),
    ("item count sign", "ITEMS.INI", ("count = 40", "count = -1"), "count '-1' is not 0 to 999"),
    ("item unit", "ITEMS.INI", ("unit = loaf", "unit = bundles"), "unit is 7 characters, above 6"),
    ("item weight", "ITEMS.INI", ("weight = 90", "weight = 1kg"), "weight '1kg' is not 0 to 999999 grams"),
    ("item key", "ITEMS.INI", ("weight = 90", "weight = 90\ncolour = red"), "unknown key 'colour' in [item.knife]"),
    ("note2 alone", "ITEMS.INI", ("note = Two coils", "note2 = Two coils"), "note2 without note"),
    ("note length", "ITEMS.INI", notes_ini("x" * 100, "y" * 100), "note and note2 make 201 characters, above 200"),
    # MAP.INI.
    ("no map", "MAP.INI", "version = 1\n", "MAP.INI:0: no [map.local] or [map.world]"),
    ("map section", "MAP.INI", ("[map.world]", "[map.moon]\n[map.world]"), "unknown section [map.moon]"),
    ("lines without map", "MAP.INI", "version = 1\n[map.local]\nbox = 0,0,9,9\n[map.world.lines]\nl1 = 1,1 2,2\n", "[map.world.lines] without [map.world]"),
    ("map title", "MAP.INI", ("title = Alder Brook", "title = A title much longer than this"), "title is 29 characters, above 24"),
    ("map key", "MAP.INI", ("title = Alder Brook", "title = Alder Brook\nscale = 2"), "unknown key 'scale' in [map.local]"),
    ("no box", "MAP.INI", ("box = 0,0,400,300\n", ""), "[map.local] has no box"),
    ("bad box", "MAP.INI", ("box = 0,0,400,300", "box = 400,0,0,300"), "box '400,0,0,300' is not"),
    ("box range", "MAP.INI", ("box = 0,0,800,600", "box = 0,0,1000,600"), "box '0,0,1000,600' is not"),
    ("line points", "MAP.INI", map_ini("[map.local.lines]\nl1 = 1,1\n"), "a line has 1 points, not 2 to 16"),
    ("line length", "MAP.INI", map_ini("[map.local.lines]\nl1 = " + "1,1 " * 17 + "\n"), "a line has 17 points"),
    ("line count", "MAP.INI", map_ini("[map.local.lines]\nl65 = 1,1 2,2\n"), "unknown key 'l65' in [map.local.lines]"),
    ("point form", "MAP.INI", map_ini("[map.local.lines]\nl1 = 1,1 2;2\n"), "point '2;2' is not x,y"),
    ("point outside", "MAP.INI", map_ini("[map.local.lines]\nl1 = 1,1 200,2\n"), "point '200,2' is outside the box"),
    ("place form", "MAP.INI", map_ini("[map.local.places]\np1 = 1,1,home\n"), "a place is 'x,y,kind,Name'"),
    ("place kind", "MAP.INI", map_ini("[map.local.places]\np1 = 1,1,castle,Keep\n"), "kind 'castle' is not one of home, water, shelter, marker, hazard"),
    ("place outside", "MAP.INI", map_ini("[map.local.places]\np1 = 1,101,home,House\n"), "place is outside the box"),
    ("place count", "MAP.INI", map_ini("[map.local.places]\np25 = 1,1,home,House\n"), "unknown key 'p25' in [map.local.places]"),
    ("place name", "MAP.INI", map_ini("[map.local.places]\np1 = 1,1,home,The house at the end of a lane\n"), "place name is 30 characters, above 24"),
    # RADIO.INI.
    ("station number", "RADIO.INI", ("[station.5]", "[station.7]"), "unknown section [station.7]"),
    ("station name", "RADIO.INI", radio_ini("kind = tune\nfile = WALK.TUN\n"), "[station.1] has no 'name'"),
    ("station name length", "RADIO.INI", ("name = Morning", "name = Morning music for the kit"), "name is 25 characters, above 20"),
    ("station kind", "RADIO.INI", ("kind = tune", "kind = midi"), "kind 'midi' is not tune or pcm"),
    ("station key", "RADIO.INI", ("name = Morning", "name = Morning\nband = fm"), "unknown key 'band' in [station.1]"),
    ("file kind", "RADIO.INI", ("file = MORNING.TUN", "file = HEARTH.PCM"), "file 'HEARTH.PCM' is not an upper-case 8.3 name ending in .TUN"),
    ("file case", "RADIO.INI", ("file = MORNING.TUN", "file = morning.tun"), "file 'morning.tun' is not an upper-case"),
    ("file missing", "RADIO/WALK.TUN", None, "RADIO.INI:12: file 'WALK.TUN' is not in RADIO/"),
    ("loop on a tune", "RADIO.INI", ("file = MORNING.TUN", "file = MORNING.TUN\nloop = yes"), "loop is only for kind = pcm"),
    ("loop value", "RADIO.INI", ("loop = yes", "loop = maybe"), "loop 'maybe' is not yes or no"),
    # Tunes and PCM tracks.
    ("tune syntax", "RADIO/WALK.TUN", ("D4 F4 A4 C5 B4", "D4 F4 A4 C5 X4"), "RADIO/WALK.TUN:6: bad note 'X4'"),
    ("tune version", "RADIO/WALK.TUN", ("version 1", "version 9"), "RADIO/WALK.TUN:1: the first line must be 'version 1'"),
    ("tune notes", "RADIO/WALK.TUN", ("length 8", "length 32\n" + "C4 D4 E4 F4\n" * 70), "more than 256 notes"),
    ("tune repeat", "RADIO/WALK.TUN", (":|", ""), "'|:' without ':|'"),
    ("pcm short", "RADIO/DRIFT.PCM", bytes([128]) * 7999, "RADIO/DRIFT.PCM:0: PCM is 7999 bytes, not 8000 to 2097152"),
    ("pcm long", "RADIO/DRIFT.PCM", bytes([128]) * (2 * 1024 * 1024 + 1), "PCM is 2097153 bytes"),
    ("pcm loop", "RADIO/DRIFT.PCM", bytes([128]) * 8000 + bytes([200]), "looped PCM ends at 200 and starts at 128"),
    # Notes.
    ("note size", "NOTES/BIG.TXT", "Big\n" + "word " * 820, "NOTES/BIG.TXT:0: file is 4104 bytes, above 4096"),
    ("note title", "NOTES/EMPTY.TXT", "\nbody\n", "NOTES/EMPTY.TXT:1: the first line, the title, is empty"),
    ("note title length", "NOTES/LONG.TXT", "A title that runs past thirty!!\nx\n", "title is 31 characters, above 30"),
    ("note wrap", "NOTES/TALL.TXT", "Tall\n" + "x\n" * 129, "text wraps to 129 lines at 38 columns, above 128"),
    ("note wrap long lines", "NOTES/WIDE.TXT", "Wide\n" + ("x" * 39 + "\n") * 70, "text wraps to 140 lines"),
    ("note bytes", "NOTES/TAB.TXT", b"Tab\nnul \x00 here\n", "NOTES/TAB.TXT:2: byte outside printable ASCII"),
    ("note count", "NOTES/", [f"N{i:02d}.TXT" for i in range(29)], "more than 32 notes"),
]


def apply(tree, rel, how):
    path = os.path.join(tree, D, rel)
    if rel.endswith("/"):
        for name in how:
            with open(os.path.join(path, name), "w") as f:
                f.write("Note\n")
        return
    if how is None:
        os.remove(path)
        return
    if isinstance(how, tuple):
        with open(path) as f:
            text = f.read()
        assert how[0] in text, (rel, how[0])
        how = text.replace(how[0], how[1], 1)
    mode = "wb" if isinstance(how, bytes) else "w"
    with open(path, mode) as f:
        f.write(how)


class Sample(unittest.TestCase):
    def test_sample_content_is_valid(self):
        with tempfile.TemporaryDirectory() as d:
            tree = os.path.join(d, "card")
            make_tree(tree)
            self.assertEqual(check_content.check_tree(tree), [])

    def test_content_alone_misses_the_tracks(self):
        errors = check_content.check_tree(os.path.join(ROOT, "content"))
        self.assertEqual(errors, [
            os.path.join(D, "RADIO.INI") + ":22: file 'HEARTH.PCM' is not in RADIO/",
            os.path.join(D, "RADIO.INI") + ":28: file 'DRIFT.PCM' is not in RADIO/",
        ])

    def test_content_alone_with_generated_tracks(self):
        names = [n + ".PCM" for n in mkpcm.TRACKS]
        self.assertEqual(check_content.check_tree(os.path.join(ROOT, "content"), names), [])
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "check_content.py"),
                            "--generated", "HEARTH.PCM", os.path.join(ROOT, "content")],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 1)
        self.assertEqual(r.stdout, os.path.join(D, "RADIO.INI") + ":28: file 'DRIFT.PCM' is not in RADIO/\n")

    def test_no_data_folder(self):
        with tempfile.TemporaryDirectory() as d:
            self.assertEqual(check_content.check_tree(d), [os.path.join(D) + ":0: no such folder"])

    def test_missing_files_are_allowed(self):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, D))
            self.assertEqual(check_content.check_tree(d), [])


class Broken(unittest.TestCase):
    def test_each_rule(self):
        for what, rel, how, msg in BROKEN:
            with self.subTest(what), tempfile.TemporaryDirectory() as d:
                tree = os.path.join(d, "card")
                make_tree(tree)
                apply(tree, rel, how)
                errors = check_content.check_tree(tree)
                self.assertTrue(any(msg in e for e in errors), f"{what}: {errors}")


class Ini(unittest.TestCase):
    def parse(self, text):
        return [(s, k, v, n) for kind, s, k, v, n in check_content.parse_ini(text.encode()) if kind == "key"]

    def test_like_ini_c(self):
        self.assertEqual(self.parse('a = 1 # c\n[ s.x ] ; c\nb=x#y\nc = "q \\" #"\r\nd =\n'),
                         [("", "a", "1", 1), ("s.x", "b", "x#y", 3), ("s.x", "c", 'q " #', 4),
                          ("s.x", "d", "", 5)])

    def test_errors(self):
        for text, msg in [("[a", "without ']'"), ("[a b!]", "bad section"), ("k", "no '='"),
                          ('k = "x', "no closing quote"), ('k = "\\t"', "bad escape"),
                          ('k = "x" y', "after the closing quote"), ("[a] x", "after the section"),
                          ("k" * 32 + " = 1", "key above 31"), ("[" + "s" * 32 + "]", "section name above 31"),
                          ("= 1", "not a section")]:
            with self.assertRaises(check_content.IniError, msg=text) as cm:
                check_content.parse_ini(text.encode())
            self.assertIn(msg, cm.exception.msg, text)


class Wrap(unittest.TestCase):
    def test_like_dx_wrap(self):
        self.assertEqual(check_content.wrap_count("", 38), 0)
        self.assertEqual(check_content.wrap_count("a\n", 38), 1)
        self.assertEqual(check_content.wrap_count("a\n\nb", 38), 3)
        self.assertEqual(check_content.wrap_count("x" * 39, 38), 2)
        self.assertEqual(check_content.wrap_count("aaaa bbbb", 5), 2)
        self.assertEqual(check_content.wrap_count("aaaa  bbbb   ", 4), 2)


class Cli(unittest.TestCase):
    def test_exit_codes(self):
        tool = os.path.join(ROOT, "tools", "check_content.py")
        with tempfile.TemporaryDirectory() as d:
            tree = os.path.join(d, "card")
            make_tree(tree)
            r = subprocess.run([sys.executable, tool, tree], capture_output=True, text=True)
            self.assertEqual((r.returncode, r.stdout), (0, ""))
            apply(tree, "EXTRA.DAT", "x")
            r = subprocess.run([sys.executable, tool, tree], capture_output=True, text=True)
            self.assertEqual(r.returncode, 1)
            self.assertEqual(r.stdout, os.path.join(D, "EXTRA.DAT") + ":0: unknown file: Deitrix ignores it\n")


if __name__ == "__main__":
    unittest.main()
