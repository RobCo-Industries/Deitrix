# Changelog

All notable changes to Deitrix are listed here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Deitrix follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Pip-OS moves to `5175bbd`, with RobCo-Core `9a11561` inside it: ABI 1.5, the system sound, disk clicks and centred tones. The SDK seals the ROM for ABI 1.5, and STAT's ABOUT page shows `ABI 1.5`. With the system sound off, the default, Deitrix sounds and looks as before.

- Pip-OS moves to `93a07d2`, the commit that builds the `pipos` 1.4.0 SDK package, with RobCo-Core `712db81` inside it. Its `pipos-app.mk` takes `ABI`, writes `flags.json` with `make flags-json` and maps the build paths with `-ffile-prefix-map`. Culvert's tape keeps the same bytes. The CI workflow pins RobCo-Core `712db81` with the same image tag. The docs link the new commits.
- Pip-OS moves to `ca2e054`, with RobCo-Core `cab5f69` inside it. The SDK seals the ROM for ABI 1.4. STAT's ABOUT page shows `ABI 1.4`. The CI workflow pins RobCo-Core `cab5f69` with the same image tag. The docs link the new commits.
- The docs record the first hardware run: on a Pi Zero W 1.1, Deitrix booted, took serial keys and wrote DEITRIX.SAV. `docs/BUILD.md` gives the Mac commands that made the card.
- The docs record a second hardware run on the same card: DEITRIX.SAV loaded at boot, serial keys reached all five tabs, and Culvert started, took keys and quit with status 0.
- `tools/check_content.py --help` prints the usage instead of checking a folder named `--help`. Exit codes stay the same.
- STAT's ABOUT page shows the true uptime past 24.8 days: a new uptime service counts the wraps of `ticks_ms`.
- The profile's gauge type is `dx_profile_gauge_t`, apart from the gauge widget. The widget has a selected, inverse mode, and RADIO's Volume row uses it.
- STAT's ABOUT page logs its device line when it shows and once per uptime minute, not every second.
- The header shows a `!` left of the clock while changes have not reached the card for 12 s, for example on a card that cannot be written.
- Every tab uses one layout grid for its body. Lists start 6 px under the sub-tab row or the header, and status rows sit on one line at the bottom.
- The footer hints follow one style: the same key names, two spaces between groups, movement first, at most 38 characters. No screen hint names the tab change.
- One held-key rule for every tab, in the shell: after a press opens or closes a view or changes the tab, the repeats of that key do nothing until the next press. A held LEFT in INV's detail no longer changes the category and the tab.
- Pip-OS moves to `2d10c2c`, with RobCo-Core `f5bba36` inside it. The kernel mounts the card with the shared boot volume rule.
- Pip-OS moves to `5f891e7`, with RobCo-Core `79cbd82` inside it. The SDK seals the ROM for ABI 1.3. Pip-OS skips the macOS `._*` side files on the card, so `docs/BUILD.md` no longer asks for `dot_clean`.

### Added

- The weep: a 140 ms rising chirp at 37500 Hz, played once right after the first frame. It needs ABI 1.5, an audio driver and an output that rests at its midpoint, which Pip-OS gives with `[audio] system_sound` on. It stays silent with UI sounds off or while the radio plays. `dx_plat_sample_rate()` wraps `sample_rate`. `tools/mkweep.py` writes the committed table from `tools/weep_params.json`, and `make weep-table-check` stops the build when it is stale. Host tests in `test_sfx.c`, tool tests in `test_mkweep.py`, QEMU tests in `test_weep.py`.
- `make boot-sound` boots the card with the system sound on and renders the whole boot, the weep included, into `build/boot_sound.wav` with Pip-OS's preview tool.

