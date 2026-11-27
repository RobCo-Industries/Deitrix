"""Saves the serial log of one whole boot with the sound on, for `make boot-sound`.

It runs only with --boot-sound-log PATH. The card is the one make
sdimage writes, with Deitrix as the ROM, and its PIPOS.CFG set to 20 ms
per character, 200 ms per line, volume 80, the beep and the system sound
on. So the log holds the kernel's boot sounds, the scroll's blips, the
weep under Deitrix's first frame and the clicks of Deitrix's card reads
after it. The test waits 3 s after the weep, then until no audio record
has come for 1 s. Pip-OS's tools/boot_sound.py turns the log into a WAV
file.
"""

import time

import pytest

from pipos_testing import TIMEOUT, config_summary, no_exception
from test_deitrix import boot_rom, wait_ready
from test_model import card_target, mtools
from test_weep import WEEP

CFG_PATH = "::PIPBOY/PIPOS.CFG"
CFG = ("[display]\nscheme = green\nchar_delay_ms = 20\nline_delay_ms = 200\n"
       "[audio]\nvolume = 80\nbeep = yes\nsystem_sound = yes\n")
AFTER_WEEP_S = 3.0


def wait_quiet(vm, quiet_s=1.0, limit_s=30.0):
    """Waits until no audio record has come for quiet_s."""
    end = time.monotonic() + limit_s
    seen, last = vm.serial.count("ev=audio "), time.monotonic()
    while time.monotonic() < end:
        time.sleep(0.25)
        now = vm.serial.count("ev=audio ")
        if now != seen:
            seen, last = now, time.monotonic()
        elif time.monotonic() - last >= quiet_s:
            return
    pytest.fail("the sounds did not stop\n" + vm.serial)


def test_save_boot_log(request, board, kernel_image, card, tmp_path, need_mtools):
    path = request.config.getoption("--boot-sound-log")
    cfg = tmp_path / "PIPOS.CFG"
    cfg.write_text(CFG)
    mtools("mcopy", "-o", "-i", card_target(card), str(cfg), CFG_PATH)
    with boot_rom(kernel_image, board, sd=card) as vm:
        wait_ready(vm)
        vm.wait_for(WEEP, timeout=TIMEOUT)
        time.sleep(AFTER_WEEP_S)
        wait_quiet(vm)
        summary = config_summary(vm)
        no_exception(vm)
        log = vm.serial
    with open(path, "w", encoding="utf-8") as f:
        f.write(log)
    assert (summary["char_delay_ms"], summary["key_click"], summary["volume"]) == ("20", "1", "80"), log
    for what, mark in (("rest ramp", " ev=audio op=pcm rate=22050 len=882 "),
                       ("soft beep", " ev=audio op=pcm rate=37500 len=2250 "),
                       ("disk click", " ev=audio op=pcm rate=37500 len=789 "),
                       ("scroll blip", " ev=audio op=pcm rate=37500 len=600 "),
                       ("weep", " ev=audio op=pcm rate=37500 len=5250 ")):
        if mark not in log:
            pytest.fail("the boot played no %s\n%s" % (what, log))
