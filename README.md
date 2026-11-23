# Deitrix

Deitrix is the user environment of a wrist computer, the ROM that [Pip-OS](https://github.com/RobCo-Industries/Pip-OS) loads from `/PIPBOY/ROM1.ROM` on the SD card. It runs in user mode and reaches the screen, buttons, dial and clock only through Pip-OS system calls. It is built with the Pip-OS SDK alone, as any third-party ROM would be.

## Status

Deitrix has a UI framework, a data model and five tabs. It boots on Pip-OS in QEMU. Every frame shows:

- a tab bar with STAT, INV, DATA, MAP and RADIO, the active tab drawn as a filled block;
- the time as `HH:MM` in UTC at the top right, or `--:--` when Pip-OS has no clock;
- the active tab's body;
- a key hint at the bottom.

STAT is built: the owner and gauges, the attributes, and the device data. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#stat).

INV is built: the items of ITEMS.INI by category, a detail per item, and counts and marks the owner changes, kept in INV.SAV. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#inv).

MAP is built: the local and world maps with their places, and a detail per place. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#map).

RADIO is built: the stations of RADIO.INI, what plays now, the volume and the UI sounds, kept in DEITRIX.SAV. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#radio).

DATA is a placeholder. It shows the tab's name, "Not built yet." and the note count, or why the notes did not load.

At start the ROM loads the content under `/PIPBOY/DATA/` on the card and its two saves. The formats are in [docs/CONTENT.md](docs/CONTENT.md). The dial, LEFT and RIGHT move between tabs and wrap at both ends. BACK and LEFT go back one level in a nested view. SELECT on RADIO cycles the stations: tunes through `play_seq`, PCM tracks streamed in chunks. The radio keeps playing on other tabs. The last tab and station are saved a moment after they change. A test-only gallery ROM shows every widget of the framework. Nothing has been tried on real hardware, and nobody has listened to the radio yet: QEMU only prints an audio trace.

## Planned, not built

- The five tabs with real content, on top of the data model.
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
| Right arrow | Next tab, or stays inside a nested view |
| Left arrow | Previous tab, or back one level in a nested view |
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
