"""The holotape slot of ABI 1.4 in QEMU, with Pip-OS's test backend.

QEMU has no SD card on SPI0. `make holotest-kernel` builds Pip-OS with
-DPIPOS_HOLO_TEST, which serves slot cards from guest RAM: each card
image goes in with QEMU's -device loader, card n at 0x02000000 +
n * 0x00800000, and the `holo` script command inserts, pulls and breaks
them. The card's PIPOS.CFG gets `[holotape] slot = on`.

Deitrix logs these lines for the slot:

    deitrix: tape slot on | off                  at start
    deitrix: tape slot bits=<n>                  holo_eject_event's bits
    deitrix: notice <text>, or "-"               the footer notice changes
    deitrix: tapes list n=<n> <FILE> <FILE>*     each listing; * is a slot tape

What this cannot check: the real slot driver, a real card, real timing.
Nothing here ran on hardware.
"""

import re
import subprocess
import time

import pytest

from test_deitrix import boot_rom, golden, rom_running, tabs, wait_ready, wait_tabs
from test_data import TAPE_RC, TAPES, TAPES_LINE, add_tapes, card, tapes, to_tapes  # noqa: F401
from test_model import card_target, mtools, wait_line
from test_stat import count, key, quiet, settled, wait_count
from pipos_testing import TIMEOUT, make_card, minimal_env, no_exception, press, script
from conftest import serial_line

CARD_BASE = 0x02000000
CARD_STRIDE = 0x00800000
CARD_MIB = 8
CARD_BLOCKS = CARD_MIB * 2048

BITS = serial_line(r"deitrix: tape slot bits=(\d+)")
NOTICE = serial_line(r"deitrix: notice (.*?)")
LIST = serial_line(r"deitrix: tapes list n=(-?\d+)((?: \S+)*)")
TAB_LINE = serial_line(r"deitrix: tab=(\w+)")
IN, OUT, BAD = "Tape slot: card loaded.", "Tape slot: card removed.", \
    "Tape slot: card unreadable."


def slot_on(card, value="on"):
    """Adds `[holotape] slot = <value>` to the card's PIPOS.CFG."""
    target = card_target(card)
    cfg = subprocess.run(["mtype", "-i", target, "::PIPBOY/PIPOS.CFG"], check=True,
                         env=minimal_env(MTOOLS_SKIP_CHECK="1"), capture_output=True).stdout
    path = card.parent / "PIPOS.CFG"
    path.write_bytes(cfg + f"\n[holotape]\nslot = {value}\n".encode())
    mtools("mcopy", "-o", "-i", target, str(path), "::PIPBOY/PIPOS.CFG")


def slot_card(tmp_path, n, files):
    """Slot card n: files ({"NAME.HTP": path}) in its root, or None for a
    card without a FAT volume."""
    path = tmp_path / f"slot{n}.img"
    if files is None:
        with open(path, "wb") as f:
            f.truncate(CARD_MIB << 20)
        return path
    return make_card(path, {name: src.read_bytes() for name, src in files.items()},
                     size_mib=CARD_MIB)


def loader(cards):
    args = []
    for n, path in cards.items():
        args += ["-device",
                 f"loader,file={path},addr={CARD_BASE + n * CARD_STRIDE:#x},force-raw=on"]
    return args


def holo(vm, *words):
    reply = vm.script("holo " + " ".join(words), timeout=TIMEOUT)
    assert reply.status == "ok", reply
    return reply


def insert(vm, n):
    holo(vm, "insert", str(n), str(CARD_BLOCKS))


def wait_bits(vm, n):
    """Waits for the n-th bits line; returns its value."""
    return int(wait_count(vm, BITS, n)[n - 1])


def last_list(vm):
    n, files = LIST.findall(vm.serial)[-1]
    return int(n), files.split()


def wait_list(vm, files, frame=True):
    """Waits for a listing of exactly these files, * for a slot tape.
    With frame, also for the TAPES frame line after it, so a key sent
    next is counted after that frame."""
    body = " ".join([f"n={len(files)}"] + files)
    line = serial_line(re.escape(f"deitrix: tapes list {body}"))
    deadline = time.monotonic() + TIMEOUT
    while True:
        found = list(line.finditer(vm.serial))
        if found and (not frame or TAPES_LINE.search(vm.serial, found[-1].end())):
            return
        assert time.monotonic() < deadline, f"no listing {body}\n{vm.serial}"
        time.sleep(0.05)


def wait_notice(vm, text):
    vm.wait_for(serial_line(re.escape(f"deitrix: notice {text}")), timeout=TIMEOUT)


def boot(kernel, board, card, cards):
    return boot_rom(kernel, board, sd=card, extra_args=loader(cards))


