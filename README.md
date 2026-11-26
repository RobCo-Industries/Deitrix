# Deitrix

Deitrix is the user environment of a wrist computer, the ROM that [Pip-OS](https://github.com/RobCo-Industries/Pip-OS) loads from `/PIPBOY/ROM1.ROM` on the SD card. It runs in user mode and reaches the screen, buttons, dial and clock only through Pip-OS system calls. It is built with the Pip-OS SDK alone, as any third-party ROM would be.

## Status

Deitrix has a UI framework, a data model, five tabs and one holotape game. It boots on Pip-OS in QEMU. On 2026-11-24 it also booted on a Pi Zero W 1.1, with keys sent over serial and no screen, speaker, buttons or dial wired. A second power-on loaded the saved settings and ran Culvert until Quit. The picture, the sound, the buttons, the dial and every other board are still not tried, and nobody has listened to the radio yet: QEMU only prints an audio trace. See [TESTING.md](docs/TESTING.md#real-hardware).

Every frame shows a tab bar with STAT, INV, DATA, MAP and RADIO, the time as `HH:MM` in UTC or `--:--`, the active tab's body, and a key hint at the bottom. A `!` left of the clock says changes have not reached the card for 12 s.

- STAT: the owner and gauges, the attributes, and the device data with the uptime.
- INV: the items of ITEMS.INI by category, a detail per item, and counts and marks kept in INV.SAV.
- DATA: the notes, the TAPES list that runs holotapes, and the clock with a time setter. With Pip-OS ABI 1.4 and a holotape slot, TAPES follows the slot card as it goes in and out, and the footer says what changed. The slot is tested in QEMU with Pip-OS's RAM card backend only: no board has a slot yet.
- MAP: the local and world maps with their places, and a detail per place.
- RADIO: the stations of RADIO.INI, what plays now, the volume and the UI sounds, kept in DEITRIX.SAV.
- Culvert: a pipe-turning puzzle holotape with generated levels, on the card as `PIPBOY/HOLO/CULVERT.HTP`.

Every tab follows one held-key rule, one footer hint style and one layout grid. [docs/USER_MANUAL.md](docs/USER_MANUAL.md) shows each screen. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) explains the code.

## Build

```sh
git clone --recursive https://github.com/RobCo-Industries/Deitrix.git
cd Deitrix
make
```

`make` writes `build/ROM1.ROM` and the Culvert holotape, `build/holo/CULVERT.HTP`. The tools are the same as for Pip-OS: see [docs/BUILD.md](docs/BUILD.md).

## Run in QEMU

```sh
make run
```

This builds the Pip-OS kernel and a card with Deitrix, then boots it in QEMU on `raspi2b`. `BOARD=raspi0` picks another board. `WINDOW=1` opens the display window. Type the keys in the terminal that runs `make run`:

| Key | Does |
|---|---|
| Up and down arrows | UP and DOWN |
| Right arrow | Next sub-tab or tab, or nothing inside a nested view |
| Left arrow | Previous sub-tab or tab, or back one level in a nested view |
| Enter | SELECT |
| Backspace, or Escape alone | Back: one level up in a nested view. It never changes the tab. |
| Ctrl-A, then x | Quits QEMU |

The terminal has no dial key. The script channel's `dial <n>` command turns the dial in the tests.

## Test

```sh
make test-host     # host unit tests, tool tests, pin, content and card checks
make test          # the above, then the QEMU tests on four boards
make test BOARD=raspi0
```

See [docs/TESTING.md](docs/TESTING.md).

## Documentation

- [docs/USER_MANUAL.md](docs/USER_MANUAL.md): how to use the device, tab by tab, with troubleshooting.
- [docs/BUILD.md](docs/BUILD.md): tools, make targets and variables, building and writing a card.
- [docs/CONTENT.md](docs/CONTENT.md): content files, tunes, PCM tracks and saves.
- [docs/ASSETS.md](docs/ASSETS.md): every asset, where it comes from and its licence.
- [docs/CULVERT.md](docs/CULVERT.md): the Culvert holotape, how to play it.
- [docs/TESTING.md](docs/TESTING.md): test layers, goldens.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): source layout and main loop.
- [CONTRIBUTING.md](CONTRIBUTING.md): how to contribute.
- [CHANGELOG.md](CHANGELOG.md): what changed.

## Assets

All artwork, sounds, music, maps and text in this repo are original. Nothing is copied from the Fallout games. The text font comes from Pip-OS: see [docs/ASSETS.md](docs/ASSETS.md).

## Licence

MIT. See [LICENSE](LICENSE).
