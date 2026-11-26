# Assets

This page lists every asset Deitrix uses or ships: where it comes from, how it is made and under which licence.

Deitrix has booted on one real board, a Pi Zero W 1.1, with no screen and no speaker wired. Nothing here has been seen or heard on real hardware.

## Summary

| Asset | Where | Made by | Licence |
|---|---|---|---|
| The text font | Pip-OS kernel, from RobCo-Core | Fixedsys Excelsior 3.02, converted by RobCo-Core's `tools/font2c.py` | Public domain or CC0, as its author states. See [The text font](#the-text-font). |
| The stroke font | `src/ui/vfont.c` | Written for Deitrix, as code | MIT, the Deitrix [LICENSE](../LICENSE) |
| The icons | `assets/*.pbm` | Drawn for Deitrix as PBM files | MIT |
| The sample content | `content/PIPBOY/DATA/` | Written for Deitrix | MIT |
| The tunes | `content/PIPBOY/DATA/RADIO/*.TUN` | Written for Deitrix in the `.TUN` format | MIT |
| The PCM tracks | Generated into `build/pcm/` | `tools/mkpcm.py`, by synthesis | MIT |
| The UI tones | `src/ui/core.c` | Four tones as numbers in the code | MIT |
| The tape slot notices and the `SLOT` mark | `src/services/services.h`, `src/screens/data_tapes.c` | Written for Deitrix | MIT |
| Culvert's icon | `holotapes/culvert/icon.pbm` | Drawn for Culvert as a PBM file | MIT |
| Culvert's pipe pieces and screens | `holotapes/culvert/draw.c` | Drawn as code with Pip-OS's drawing calls | MIT |
| Culvert's levels | `holotapes/culvert/logic.c` | Generated from the level number, not stored | MIT |
| Culvert's sounds | `holotapes/culvert/main.c` | Notes as numbers in the code | MIT |
| Culvert's text | `holotapes/culvert/draw.c` | Written for Culvert | MIT |
| The test tapes' icon | `tests/tapes/linetest/icon.pbm` | Drawn for the tests as a PBM file. Never on the default card. | MIT |
| The manual's pictures | `tests/golden/*.png` | Screenshots of the QEMU tests | MIT |

Deitrix's own files are under the MIT licence in [LICENSE](../LICENSE), copyright 2026 RobCo Industries. The README states that all artwork, sounds, music, maps and text in this repository are original and that nothing is copied from the Fallout games. Only the text font is not original to this project: it comes from Pip-OS.

## The text font

Deitrix draws all normal text through Pip-OS's `text` call. It ships no font of its own. The kernel draws an 8 by 16 font from RobCo-Core, the library inside Pip-OS.