def test_slot_insert_and_pull(board, holotest_kernel, card, tmp_path, need_mtools, tapes):
    """A card goes in while TAPES shows: the list follows, the selection
    stays on its tape, a notice shows; goldens of both. Then the same
    on another tab."""
    add_tapes(card, tapes, ["linetest"])
    slot_on(card)
    cards = {0: slot_card(tmp_path, 0, {"FAULT.HTP": tapes["fault"]})}
    with boot(holotest_kernel, board, card, cards) as vm:
        wait_ready(vm)
        assert "deitrix: tape slot on" in vm.serial, vm.serial
        assert to_tapes(vm) == ("0", "0", "1", "")
        assert last_list(vm) == (1, ["LINETEST.HTP"])
        before = count(vm, TAPES_LINE)
        insert(vm, 0)
        assert wait_bits(vm, 1) == 1
        # LINETEST stays selected, now on the second row.
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == ("1", "0", "2", "")
        assert last_list(vm) == (2, ["FAULT.HTP*", "LINETEST.HTP"])
        wait_notice(vm, IN)
        golden(settled(vm, tmp_path), "data_tapes_slot_notice")
        # A key works while the notice shows.
        assert key(vm, "up", TAPES_LINE) == ("0", "0", "2", "")
        wait_notice(vm, "-")
        golden(settled(vm, tmp_path), "data_tapes_slot")

        # The selected slot tape is pulled: the row stays, on LINETEST.
        before = count(vm, TAPES_LINE)
        holo(vm, "pull")
        assert wait_bits(vm, 2) == 2
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == ("0", "0", "1", "")
        assert last_list(vm) == (1, ["LINETEST.HTP"])
        wait_notice(vm, OUT)

        # On MAP: the notices show there, the tab stays.
        n = len(tabs(vm))
        script(vm, "dial 1")
        assert wait_tabs(vm, n + 1)[-1] == "MAP"
        insert(vm, 0)
        assert wait_bits(vm, 3) == 1
        wait_notice(vm, IN)
        assert tabs(vm)[-1] == "MAP" and len(tabs(vm)) == n + 1, vm.serial
        # Back on DATA, TAPES lists the slot tape.
        before = count(vm, TAPES_LINE)
        script(vm, "dial -1")
        assert wait_tabs(vm, n + 2)[-1] == "DATA"
        press(vm, "right")
        wait_count(vm, TAPES_LINE, before + 1)
        wait_list(vm, ["FAULT.HTP*", "LINETEST.HTP"])
        rom_running(vm)
        no_exception(vm)


def test_slot_tape_shadows_a_boot_tape(board, holotest_kernel, card, tmp_path, need_mtools,
                                       tapes):
    """A slot LINETEST.HTP, here the fault tape, hides the boot card's:
    one row, marked, and SELECT runs the slot one. The boot tape's result
    line does not carry over to it. The pull brings the boot card's back."""
    add_tapes(card, tapes, ["linetest"])
    slot_on(card)
    cards = {0: slot_card(tmp_path, 0, {"LINETEST.HTP": tapes["fault"]})}
    with boot(holotest_kernel, board, card, cards) as vm:
        wait_ready(vm)
        to_tapes(vm)
        before = count(vm, TAPES_LINE)
        press(vm, "select")
        assert wait_count(vm, TAPES_LINE, before + 1)[-1][3] != ""
        before = count(vm, TAPES_LINE)
        insert(vm, 0)
        wait_bits(vm, 1)
        wait_notice(vm, IN)
        wait_list(vm, ["LINETEST.HTP*"], frame=False)
        # The slot tape is another tape: the result line goes.
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == ("0", "0", "1", "")
        before = count(vm, TAPES_LINE)
        press(vm, "select")
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == \
            ("0", "0", "1", "Tape was stopped by the system.")
        assert "faulttest: fault now" in vm.serial, vm.serial
        holo(vm, "pull")
        wait_bits(vm, 2)
        wait_list(vm, ["LINETEST.HTP"], frame=False)
        no_exception(vm)


def test_slot_bad_card(board, holotest_kernel, card, tmp_path, need_mtools, tapes):
    """A card without a FAT volume: the notice says so, the list stays."""
    add_tapes(card, tapes, ["linetest"])
    slot_on(card)
    with boot(holotest_kernel, board, card, {0: slot_card(tmp_path, 0, None)}) as vm:
        wait_ready(vm)
        to_tapes(vm)
        insert(vm, 0)
        assert wait_bits(vm, 1) == 4
        wait_notice(vm, BAD)
        assert last_list(vm) == (1, ["LINETEST.HTP"])
        holo(vm, "pull")
        assert wait_bits(vm, 2) == 2
        wait_notice(vm, OUT)
        no_exception(vm)


