"""The MAP tab in QEMU: the two maps, the places, the detail and the empty states.

MAP logs a "deitrix: map ..." line when a frame shows another view:
"shows <LOCAL|WORLD> place=<n>" for a map, "detail <map> place=<n>" for
a place's detail and "shows none" for the empty state. After each map
frame it logs "map drew <ms> ms", the time the body took. As on STAT,
the first frame after the tab opens logs nothing. A line goes out
during the draw, just before the present, so a test takes its
screenshot once two in a row match, and sends a key only then.

The sample card's MAP.INI has LOCAL with 7 places and WORLD with 5.
MAP sits between DATA and RADIO: three dial detents from STAT.
"""

import re

from test_deitrix import boot_rom, golden, tabs, wait_ready, wait_tabs
from test_model import card_target, mtools
from test_stat import count, key, quiet, settled, wait_count
from pipos_testing import no_exception, press, script

SHOWS = re.compile(r"^deitrix: map shows (LOCAL|WORLD) place=(-?\d+)\r?\n", re.M)
DETAIL = re.compile(r"^deitrix: map detail (LOCAL|WORLD) place=(-?\d+)\r?\n", re.M)
NONE = re.compile(r"^deitrix: map shows none\r?\n", re.M)
DREW = re.compile(r"^deitrix: map drew (\d+) ms\r?\n", re.M)
LOCAL_PLACES = 7

# Rows of the 640x480 screenshot: the header and the sub-tab row (screen
# rows 0 to 41), and the footer (215 to 239).
TOP = (0, 0, 640, 2 * 42)
FOOTER = (0, 2 * 215, 640, 480)


def to_map(vm):
    """STAT to MAP: three dial detents."""
    script(vm, "dial 3")
    assert wait_tabs(vm, 1) == ["MAP"], vm.serial


def card_with(card, tmp_path, text):
    path = tmp_path / "MAP.INI"
    path.write_text(text)
    mtools("mcopy", "-o", "-i", card_target(card), str(path), "::PIPBOY/DATA/MAP.INI")


