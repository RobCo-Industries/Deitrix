"""The widget gallery ROM in QEMU: one golden per screen, and scripted
runs through the list, a held key and the text block.

The gallery is a test-only ROM from `make gallery`: one screen per
widget on the same app shell. It logs a "gallery: ..." line for each
list and text move. A move logs before its redraw, so a test that needs
the new frame on screen sends one more key that changes nothing and
waits for its line: the shell draws before it takes the next event.
"""

import time

from test_deitrix import boot_rom, golden, tabs, wait_ready, wait_tabs
from pipos_testing import TIMEOUT, no_exception, press, screenshot, script
from conftest import serial_line

SEL = serial_line(r"gallery: list sel=(\d+) top=(\d+)")
LIST_END = serial_line(r"gallery: list end=(\d+)")
TEXT_TOP = serial_line(r"gallery: text top=(\d+)")
TEXT_END = serial_line(r"gallery: text end=(\d+)")
SUB = serial_line(r"gallery: sub=(\d+)")
DETAIL = serial_line(r"gallery: detail=(\d+)")
BACK = serial_line(r"gallery: back=(\d+)")

LIST_ROWS = 12
LIST_WINDOW = 8


def wait_count(vm, pattern, count):
    """Waits for `count` lines matching pattern, returns their groups."""
    deadline = time.monotonic() + TIMEOUT
    while len(pattern.findall(vm.serial)) < count:
        assert time.monotonic() < deadline, f"wanted {count} of {pattern.pattern}\n{vm.serial}"
        time.sleep(0.05)
    return pattern.findall(vm.serial)


def key(vm, name, pattern):
    """Taps a key and waits for one more line of pattern. Returns the last."""
    before = len(pattern.findall(vm.serial))
    press(vm, name)
    return wait_count(vm, pattern, before + 1)[-1]


def test_gallery_screens(board, kernel_image, gallery_card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=gallery_card) as vm:
        wait_ready(vm)
        golden(screenshot(vm, tmp_path), "gallery_list")
        order = ["TEXT", "BAR", "KV", "TABS"]
        for i, name in enumerate(order, start=1):
            press(vm, "right")
            assert wait_tabs(vm, i)[-1] == name, vm.serial
            golden(screenshot(vm, tmp_path), f"gallery_{name.lower()}")
        # TABS keeps RIGHT for its sub-tabs until the last one.
        for sub in range(1, 4):
            assert key(vm, "right", SUB) == str(sub)
        assert tabs(vm)[-1] == "TABS", vm.serial
        for i, name in enumerate(["ICON", "FONT"], start=len(order) + 1):
            press(vm, "right")
            assert wait_tabs(vm, i)[-1] == name, vm.serial
            golden(screenshot(vm, tmp_path), f"gallery_{name.lower()}")
        # FONT is the last: RIGHT wraps to LIST, LEFT comes back past
        # the sub-tabs, which stay on the last.
        press(vm, "right")
        assert wait_tabs(vm, 7)[-1] == "LIST", vm.serial
        script(vm, "dial -3")
        assert wait_tabs(vm, 8)[-1] == "TABS", vm.serial
        assert key(vm, "left", SUB) == "2"
        no_exception(vm)


def test_gallery_list_and_text(board, kernel_image, gallery_card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=gallery_card) as vm:
        wait_ready(vm)
        # Down to the last row: the window follows one row at a time.
        for row in range(1, LIST_ROWS):
            sel, top = key(vm, "down", SEL)
            assert int(sel) == row
            assert int(top) == max(0, row - LIST_WINDOW + 1)
        # DOWN at the end: no move, no wrap.
        assert key(vm, "down", LIST_END) == str(LIST_ROWS - 1)
        golden(screenshot(vm, tmp_path), "gallery_list_end")
        # And back to the top, where the window starts again at 0.
        for row in range(LIST_ROWS - 2, -1, -1):
            sel, top = key(vm, "up", SEL)
            assert int(sel) == row
            assert int(top) == min(row, LIST_ROWS - LIST_WINDOW)
        assert key(vm, "up", LIST_END) == "0"
        golden(screenshot(vm, tmp_path), "gallery_list")

        # The text block: down to its end, a golden, then back up.
        press(vm, "right")
        assert wait_tabs(vm, 1) == ["TEXT"], vm.serial
        steps = 0
        while not TEXT_END.findall(vm.serial):
            press(vm, "down")
            steps += 1
            assert steps < 60, vm.serial
            time.sleep(0.05)
        last = int(wait_count(vm, TEXT_TOP, 1)[-1])
        assert last > 0
        # Every step moved one line, until the end.
        assert [int(t) for t in TEXT_TOP.findall(vm.serial)] == list(range(1, last + 1))
        ends = len(TEXT_END.findall(vm.serial))
        assert key(vm, "down", TEXT_END) == str(last)
        assert len(TEXT_END.findall(vm.serial)) == ends + 1
        golden(screenshot(vm, tmp_path), "gallery_text_end")
        for line in range(last - 1, -1, -1):
            assert key(vm, "up", TEXT_TOP) == str(line)
        assert key(vm, "up", TEXT_END) == "0"
        golden(screenshot(vm, tmp_path), "gallery_text")

        # Back to LIST: it kept its state.
        press(vm, "left")
        assert wait_tabs(vm, 2)[-1] == "LIST", vm.serial
        assert key(vm, "up", LIST_END) == "0"
        golden(screenshot(vm, tmp_path), "gallery_list")
        no_exception(vm)