def test_slot_tape_pulled_while_it_runs(board, holotest_kernel, card, tmp_path, need_mtools,
                                        tapes):
    """The kernel ends the tape with -PIP_ENODEV: DATA shows the reason,
    the list loses the tape, the footer says the card left."""
    add_tapes(card, tapes, ["fault"])
    slot_on(card)
    cards = {0: slot_card(tmp_path, 0, {"LINETEST.HTP": tapes["linetest"]})}
    with boot(holotest_kernel, board, card, cards) as vm:
        wait_ready(vm)
        to_tapes(vm)
        insert(vm, 0)
        wait_bits(vm, 1)
        wait_list(vm, ["LINETEST.HTP*", "FAULT.HTP"])
        # FAULT kept the selection: up to the slot tape.
        assert key(vm, "up", TAPES_LINE) == ("0", "0", "2", "")
        before = count(vm, TAPES_LINE)
        press(vm, "select")
        wait_line(vm, 'linetest: ready args ""')
        holo(vm, "pull")
        # LINETEST sleeps in wait_event: Pip-OS ends it at its next
        # syscall, which a key other than SELECT brings.
        vm.wait_for(re.compile(r"ev=holo_slot state=empty"), timeout=TIMEOUT)
        press(vm, "up")
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == \
            ("0", "0", "1", "Tape pulled while running.")
        assert TAPE_RC.findall(vm.serial)[-1] == "-41", vm.serial
        assert "ev=holo_kill reason=eject" in vm.serial, vm.serial
        assert last_list(vm) == (1, ["FAULT.HTP"])
        assert wait_bits(vm, 2) == 2
        wait_notice(vm, OUT)
        # DATA works on: FAULT runs and its own result shows.
        before = count(vm, TAPES_LINE)
        press(vm, "select")
        assert wait_count(vm, TAPES_LINE, before + 1)[-1] == \
            ("0", "0", "1", "Tape was stopped by the system.")
        rom_running(vm)
        no_exception(vm)


ATTACH_MS = 3000


def test_slot_slow_attach(board, holotest_kernel, card, tmp_path, need_mtools, tapes):
    """While a slow card attaches, the list shows no slot tape and the
    keys work; the tape shows once the card is mounted."""
    add_tapes(card, tapes, ["linetest", "fault"])
    slot_on(card)
    cards = {0: slot_card(tmp_path, 0, {"SLOW.HTP": tapes["linetest"]})}
    with boot(holotest_kernel, board, card, cards) as vm:
        wait_ready(vm)
        to_tapes(vm)
        holo(vm, "delay", str(ATTACH_MS))
        insert(vm, 0)
        vm.wait_for(re.compile(r"ev=holo_attach_start"), timeout=TIMEOUT)
        # Keys during the attach: each one answers at once.
        assert key(vm, "down", TAPES_LINE) == ("1", "0", "2", "")
        assert key(vm, "up", TAPES_LINE) == ("0", "0", "2", "")
        reply = holo(vm, "pending")
        pending = reply.lines[0].startswith("attaching=1")
        assert count(vm, BITS) == 0 or not pending, vm.serial
        if pending:
            # Still attaching after the keys: none of it showed yet.
            assert last_list(vm) == (2, ["LINETEST.HTP", "FAULT.HTP"]), vm.serial
            assert NOTICE.findall(vm.serial) == [], vm.serial
        assert wait_bits(vm, 1) == 1
        wait_list(vm, ["SLOW.HTP*", "LINETEST.HTP", "FAULT.HTP"])
        assert pending, "the attach ended before the keys: the run did not meet its case"
        no_exception(vm)


def test_slot_off_shows_nothing(board, holotest_kernel, card, tmp_path, need_mtools, tapes):
    """With `slot = off` the kernel reports no slot: Deitrix never asks,
    shows no notice and no slot mark, whatever the backend does."""
    add_tapes(card, tapes, ["linetest"])
    slot_on(card, "off")
    cards = {0: slot_card(tmp_path, 0, {"FAULT.HTP": tapes["fault"]})}
    with boot(holotest_kernel, board, card, cards) as vm:
        wait_ready(vm)
        assert "deitrix: tape slot off" in vm.serial, vm.serial
        to_tapes(vm)
        vm.script("holo insert 0 " + str(CARD_BLOCKS), timeout=TIMEOUT)
        press(vm, "down")
        quiet(vm, BITS)
        assert count(vm, NOTICE) == 0, vm.serial
        assert all("*" not in files for _, files in LIST.findall(vm.serial)), vm.serial
        no_exception(vm)


def test_shipped_kernel_has_no_slot(board, kernel_image, card, need_mtools):
    """The kernel of the card, with its sample PIPOS.CFG: no slot."""
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        assert "deitrix: tape slot off" in vm.serial, vm.serial
        no_exception(vm)
