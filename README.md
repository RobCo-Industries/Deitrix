# Deitrix

Deitrix is the user environment of a wrist computer, the ROM that [Pip-OS](https://github.com/RobCo-Industries/Pip-OS) loads from `/PIPBOY/ROM1.ROM` on the SD card. It runs in user mode and reaches the screen, buttons, dial and clock only through Pip-OS system calls. It is built with the Pip-OS SDK alone, as any third-party ROM would be.

## Status

Deitrix has a UI framework and five placeholder tabs. It boots on Pip-OS in QEMU and shows one frame:

- a tab bar with STAT, INV, DATA, MAP and RADIO, the active tab drawn as a filled block;
- the time as `HH:MM` in UTC at the top right, or `--:--` when Pip-OS has no clock;
- the active tab's name and "Not built yet." in the body;
- a key hint at the bottom.

The dial, LEFT and RIGHT move between tabs and wrap at both ends. Nothing else reacts yet. The framework's widgets run in a test-only gallery ROM until the tabs use them. Nothing has been tried on real hardware.

## Planned, not built

- The five tabs with real content.
- Content files read from `/PIPBOY/DATA/` on the card.
- Saves.
- A holotape game.

## Build

```sh
git clone --recursive https://github.com/RobCo-Industries/Deitrix.git
cd Deitrix
make
```

`make` writes `build/ROM1.ROM`. The tools are the same as for Pip-OS: see [docs/BUILD.md](docs/BUILD.md).

## Run in QEMU

```sh
make run
```

This builds the Pip-OS kernel and a card with Deitrix, then boots it in QEMU on `raspi2b`. `BOARD=raspi0` picks another board. `WINDOW=1` opens the display window. Type the keys in the terminal that runs `make run`:

| Key | Does |
|---|---|
| Right arrow | Next tab |
| Left arrow | Previous tab |
| Ctrl-A, then x | Quits QEMU |

The terminal has no dial key. The script channel's `dial <n>` command turns the dial in the tests.

## Test

```sh
make test-host     # host unit tests, pin checks, card check
make test          # the above, then the QEMU tests on four boards
make test BOARD=raspi0
```

See [docs/TESTING.md](docs/TESTING.md).

## Docs

- [docs/BUILD.md](docs/BUILD.md): tools, make targets, card layout.
- [docs/TESTING.md](docs/TESTING.md): test layers, goldens.
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): source layout and main loop.
- [CONTRIBUTING.md](CONTRIBUTING.md) and [CHANGELOG.md](CHANGELOG.md).

## Assets

All artwork, fonts, sounds and text in this repo are original. Nothing is copied from the Fallout games.

## Licence

MIT. See [LICENSE](LICENSE).