RobCo-Core holds two font tables. Its [FONTS.md](https://github.com/RobCo-Industries/RobCo-Core/blob/712db81c9c804d4a8ede1f866ea764c0c6299844/docs/FONTS.md) describes both:

| Font | Table | Source | Licence |
|---|---|---|---|
| Fixedsys Excelsior 3.02, the default | `src/gfx/font8x16_fixedsys.c` | `third_party/fixedsys-excelsior/FSEX302.ttf` | No licence file. See below. |
| Spleen 8x16 | `src/gfx/font8x16_spleen.c` | `third_party/spleen/spleen-8x16.bdf` | BSD 2-Clause, `third_party/spleen/LICENSE`, copyright 2018-2026 Frederic Cambus |

`ROBCO_FONT` picks the table. Pip-OS's `kernel/robco_config.h` does not set it, so the kernel uses the default, Fixedsys Excelsior. Spleen is not in the image.

Fixedsys Excelsior is a pixel font by Darien Valentine. RobCo-Core took `FSEX302.ttf` unchanged from <https://github.com/kika/fixedsys>, release `v3.09.10`. Its `third_party/fixedsys-excelsior/README.md` and `PROVENANCE` record the commit and the SHA-256. They state that the font file has no licence fields and the upstream repository has no licence file. The terms are two statements:

- The upstream README says the creators released it to the public domain, and under the Creative Commons Zero Dedication where that is not permitted.
- The author wrote in [kika/fixedsys#21](https://github.com/kika/fixedsys/issues/21#issuecomment-511143610) that the font is "intended to be free to use and alter as you see fit".

The generated table names its source, its SHA-256 and the command that made it. RobCo-Core states that a table inherits the licence of its source font.

The font holds printable ASCII, the block and shade characters and the box-drawing set. Deitrix shows any byte above 0x7E as a box.

## The stroke font

The large text, such as the owner's name on STAT and the time on DATA, CLOCK, uses a stroke font. It lives in `src/ui/vfont.c` as code: each glyph is a few polylines on a grid 4 units wide and 6 high. It has digits, A to Z, space and `: - . / %`. It is not made from any font file. It was written for Deitrix.

## The icons

`assets/` holds three 24 by 24 icons as plain-text PBM files: `note.pbm`, `pin.pbm` and `speaker.pbm`. The second line of each file says it is "an original 24x24 icon for Deitrix". Only the widget gallery shows them today.

At build time `tools/pbm2c.py` turns every `assets/*.pbm` into `build/gen/icons.c`. That file is not committed. Each icon is named by its file name without `.pbm`.

## The sample content

`content/PIPBOY/DATA/` holds the sample card: a profile, 14 items in 4 categories, 4 notes, two maps and five radio stations. The text, the items, the place names and the map lines were written for Deitrix. The maps are hand-written coordinates in `MAP.INI`, 0 to 999 on each axis. They show no real place. The formats are in [CONTENT.md](CONTENT.md).

## The tunes

Three stations play tunes: `MORNING.TUN`, `WALK.TUN` and `LULLABY.TUN`. Each is a short text file of notes in the `.TUN` format of [CONTENT.md](CONTENT.md#tunes-tun-files). A comment at the top of each gives its key and metre, such as "a slow waltz in F major". The ROM turns a tune into notes for Pip-OS's `play_seq` call.

`tools/tun2notes.py` is the reference: it prints the notes the ROM plays, and `--wav <out.wav>` renders them with a square wave.

## The PCM tracks

Two stations play recordings: `HEARTH.PCM` and `DRIFT.PCM`. Git does not hold them. `tools/mkpcm.py` makes them at build time:

- Hearth: a pad, a bass and a slow arpeggio over C, Am, F, G, C, Em, F, G. 60 bpm, 32 seconds.
- Drift: a drone in D, soft wind and bell notes from a D minor pentatonic picked by a fixed seed. 75 bpm, 32 seconds.

The synthesis uses integers only, so the bytes are the same on every machine. Each track loops without a click. The output is raw 8-bit unsigned mono at 8000 Hz, as [CONTENT.md](CONTENT.md#pcm-tracks-pcm-files) describes. `--wav` also writes WAV files to listen to on a computer.

## The UI tones

The key tones are not files. `src/ui/core.c` holds four tones as a frequency and a length each: MOVE 1400 Hz for 12 ms, SELECT 1900 Hz for 30 ms, BACK 900 Hz for 30 ms and DENY 180 Hz for 70 ms. Pip-OS plays them with its `tone` call.

## Culvert

The Culvert holotape lives in `holotapes/culvert/`. Every asset in it was made for Culvert and is under the MIT licence of [LICENSE](../LICENSE). Its Makefile hands them to the SDK, which seals them into `CULVERT.HTP`.

| Asset | File | What it is |
|---|---|---|
| Icon | `icon.pbm` | A 32 by 32 PBM drawing of a pipe elbow. Deitrix shows it on DATA, TAPES. |
| Pipe pieces | `draw.c` | The end cap, straight, elbow, tee, cross and source ring, drawn with rectangles at run time. No image file. |
| Title strip | `draw.c` | Seven pipe tiles on the title screen, drawn from the same code |
| Levels | `logic.c` | A random tree grown from a seed set by the level number. No level is stored. |
| Turn tick | `main.c` | One tone, 900 Hz for 12 ms |
| Connect notes | `main.c` | Two rising notes, 1320 Hz then 1760 Hz |
| Solved tune | `main.c` | Six notes and a rest, from 784 Hz up to 1568 Hz |
| Text | `draw.c` | The title, the menu entries, the hints and the status row |

[CULVERT.md](CULVERT.md) explains the game.

The QEMU tests use two small test tapes in `tests/tapes/`, `linetest` and `fault`. The line test tape has its own `icon.pbm`, a cassette. They never go on the default card.

## The manual's pictures

[USER_MANUAL.md](USER_MANUAL.md) shows the test goldens in `tests/golden/` where they are. They are not copies. They are the screens of one emulated board with the sample card. The tests in `tests/` compare each screen with its golden pixel for pixel, on every QEMU board they run on, so one file serves all four boards. A change that alters a screen updates its golden, so the manual's pictures follow. They show the emulator, not a real panel.

## Adding an asset

1. Make it yourself, or use one whose licence allows it in an MIT project.
2. For an icon, draw it as a PBM file and save it in `assets/` with a lower-case name. `make` turns it into C.
3. For content, put the file under `content/PIPBOY/DATA/` at its card path, with an upper-case 8.3 name. Run `make check-content`.
4. For a generated asset, write the tool in `tools/` and make the file a build target, as `tools/mkpcm.py` does. Keep the output the same on every machine.
5. For an asset from elsewhere, keep its licence file next to it and add it to this page: where it came from, the version, the licence.
6. Add a line to [CHANGELOG.md](../CHANGELOG.md).

## Open questions

- The tunes and the PCM chord progression: the README says all music is original. No other record says who wrote the three melodies or checks them against existing songs.
- The sample text and the icons: the README and the PBM comments say they are original. Git history adds them in one commit each and records nothing more.
