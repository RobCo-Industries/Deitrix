"""The weep in QEMU: the chirp Deitrix plays under its first frame.

QEMU has no sound device, so the test reads the audio trace. The weep
is one "op=pcm rate=37500 len=5250" line with the CRC-32 of
src/services/weep_table.c. It needs ABI 1.5 and an output that rests at
its midpoint: Pip-OS sets PIP_HW_AUDIO_MID only with [audio]
system_sound on. So the stock card, with the sound off, plays no weep
and keeps its goldens.
"""

import re
import struct
import subprocess
import sys
import time
import zlib

from conftest import ROOT, serial_line
from pipos_testing import TIMEOUT, minimal_env, no_exception, screenshot
from test_deitrix import boot_rom, golden, wait_ready
from test_model import card_target, down_to, mtools, select, to_radio, wait_line

sys.path.insert(0, str(ROOT) + "/tools")
import mkweep  # noqa: E402

CFG = "::PIPBOY/PIPOS.CFG"
WEEP_CRC = zlib.crc32(mkweep.build(mkweep.load_params()))
WEEP = serial_line(r"@RC stage=pipos ev=audio op=pcm rate=37500 len=5250 .*?")
# Every sample play: its rate, its length and its CRC.
PCM = serial_line(r"@RC stage=pipos ev=audio op=pcm rate=(\d+) len=(\d+) .*?crc=([0-9a-fA-F]+).*?")
READY = "deitrix: ready"
# The trace is flushed late: how long a test waits for a line that must
# not come.
SETTLE_S = 2.0


def sound_on(card, tmp_path):
    """Turns [audio] system_sound on in the card's PIPOS.CFG."""
    text = subprocess.run(["mtype", "-i", card_target(card), CFG], check=True,
                          env=minimal_env(MTOOLS_SKIP_CHECK="1"),
                          capture_output=True, text=True).stdout
    text, n = re.subn(r"(?m)^\[audio\]$", "[audio]\nsystem_sound = yes", text)
    assert n == 1, text
    cfg = tmp_path / "PIPOS.CFG"
    cfg.write_text(text)
    mtools("mcopy", "-o", "-i", card_target(card), str(cfg), CFG)


def ui_sounds_off(card, tmp_path):
    """Writes a DEITRIX.SAV with UI sounds off: tab 0, volume 80, no station."""
    write_settings(card, tmp_path, ui_sound=0)


def write_settings(card, tmp_path, ui_sound):
    """Writes a DEITRIX.SAV with this UI sounds value: tab 0, volume 80, no station."""
    payload = bytes([0, 80, ui_sound, 0]) + bytes(4)
    data = b"DXST" + struct.pack("<HHI", 1, 1, zlib.crc32(payload)) + payload
    path = tmp_path / "DEITRIX.SAV"
    path.write_bytes(data)
    target = card_target(card)
    subprocess.run(["mmd", "-i", target, "::/PIPBOY/SAVE"],
                   env=minimal_env(MTOOLS_SKIP_CHECK="1"), capture_output=True)
    mtools("mcopy", "-o", "-i", target, str(path), "::/PIPBOY/SAVE/DEITRIX.SAV")


def plays(vm):
    """[(rate, len, crc)] of every sample play in the trace."""
    return [(int(m[1]), int(m[2]), int(m[3], 16)) for m in PCM.finditer(vm.serial)]


def test_weep_after_the_first_frame(board, kernel_image, card, tmp_path, need_mtools):
    sound_on(card, tmp_path)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        vm.wait_for(WEEP, timeout=TIMEOUT)
        ready = vm.serial.index(READY)
        assert vm.serial.index("ev=audio op=pcm rate=37500 len=5250 ") > ready, vm.serial
        weeps = [p for p in plays(vm) if p[:2] == (37500, 5250)]
        assert weeps == [(37500, 5250, WEEP_CRC)], vm.serial
        # A PCM station plays at 8000 Hz: the rate came back.
        to_radio(vm)
        down_to(vm, 3)
        mark = len(vm.serial)
        select(vm, "deitrix: radio station=4")
        deadline = time.monotonic() + TIMEOUT
        while not [p for p in PCM.finditer(vm.serial, mark) if p[1] == "8000"]:
            assert time.monotonic() < deadline, vm.serial[mark:]
            time.sleep(0.1)
        time.sleep(SETTLE_S)
        weeps = [p for p in plays(vm) if p[:2] == (37500, 5250)]
        assert len(weeps) == 1, vm.serial
        no_exception(vm)


def test_no_weep_with_the_sound_off(board, kernel_image, card, tmp_path, need_mtools):
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        time.sleep(SETTLE_S)
        golden(screenshot(vm, tmp_path), "stat")
        assert "len=5250" not in vm.serial, vm.serial
        assert "rate=37500" not in vm.serial, vm.serial
        no_exception(vm)


def test_no_weep_with_ui_sounds_off(board, kernel_image, card, tmp_path, need_mtools):
    sound_on(card, tmp_path)
    ui_sounds_off(card, tmp_path)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        wait_line(vm, "deitrix: load DEITRIX.SAV ok tab=0 volume=80 sound=0 station=0")
        time.sleep(SETTLE_S)
        assert "len=5250" not in vm.serial, vm.serial
        no_exception(vm)
