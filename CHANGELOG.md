# Changelog

All notable changes to Deitrix are listed here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Deitrix follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Pip-OS moves to `2d10c2c`, with RobCo-Core `f5bba36` inside it. The kernel mounts the card with the shared boot volume rule.

### Added

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
- A UI framework: a drawing table in the platform layer, a theme with dim and faint shades, UI events, a screen interface and an app shell that routes keys to the active screen. Each tab is a screen in `src/screens/`.
- Widgets: a list, a text block, a gauge, key and value rows, sub-tabs, 1-bit icons and a stroke font for large text.
- `tools/pbm2c.py`, which turns the icons in `assets/` into C at build time.
- Short UI tones for moves, selections and refused keys.
- `docs/CONTENT.md`: the formats of PROFILE.INI, ITEMS.INI, the notes, MAP.INI, RADIO.INI, `.TUN` tunes, `.PCM` tracks and the two saves, with every limit.
- Sample content in `content/PIPBOY/DATA/`: a profile, 14 items, 4 notes, two maps, three tunes and five stations. `content/PIPBOY/DATA/README.TXT` is gone.
- `tools/check_content.py`, run by `make check-content`; `tools/tun2notes.py`; `tools/mkpcm.py`, which generates the two PCM tracks at build time. `make card-tree` puts them with `content/` in `build/card/`, the tree that goes to the card.
- Tests for the content tools in `tests/tools/`, run by `make tools-test`.
- `make gallery`: a test-only ROM that shows every widget, with QEMU goldens and scripted runs.
