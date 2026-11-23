# Testing

## Layers

| Layer | Command | What it covers |
|---|---|---|
| Host unit tests | `make test-host` | Pure code in `src/model/`, `src/ui/`, `src/screens/` and the app shell, against a fake platform; `tools/pbm2c.py` |
| Pin checks | `make check-pins`, part of `test-host` | The CI workflow pins and the commit links in the docs |
| Card check | `make sdimage-test`, part of `test-host` | The card holds the ROM, the kernel, `PIPOS.CFG` and every file of `content/` |
| QEMU tests | `make test-target` | The ROM and the widget gallery on Pip-OS on `raspi0`, `raspi1ap`, `raspi2b` and `raspi3b` |

`make test` runs `test-host`, then `test-target`. `BOARD=raspi0` limits the QEMU tests to one board.

## Host tests

Each `tests/host/test_<name>.c` builds into one program with RobCo-Core's `mk/host.mk`: `-Werror`, AddressSanitizer and UndefinedBehaviorSanitizer when the compiler has them. Every program links all of `src/` except `src/platform/`, the generated `build/gen/icons.c` and `tests/host/fake_platform.c`. The fake records draw calls, log lines and tones, and plays back scripted events, clock readings and ticks.

| File | Covers |
|---|---|
| `test_nav.c` | Next and previous, wrap, several detents both ways, extreme deltas, any tab count |
| `test_clock.c` | `00:00`, `23:59`, `--:--`, the minute of the day, the wait to the next minute |
| `test_ui.c` | The theme shades, event translation, the sound for each outcome, the tones, number text, cut text |
| `test_frame.c` | The header and footer draw lists, the active-tab block, the tab pad with many tabs, the placeholder body, the ROM's screen table |
| `test_app.c` | One `present` per redraw, the clip around the body, no redraw without a change, the log lines, the wait timeout, the scheme colour, routing of the dial, LEFT, RIGHT, BACK and other keys, `enter` and `leave`, the hint, `tick` and the wake time |
| `test_list.c` | Selection, the ends, the scroll window for every size up to 30 rows, draw lists, the scroll bar, cut labels, the empty message, coordinates inside the area |
| `test_text.c` | Wrap cases with exact breaks, no line past the width, scrolling, a draw list, coordinates inside the area |
| `test_widgets.c` | Gauge clamps and draw lists, key and value rows, sub-tab ends and draw list, the generated icons |
| `test_vfont.c` | Every glyph inside its cell, no two glyphs alike, the crossed box for unknown characters, widths, a draw list |

`tests/host/test_pbm2c.py` tests the PBM converter: parsing, bit packing, the generated table, bad input and the files in `assets/`. `make test-host` runs it.

To add one, write `tests/host/test_<name>.c` with `TEST(name) { CHECK(...); }` from RobCo-Core's `rc_test.h`. `make test-host` finds it.

The shell tests are `tests/host/check_pipos_links_test.sh` and `tests/host/sdimage_test.sh`.

### Draw-list tests

A widget test draws into the fake and compares `fake.draws` with the exact list of calls, one per line:

```
rect 0 38 320 16 fg fill
text 12 38 "Spare fuse" bg fg s1
```

Colours print as theme names, so a change of scheme colour does not touch the tests. Keep each list short: a 3-row list, 2 lines of text. Sweeps check a property instead: `fake_points_inside()` says whether every recorded point lies in the widget's area.

When a draw list changes on purpose, run the test, read the "got" side of the failed `CHECK_STR`, check every changed line against the change you meant, and paste it in.

## QEMU tests

`tests/test_deitrix.py` boots the card from `make sdimage` with the Pip-OS kernel. `tests/test_gallery.py` boots the gallery's card from `make gallery`. It uses RobCo-Core's harness and Pip-OS's `tests/pipos_helpers.py`, both from the submodule. Each test waits for `deitrix: ready` on serial, then drives the ROM through the script channel.

| Test | Checks |
|---|---|
| `test_first_screen` | The STAT screen matches `golden/stat.png`, the ROM still runs |
| `test_navigation` | The dial and the LEFT and RIGHT keys give the expected `deitrix: tab=` lines, with wraps both ways; the INV screen matches `golden/inv.png`; keys that do nothing log nothing; the ROM still runs |
| `test_clock_shown` | With a ds1338 RTC at 09:47:00, the header clock matches `golden/clock_0947.png` |
| `test_clock_minute_change` | With the RTC at 09:47:30, the clock first shows 09:47, then 09:48 within 10 s after the minute |
| `test_gallery_screens` | Each gallery screen matches its golden, `golden/gallery_<name>.png`; RIGHT moves through the sub-tabs before it leaves TABS; RIGHT on FONT wraps to LIST; the dial goes back to TABS, which kept its last sub-tab |
| `test_gallery_list_and_text` | The list moves down to its last row and back, with the window at each step; DOWN at the end and UP at the top do not move it; the text scrolls to its end and back; LIST keeps its state across a tab change; goldens at both ends |
| `test_gallery_hold_repeats` | DOWN held for 1.6 s moves the list one row at a time, at least 5 rows, never past the end |

The first two tests boot without an RTC, so the header shows `--:--` and the goldens do not depend on the time. The goldens are 640x480 QEMU screenshots, or crops of them, and must match on every board pixel for pixel.

The gallery logs each list and text move before its redraw. To be sure a frame is on screen, the tests send one more key that changes nothing and wait for its line: the shell draws before it takes the next event.

`test_gallery_hold_repeats` depends on timing. Pip-OS repeats a held key 500 ms after the press, then every 100 ms, so 1.6 s gives the press and about 11 repeats. After a stall the kernel restarts the schedule, so the test asks for 5 rows only.

`test_clock_minute_change` depends on timing. The ROM redraws at most about 1 s after the minute changes. The test allows 10 s more. If it fails on a loaded machine, rerun it alone before you conclude:

```sh
.venv/bin/pytest tests -k minute_change --board raspi2b
.venv/bin/pytest tests -k hold_repeats --board raspi2b
```

## Goldens

After a deliberate change to the screen, write the goldens again on one board, then check every board:

```sh
make sdimage kernel gallery venv
DEITRIX_UPDATE_GOLDEN=1 .venv/bin/pytest tests --board raspi2b
make test-target
```

Look at each changed PNG before you commit it.