def test_gallery_detail(board, kernel_image, gallery_card, tmp_path, need_mtools):
    """SELECT on a list row opens one level down, LEFT comes back."""
    with boot_rom(kernel_image, board, sd=gallery_card) as vm:
        wait_ready(vm)
        for row in (1, 2):
            assert key(vm, "down", SEL) == (str(row), "0")
        assert key(vm, "select", DETAIL) == "2"
        # UP changes nothing in the detail: once it logs, the frame is shown.
        assert key(vm, "up", DETAIL) == "2"
        golden(screenshot(vm, tmp_path), "gallery_detail")
        # RIGHT stays inside the level: no tab change.
        assert key(vm, "right", DETAIL) == "2"
        assert tabs(vm) == [], vm.serial
        # LEFT is back: one level up, still on LIST, the selection kept.
        assert key(vm, "left", BACK) == "2"
        assert tabs(vm) == [], vm.serial
        assert key(vm, "up", SEL) == ("1", "0")
        # At the top level LEFT changes the tab again.
        press(vm, "left")
        assert wait_tabs(vm, 1)[-1] == "FONT", vm.serial
        no_exception(vm)


def wait_back(vm, before):
    """Waits for the back line after `before` of them, returns its row."""
    return wait_count(vm, BACK, before + 1)[-1]


def test_gallery_back_button(board, kernel_image, gallery_card, tmp_path, need_mtools):
    """The Back button, ABI 1.2's button 6, goes back like LEFT: from the
    script channel and from Backspace on the console. At the top level
    it does not change the tab."""
    with boot_rom(kernel_image, board, sd=gallery_card) as vm:
        wait_ready(vm)
        assert key(vm, "down", SEL) == ("1", "0")
        assert key(vm, "select", DETAIL) == "1"
        script(vm, "key back 1")
        script(vm, "key back 0")
        assert wait_back(vm, 0) == "1"
        assert tabs(vm) == [], vm.serial
        # Back at the list: the selection is kept, UP moves it.
        assert key(vm, "up", SEL) == ("0", "0")
        # Backspace on the console is Back too.
        assert key(vm, "select", DETAIL) == "0"
        vm.send("\x7f")
        assert wait_back(vm, 1) == "0"
        assert tabs(vm) == [], vm.serial
        # At the top level the shell ignores Back. UP at row 0 logs the
        # list end after it, so the Back press was taken by then.
        press(vm, "back")
        assert key(vm, "up", LIST_END) == "0"
        vm.send("\x7f")
        assert key(vm, "up", LIST_END) == "0"
        assert tabs(vm) == [], vm.serial
        assert len(BACK.findall(vm.serial)) == 2, vm.serial
        golden(screenshot(vm, tmp_path), "gallery_list")
        no_exception(vm)


# The kernel repeats a held key first 500 ms after the press, then every
# 100 ms. Held for HOLD_S, the list gets the press and, without a stall,
# at least (HOLD_S - 0.5) / 0.1 repeats: 11 here, more than the list has.
# After a stall the kernel restarts the schedule from that moment, so the
# test asks for MIN_ROWS only: the press and several repeats.
HOLD_S = 1.6
MIN_ROWS = 5


def test_gallery_hold_repeats(board, kernel_image, gallery_card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=gallery_card) as vm:
        wait_ready(vm)
        script(vm, "key down 1")
        held = time.monotonic()
        time.sleep(HOLD_S)
        script(vm, "key down 0")
        held = time.monotonic() - held
        assert held >= HOLD_S
        # The release logs nothing: wait for the queue to drain.
        time.sleep(0.5)
        rows = [int(s) for s, _ in SEL.findall(vm.serial)]
        assert rows, vm.serial
        assert rows == list(range(1, len(rows) + 1)), rows
        assert rows[-1] >= MIN_ROWS, f"held {held:.2f} s, moved {rows[-1]} rows"
        # The list stops at the end: a held key never wraps.
        assert rows[-1] <= LIST_ROWS - 1
        # Return to the top, one tap per row.
        for row in range(rows[-1] - 1, -1, -1):
            sel, _ = key(vm, "up", SEL)
            assert int(sel) == row
        no_exception(vm)
