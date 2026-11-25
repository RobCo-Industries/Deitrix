"""The INV tab in QEMU: its views, keys, the empty states and INV.SAV.

The tab logs one "deitrix: inv ..." line for each key it takes, before
the redraw. A key that changes nothing logs "deitrix: inv idle". So a
test that needs a frame on screen sends one such key and waits for its
line: the shell draws before it takes the next event. At the list that
key is Back, in the detail it is RIGHT.
"""

import time

from test_deitrix import boot_rom, golden, tabs, wait_ready, wait_tabs
from test_model import card_target, mtools, wait_line
from pipos_testing import TIMEOUT, no_exception, press, screenshot, script
from conftest import serial_line

INV = serial_line(r"deitrix: inv (.*?)")
COUNT = serial_line(r"deitrix: inv count=(\d+)")


def inv_lines(vm):
    return INV.findall(vm.serial)


def key(vm, name, want):
    """Taps a key and waits for the tab's next line, which must be `want`."""
    before = len(inv_lines(vm))
    press(vm, name)
    deadline = time.monotonic() + TIMEOUT
    while len(inv_lines(vm)) <= before:
        assert time.monotonic() < deadline, f"no inv line after {name}\n{vm.serial}"
        time.sleep(0.05)
    got = inv_lines(vm)[before]
    assert got == want, f"{name}: {got!r} != {want!r}\n{vm.serial}"


def shot(vm, tmp_path, name, detail=False):
    """Makes sure the last frame is on screen, then compares it."""
    key(vm, "right" if detail else "back", "idle")
    golden(screenshot(vm, tmp_path), name)


def to_inv(vm, count=1):
    script(vm, "dial 1")
    assert wait_tabs(vm, count)[-1] == "INV", vm.serial


def change_knife(vm, tmp_path=None):
    """Opens the knife, counts 1 to 3, marks it, goes back with Back."""
    key(vm, "select", "open=knife")
    if tmp_path:
        shot(vm, tmp_path, "inv_detail", detail=True)
    key(vm, "up", "count=2")
    key(vm, "up", "count=3")
    key(vm, "select", "marked=1")
    if tmp_path:
        shot(vm, tmp_path, "inv_detail_changed", detail=True)
    key(vm, "back", "back row=0")