def test_map_walk(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_map(vm)
        golden(settled(vm, tmp_path), "map_local")

        # DOWN through every place and round to the first; UP wraps back.
        for i in list(range(1, LOCAL_PLACES)) + [0]:
            assert key(vm, "down", SHOWS) == ("LOCAL", str(i))
        assert key(vm, "up", SHOWS) == ("LOCAL", str(LOCAL_PLACES - 1))
        golden(settled(vm, tmp_path), "map_local_place")

        # The detail: RIGHT, UP, DOWN and SELECT do nothing there.
        assert key(vm, "select", DETAIL) == ("LOCAL", "6")
        golden(settled(vm, tmp_path), "map_detail")
        for name in ("right", "up", "down", "select"):
            press(vm, name)
        quiet(vm, SHOWS)
        assert count(vm, DETAIL) == 1 and tabs(vm) == ["MAP"], vm.serial
        # Back, then LEFT: each returns to the map with the place kept.
        assert key(vm, "back", SHOWS) == ("LOCAL", "6")
        assert key(vm, "select", DETAIL) == ("LOCAL", "6")
        assert key(vm, "left", SHOWS) == ("LOCAL", "6")
        assert tabs(vm) == ["MAP"], vm.serial

        # WORLD, then out to RADIO and back: the sub-tab and places stay.
        assert key(vm, "right", SHOWS) == ("WORLD", "0")
        golden(settled(vm, tmp_path), "map_world")
        assert key(vm, "down", SHOWS) == ("WORLD", "1")
        press(vm, "right")
        assert wait_tabs(vm, 2) == ["MAP", "RADIO"], vm.serial
        press(vm, "left")
        assert wait_tabs(vm, 3) == ["MAP", "RADIO", "MAP"], vm.serial
        settled(vm, tmp_path)
        assert key(vm, "up", SHOWS) == ("WORLD", "0")
        assert key(vm, "left", SHOWS) == ("LOCAL", "6")
        press(vm, "left")
        assert wait_tabs(vm, 4)[-1] == "DATA", vm.serial

        # The dial leaves from the detail; coming back shows the map.
        # RIGHT on DATA changes its sub-tab, so the dial goes back.
        script(vm, "dial 1")
        assert wait_tabs(vm, 5)[-1] == "MAP", vm.serial
        settled(vm, tmp_path)
        assert key(vm, "select", DETAIL) == ("LOCAL", "6")
        settled(vm, tmp_path)
        script(vm, "dial 1")
        assert wait_tabs(vm, 6)[-1] == "RADIO", vm.serial
        settled(vm, tmp_path)
        script(vm, "dial -1")
        assert wait_tabs(vm, 7)[-1] == "MAP", vm.serial
        golden(settled(vm, tmp_path), "map_local_place")
        assert key(vm, "down", SHOWS) == ("LOCAL", "0")
        no_exception(vm)


def test_map_no_file(board, kernel_image, card, tmp_path, need_mtools):
    mtools("mdel", "-i", card_target(card), "::PIPBOY/DATA/MAP.INI")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert "deitrix: load MAP.INI missing" in vm.serial, vm.serial
        to_map(vm)
        golden(settled(vm, tmp_path), "map_empty")
        for name in ("up", "down", "select", "back"):
            press(vm, name)
        quiet(vm, SHOWS)
        assert tabs(vm) == ["MAP"] and count(vm, NONE) == 0, vm.serial
        # No sub-tabs: RIGHT changes the tab.
        press(vm, "right")
        assert wait_tabs(vm, 2) == ["MAP", "RADIO"], vm.serial
        no_exception(vm)


NO_PLACES = """version = 1
[map.local]
title = Bare ground
box = 0,0,200,100
[map.local.lines]
l1 = 0,50 60,40 120,60 200,50
l2 = 90,10 110,10 110,30 90,30 90,10
"""


def test_map_no_places(board, kernel_image, card, tmp_path, need_mtools):
    card_with(card, tmp_path, NO_PLACES)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_map(vm)
        golden(settled(vm, tmp_path), "map_no_places")
        for name in ("up", "down", "select", "back"):
            press(vm, name)
        quiet(vm, DETAIL)
        assert count(vm, SHOWS) == 0 and tabs(vm) == ["MAP"], vm.serial
        no_exception(vm)


# A map whose lines run on its box's edges and whose places sit in its
# corners: the fitted map reaches the pane's edges. The same places in
# the centre and no lines draw nothing near them.
EDGES = """version = 1
[map.local]
title = Edges
box = 0,0,999,999
[map.local.lines]
l1 = 0,0 999,0 999,999 0,999 0,0
l2 = 0,0 999,999
l3 = 999,0 0,999
[map.local.places]
p1 = 0,0,marker,Corner
p2 = 999,0,marker,Corner two
p3 = 999,999,marker,Corner three
p4 = 0,999,marker,Corner four
"""
CENTRE = EDGES.split("[map.local.lines]")[0] + """[map.local.places]
p1 = 500,500,marker,Corner
p2 = 500,500,marker,Corner two
p3 = 500,500,marker,Corner three
p4 = 500,500,marker,Corner four
"""


def shot_of(kernel_image, board, card, tmp_path, text, downs):
    card_with(card, tmp_path, text)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_map(vm)
        settled(vm, tmp_path)
        for i in range(downs):
            assert key(vm, "down", SHOWS) == ("LOCAL", str(i + 1))
        img = settled(vm, tmp_path)
        no_exception(vm)
        return img


def test_map_clip(board, kernel_image, card, tmp_path, need_mtools):
    # The selected place: the bottom-right corner, the ring's largest reach.
    edges = shot_of(kernel_image, board, card, tmp_path, EDGES, 2)
    centre = shot_of(kernel_image, board, card, tmp_path, CENTRE, 2)
    for box in (TOP, FOOTER):
        assert edges.crop(box).tobytes() == centre.crop(box).tobytes(), box
    # The edge map is drawn: the frame lines light both screens' columns.
    assert edges.tobytes() != centre.tobytes()


def max_map():
    """MAP.INI at the format's limits: 64 lines of 16 points across the
    whole range, 24 places with 24-character names, in both maps."""
    out = ["version = 1"]
    for name in ("local", "world"):
        out += [f"[map.{name}]", "title = " + "T" * 24, "box = 0,0,999,999",
                f"[map.{name}.lines]"]
        for n in range(1, 65):
            pts = [f"{(i * 66 + n * 7) % 1000},{999 if (i + n) % 2 else 0}" for i in range(16)]
            out.append(f"l{n} = " + " ".join(pts))
        out.append(f"[map.{name}.places]")
        for n in range(1, 25):
            out.append(f"p{n} = {n * 41},{999 - n * 41},hazard,{('Place %02d ' % n).ljust(24, 'x')}")
    return "\n".join(out) + "\n"


def test_map_redraw_cost(board, kernel_image, card, tmp_path, need_mtools):
    """The largest map: every redraw of the map view logs its time. The
    design budget is 100 ms for one redraw."""
    card_with(card, tmp_path, max_map())
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert "deitrix: load MAP.INI maps=2 lines=128 places=48" in vm.serial, vm.serial
        to_map(vm)
        settled(vm, tmp_path)
        for i in range(1, 6):
            assert key(vm, "down", SHOWS) == ("LOCAL", str(i))
        wait_count(vm, DREW, 5)
        times = [int(t) for t in DREW.findall(vm.serial)]
        print(f"map redraw on {board}: {times} ms")
        assert max(times) < 100, times
        no_exception(vm)