- `holotapes/culvert/robco.toml`, Culvert's manifest for RobCo Studio. Culvert's Makefile states `ABI := 1.4`, the ABI its tape already carried, so both files state the same ABI. The sources keep the Makefile's order, and the tape keeps the same bytes.
- The holotape slot of ABI 1.4. A tape slot service reads `hw_flags` and, with a slot, asks `holo_eject_event` after each `PIP_EV_TAPE` event, each key and each launch. TAPES reads the list again after a change and keeps the selection on its tape, or on its row when the tape left. A slot tape shows `SLOT GAME` or `SLOT UTIL`. The footer shows a notice for 3 s when a card goes in, comes out or cannot be read, on any tab, and the keys keep working. A tape whose card is pulled while it runs comes back with "Tape pulled while running.". With no slot, or on a Pip-OS before 1.4, nothing changes.
- `make holotest-kernel` builds Pip-OS's slot test kernel through its Makefile, and `tests/test_slot.py` drives its RAM card backend in QEMU. Host tests in `test_notice.c` and `test_data.c`. Nothing here ran on a board.
- Culvert, a pipe-turning puzzle holotape with generated levels, built by `make holotapes` and shipped on the card as `CULVERT.HTP`. DATA's TAPES list starts it.
- The STAT tab: the owner and gauges, the attributes, and the device data with the uptime, refreshed each second.
- The INV tab: one sub-tab per category, a list of its items with a summary row, and a detail view where UP and DOWN change the count and SELECT marks the item.
- The DATA tab: the notes, a holotape launcher, and the clock with a time setter.
- The MAP tab: the local and world maps with their places, and a detail per place.
- The RADIO tab: the stations with the one on air marked, what plays now, the volume and the UI sounds. SELECT starts or stops a station.
- A data model in `src/model/` that loads PROFILE.INI, ITEMS.INI, the notes, MAP.INI, RADIO.INI and the tunes as `docs/CONTENT.md` says, with fixed tables and no allocation. One serial line per file loaded.
- DEITRIX.SAV and INV.SAV: the settings and the item overlay, read at start and written by an autosave service.
- Services in the app shell: a table of background jobs with their own wake times. The radio service plays tunes and streams PCM tracks on any tab.
- UI tones stay silent while the radio plays, and when the settings turn them off.
- The platform layer reads files, lists folders, reads and writes saves, plays sequences and samples, and reports system information. It wraps libpip's INI parser and CRC-32.
- Host tests for every loader, the saves, the services and the content loader, with fixtures generated from `tools/tun2notes.py` and the checker's test table. QEMU tests for the load log, a save across a reboot, a broken file, a card without data and the radio.
- Pip-OS as the only submodule, at `4178b7f`, with RobCo-Core `222f4e4` inside it.
- `build/ROM1.ROM` built with the Pip-OS SDK: a frame with five tabs, a UTC clock and a key hint. The dial, LEFT and RIGHT change tabs.
- `make sdimage`, `run` and `bootdir` through Pip-OS's Makefile, with the `content/` tree on the card.
- Host unit tests, QEMU tests on four boards with screen goldens, and a card check.
- `make check-pins` and `tools/check_pipos_links.sh`.
- CI through RobCo-Core's reusable workflow.
- README, build, testing, architecture and contributing docs.
- `docs/USER_MANUAL.md` for the person who wears the device, `docs/ASSETS.md` for every asset and its licence, and a build guide for Mac and Linux in `docs/BUILD.md`.
- A UI framework: a drawing table in the platform layer, a theme with dim and faint shades, UI events, a screen interface and an app shell that routes keys to the active screen. Each tab is a screen in `src/screens/`.
- Widgets: a list, a text block, a gauge, key and value rows, sub-tabs, 1-bit icons and a stroke font for large text.
- `tools/pbm2c.py`, which turns the icons in `assets/` into C at build time.
- Short UI tones for moves, selections and refused keys.
- `docs/CONTENT.md`: the formats of PROFILE.INI, ITEMS.INI, the notes, MAP.INI, RADIO.INI, `.TUN` tunes, `.PCM` tracks and the two saves, with every limit.
- Sample content in `content/PIPBOY/DATA/`: a profile, 14 items, 4 notes, two maps, three tunes and five stations. `content/PIPBOY/DATA/README.TXT` is gone.
- `tools/check_content.py`, run by `make check-content`; `tools/tun2notes.py`; `tools/mkpcm.py`, which generates the two PCM tracks at build time. `make card-tree` puts them with `content/` in `build/card/`, the tree that goes to the card.
- Tests for the content tools in `tests/tools/`, run by `make tools-test`.
- `make gallery`: a test-only ROM that shows every widget, with QEMU goldens and scripted runs.