def test_inv_views(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_inv(vm)
        shot(vm, tmp_path, "inv")
        # The sub-tabs: RIGHT to FOOD, LEFT back to TOOLS.
        key(vm, "right", "cat=1 row=0")
        shot(vm, tmp_path, "inv_food")
        key(vm, "down", "cat=1 row=1")
        key(vm, "left", "cat=0 row=0")
        key(vm, "up", "end row=0")
        change_knife(vm, tmp_path)
        shot(vm, tmp_path, "inv_changed")
        # In the detail: RIGHT stays, LEFT goes back like Back.
        key(vm, "select", "open=knife")
        key(vm, "right", "idle")
        key(vm, "left", "back row=0")
        # FOOD kept its row.
        key(vm, "right", "cat=1 row=1")
        key(vm, "left", "cat=0 row=0")
        assert tabs(vm) == ["INV"], vm.serial
        # The dial leaves from the detail; back on INV the list shows.
        key(vm, "select", "open=knife")
        script(vm, "dial 1")
        assert wait_tabs(vm, 2)[-1] == "DATA", vm.serial
        script(vm, "dial -1")
        assert wait_tabs(vm, 3)[-1] == "INV", vm.serial
        shot(vm, tmp_path, "inv_changed")
        # LEFT at the first category changes the tab.
        press(vm, "left")
        assert wait_tabs(vm, 4)[-1] == "STAT", vm.serial
        no_exception(vm)


def test_inv_count_refused_at_zero(board, kernel_image, card, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_inv(vm)
        key(vm, "select", "open=knife")
        key(vm, "down", "count=0")
        key(vm, "down", "refused count=0")
        key(vm, "up", "count=1")
        no_exception(vm)


def test_inv_survives_a_reboot(board, kernel_image, card, tmp_path, need_mtools):
    """A count and a mark reach INV.SAV through the autosave, and the
    next boot of the same card shows them in the list."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_inv(vm)
        change_knife(vm)
        wait_line(vm, "deitrix: save INV.SAV ok", timeout=20)
        no_exception(vm)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load INV.SAV ok records=14")
        # The settings save kept INV as the tab: no tab line.
        assert tabs(vm) == [], vm.serial
        shot(vm, tmp_path, "inv_changed")
        no_exception(vm)


def test_inv_no_items_file(board, kernel_image, card, tmp_path, need_mtools):
    mtools("mdel", "-i", card_target(card), "::PIPBOY/DATA/ITEMS.INI")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_inv(vm)
        shot(vm, tmp_path, "inv_empty")
        key(vm, "select", "idle")
        key(vm, "down", "idle")
        no_exception(vm)


EMPTY_CAT = b"""version = 1
[categories]
tools = TOOLS
spare = SPARE
[item.knife]
name = Folding knife
cat = tools
"""


def test_inv_empty_category(board, kernel_image, card, tmp_path, need_mtools):
    items = tmp_path / "ITEMS.INI"
    items.write_bytes(EMPTY_CAT)
    mtools("mcopy", "-o", "-i", card_target(card), str(items), "::PIPBOY/DATA/ITEMS.INI")
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_inv(vm)
        key(vm, "right", "cat=1 row=0")
        shot(vm, tmp_path, "inv_empty_cat")
        key(vm, "select", "idle")
        press(vm, "right")
        assert wait_tabs(vm, 2)[-1] == "DATA", vm.serial
        no_exception(vm)


# The kernel repeats a held key 500 ms after the press, then every
# 100 ms. Held for HOLD_S without a stall that is the press and 11
# repeats. After a stall the kernel restarts the schedule, so, like the
# gallery's hold test, the test asks for MIN_STEPS only.
HOLD_S = 1.6
MIN_STEPS = 5


def test_inv_hold_counts_up(board, kernel_image, card, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_inv(vm)
        key(vm, "down", "cat=0 row=1")
        key(vm, "select", "open=rope")
        script(vm, "key up 1")
        held = time.monotonic()
        time.sleep(HOLD_S)
        script(vm, "key up 0")
        held = time.monotonic() - held
        assert held >= HOLD_S
        # Repeats queued before the release still arrive: wait until the
        # lines stop, then sync.
        seen = -1
        while len(inv_lines(vm)) != seen:
            seen = len(inv_lines(vm))
            time.sleep(0.5)
        key(vm, "right", "idle")
        counts = [int(c) for c in COUNT.findall(vm.serial)]
        # One step per event, from the file's 20.
        assert counts == list(range(21, 21 + len(counts))), counts
        assert len(counts) >= MIN_STEPS, f"held {held:.2f} s, {len(counts)} steps"
        no_exception(vm)


def test_inv_held_left_closes_the_detail_once(board, kernel_image, card, need_mtools):
    """A held LEFT in the detail closes it, and its repeats change
    neither the category nor the tab: the shell's held-key rule."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        to_inv(vm)
        key(vm, "right", "cat=1 row=0")
        key(vm, "select", "open=bread")
        before = len(inv_lines(vm))
        script(vm, "key left 1")
        held = time.monotonic()
        time.sleep(1.5)
        script(vm, "key left 0")
        assert time.monotonic() - held >= 1.5
        seen = -1
        while len(inv_lines(vm)) != seen:
            seen = len(inv_lines(vm))
            time.sleep(0.5)
        # Back at the list logs idle: the frame is on screen.
        key(vm, "back", "idle")
        assert inv_lines(vm)[before:] == ["back row=0", "idle"], vm.serial
        assert tabs(vm) == ["INV"], vm.serial
        no_exception(vm)
