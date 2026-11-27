# Building Deitrix

This page is for a person with a Mac or a Linux machine who wants to build a Deitrix card. It needs no knowledge of the code.

Deitrix has run on one real board so far: a Pi Zero W 1.1, on 2026-11-24, at commit `ba21d5a`, with serial only. The steps below for a real card produced a card that booted there. [TESTING.md](TESTING.md#real-hardware) records what that run shows and what it does not.

## Tools

Deitrix needs the same tools as Pip-OS. Pip-OS's [BUILD.md](https://github.com/RobCo-Industries/Pip-OS/blob/5175bbd0d2bf5ed04230f905707298fc53b30740/docs/BUILD.md#tools) and RobCo-Core's [getting started guide](https://github.com/RobCo-Industries/RobCo-Core/blob/9a11561b63068156cce77ff14af45e34e11da37f/docs/guide/GETTING_STARTED.md#prerequisites) list them. The versions below come from those guides.

| Tool | Version | Used for |
|---|---|---|
| `arm-none-eabi-gcc` and binutils | Any recent release. CI uses Ubuntu 24.04's `gcc-arm-none-eabi`. | The ROM and the kernel |
| GNU make | 3.81 or later | The build |
| git | Any | The sources and the submodules |
| Python 3 with `venv` | 3.9 or later for the build tools. CI uses 3.12. | Sealing the ROM, the content tools, the tests |
| A host C compiler | Any | The host tests |
| `qemu-system-arm` and `qemu-system-aarch64` | 10.1.x. Ubuntu's packaged QEMU 8.x is too old. | `make run` and the QEMU tests |
| mtools | Any | The card image |

The ROM build itself needs only the ARM compiler, make and Python 3. QEMU and mtools are for the card image, `make run` and the tests.

On macOS with Homebrew:

```sh
brew install arm-none-eabi-gcc qemu mtools python
```

Check what you have:

```sh
arm-none-eabi-gcc --version
qemu-system-arm --version
python3 --version
```

CI runs in RobCo-Core's `robco-ci` container, which holds every tool at the version CI uses. `.github/workflows/ci.yml` pins its tag.

## Get the source

Pip-OS is the only submodule, in `external/pip-os`. RobCo-Core comes inside it, in `external/pip-os/external/robco-core`. Clone with both:

```sh
git clone --recursive https://github.com/RobCo-Industries/Deitrix.git
cd Deitrix
```

In a clone made without `--recursive`, fetch them:

```sh
git submodule update --init --recursive
```

## Build the ROM

```sh
make
```

This writes `build/ROM1.ROM`, the Deitrix ROM sealed by the Pip-OS SDK, and `build/holo/CULVERT.HTP`, the Culvert holotape. It takes a few seconds. Other files in `build/`:

| Path | What it is |
|---|---|
| `build/ROM1.ROM` | The ROM. It goes on the card as `PIPBOY/ROM1.ROM`. |
| `build/ROM1.elf`, `build/ROM1.map` | The linked program and its map, for debugging |
| `build/gen/icons.c` | The icons, made from `assets/*.pbm` |
| `build/card/` | The card tree, from `make card-tree` |
| `build/pcm/` | The two generated radio tracks |
| `build/holo/CULVERT.HTP` | The Culvert holotape. It goes on the card as `PIPBOY/HOLO/CULVERT.HTP`. |
| `build/pipos/` | The Pip-OS kernel and the card image |

The SDK's guide is Pip-OS's [ROM_DEVELOPMENT.md](https://github.com/RobCo-Industries/Pip-OS/blob/5175bbd0d2bf5ed04230f905707298fc53b30740/docs/ROM_DEVELOPMENT.md).

## Run it in QEMU

```sh
make run
```

This builds the ROM, the Pip-OS kernel and a card, then boots the card in QEMU on `raspi2b`. The screen goes to a window only with `WINDOW=1`. The serial log goes to the terminal. Type the keys in that terminal:

| Key | Does |
|---|---|
| Arrow keys | UP, DOWN, LEFT and RIGHT |
| Enter | SELECT |
| Backspace, or Escape alone | BACK |
| Ctrl-A, then x | Quits QEMU |

The terminal has no dial key. LEFT and RIGHT change the tab where no sub-tab takes them.

```sh
make run BOARD=raspi0 WINDOW=1
```

`BOARD` picks the board: `raspi0`, `raspi1ap`, `raspi2b` or `raspi3b`. [USER_MANUAL.md](USER_MANUAL.md) explains what each tab does.

## Build a card image

```sh
make sdimage
```

This writes `build/pipos/sd.img`, a 64 MiB FAT32 image for QEMU:

| Path on the card | From |
|---|---|
| `PIPOS.IMG`, `config.txt` | The Pip-OS kernel and its boot config |
| `PIPBOY/PIPOS.CFG` | Pip-OS's sample settings, green scheme |
| `PIPBOY/ROM1.ROM` | `build/ROM1.ROM` |
| `PIPBOY/HOLO/CULVERT.HTP` | `build/holo/CULVERT.HTP`, through Pip-OS's `TAPE=` list |
| everything else | `build/card/`, copied to the card root |

`build/card/` is `content/` plus `PIPBOY/DATA/RADIO/HEARTH.PCM` and `DRIFT.PCM`. `tools/mkpcm.py` generates the two tracks into `build/pcm/`, 250 KiB each, so git does not hold them. `tools/mkpcm.py --list` names them, and each one is its own make target. A track is made again when it is missing or when `tools/mkpcm.py` changes. The copy of `content/` runs every time. `tools/check_content.py` then checks `build/card/`, so a station whose file is missing stops `sdimage`, `run` and `bootdir`.

To put a file on the card, add it under `content/` at the path it should have. [CONTENT.md](CONTENT.md) lists the files Deitrix reads and their formats.

## Holotapes

A holotape is a program Deitrix runs from DATA, TAPES. Each one lives in its own folder under `holotapes/`, with a Makefile that includes the SDK's `pipos-app.mk` with `KIND := holotape`. The only one today is Culvert, in `holotapes/culvert/`.

```sh
make holotapes
```

This builds `build/holo/CULVERT.HTP`. `make` builds it too, with the ROM.

Culvert's folder also holds `robco.toml`, the same project for RobCo Studio, as RobCo-Core's [STUDIO_PROJECT.md](https://github.com/RobCo-Industries/RobCo-Core/blob/9a11561b63068156cce77ff14af45e34e11da37f/docs/spec/STUDIO_PROJECT.md#a-project-that-also-builds-with-a-makefile) asks. Its keys equal the Makefile's `NAME`, `TITLE`, `VERSION`, `ICON`, `SUBTYPE`, `ABI` and `SRCS`, with the sources in the Makefile's order. Change both files in one commit. Studio's manifest reader accepts the file. No Studio build of Culvert has run yet, so nobody has compared its tape with the Makefile's.

Deitrix's Makefile lists the tapes in `TAPES` and passes them to Pip-OS as `TAPE=build/holo/CULVERT.HTP`. Pip-OS copies every file of that list to `PIPBOY/HOLO/` under its own name. So `make sdimage`, `make run` and `make bootdir` all put Culvert on the card, and Pip-OS's example tape is not there. To add a tape, give it a folder under `holotapes/`, then add its build and its `.HTP` file to the `holotapes` goal and to `TAPES`. Pip-OS's [BUILD.md](https://github.com/RobCo-Industries/Pip-OS/blob/5175bbd0d2bf5ed04230f905707298fc53b30740/docs/BUILD.md) gives the rules for `TAPE`: names end in `.HTP`, and two names may not differ only in case.

To put a tape on a card by hand, copy its `.HTP` file to the card's `/PIPBOY/HOLO/` folder.

## Put it on a real card

This follows Pip-OS's [A real card](https://github.com/RobCo-Industries/Pip-OS/blob/5175bbd0d2bf5ed04230f905707298fc53b30740/docs/BUILD.md#a-real-card). These steps, with the Mac commands below, produced a card that booted Deitrix on a Pi Zero W 1.1 on 2026-11-24. No other board and no other host has been tried. See [TESTING.md](TESTING.md#real-hardware).

```sh
make bootdir DIR=card
```

This writes the files of the card into the folder `card/`: the Raspberry Pi firmware, `PIPOS.IMG`, `config.txt`, `PIPBOY/PIPOS.CFG`, `PIPBOY/ROM1.ROM`, the holotapes in `PIPBOY/HOLO/` and the card tree. It never writes a device.

1. Format the SD card as FAT32.
2. Copy everything in `card/` to the card.
3. Put the card in the board and power it on.

On a Mac, these commands made the card that booted. Replace `diskN` with the card's disk and `card` with your folder.

```sh
diskutil list
# ERASES THE WHOLE DISK diskN. Check the disk number above first.
diskutil eraseDisk FAT32 ROBCO MBRFormat /dev/diskN
cp -RX card/. /Volumes/ROBCO/
diskutil eject /dev/diskN
```

`cp -RX` copies without the macOS extended attributes. macOS can still write side files named `._*` on a FAT32 card. Pip-OS skips them, and hidden and system files, when it counts and lists the holotapes and the notes. The card that booted was also cleaned with `dot_clean -m`, since the Pip-OS of that run did not skip them yet.

The firmware comes from the network. `FIRMWARE=0` leaves it out, so the command needs no network. You then copy the firmware yourself.

`bootdir` keeps a `PIPBOY/PIPOS.CFG` that is already in the folder. It writes `BOOTDIR.LST`, the list of files it wrote. Running it again into the same folder can stop and list old files. Use a new folder, or `CLEAN=1` to remove the files an earlier run wrote. Pip-OS's [Running bootdir again](https://github.com/RobCo-Industries/Pip-OS/blob/5175bbd0d2bf5ed04230f905707298fc53b30740/docs/BUILD.md#running-bootdir-again) gives every rule.

## Make targets

| Target | What it does |
|---|---|
| `make`, `make all` | Builds `build/ROM1.ROM` and the holotapes with the SDK |
| `make holotapes` | Builds the holotapes only: `build/holo/CULVERT.HTP` |
| `make run` | Builds the card and boots it in QEMU on this terminal |
| `make sdimage` | Builds the card image, `build/pipos/sd.img` |
| `make bootdir DIR=<path>` | Writes the files of a real card into a folder |
| `make kernel` | Builds the Pip-OS kernel into `build/pipos/pipos.img` |
| `make card-tree` | `build/card/`: a fresh copy of `content/` plus the PCM tracks from `tools/mkpcm.py`, kept in `build/pcm/`. Then `tools/check_content.py` checks it, and any problem stops the build. `sdimage`, `run`, `bootdir` and `check-content` run it first. |
| `make check-content` | `tools/check_content.py` on `content/`, where the generated tracks may be missing, and on `build/card/`, where they must be there |
| `make check-pins` | The CI pins and the commit links in the docs match the submodules |
| `make weep-table` | Writes `src/services/weep_table.c` and `.h` again from `tools/weep_params.json`. Commit them |
| `make weep-table-check` | Fails when the committed weep table is stale. `make` and `make test-host` run it first |
| `make boot-sound` | Boots the card in QEMU with the system sound on and writes `build/boot_sound.wav`, the whole boot and the weep. See [TESTING.md](TESTING.md#the-boot-sound-preview) |
| `make check-format` | No unchecked C library string writer in `src/` or `tests/host/` |
| `make tools-test` | The tests in `tests/tools/`, with Python's `unittest` |
| `make test-host` | The C host unit tests, `weep-table-check`, `check-format`, `check-pins`, the `check-pins` wiring test, the `pbm2c.py` test, `check-content`, `tools-test` and the card check |
| `make test-target` | The QEMU tests |
| `make test` | `test-host`, then `test-target` |
| `make gallery` | `build/gallery/`: the widget gallery, a test-only ROM, and its card |
| `make venv` | `.venv/` with the Python packages the QEMU tests need |
| `make clean` | Removes `build/` |

[TESTING.md](TESTING.md) describes every test target and what each test checks.

## Variables

| Variable | Default | Meaning |
|---|---|---|
| `BUILD` | `build` | Output folder |
| `BOARD` | `all` | QEMU board for `test` and `run`: `raspi0`, `raspi1ap`, `raspi2b`, `raspi3b` or `all`. `run` takes `raspi2b` for `all`. |
| `WINDOW` | unset | `WINDOW=1` makes `run` open the display window |
| `DIR` | unset | The folder for `bootdir` |
| `FIRMWARE` | unset | `FIRMWARE=0` makes `bootdir` leave out the Raspberry Pi firmware |
| `CLEAN` | unset | `CLEAN=1` makes `bootdir` remove the files an earlier `bootdir` run wrote |
| `CI` | unset | `CI=1` turns compiler warnings into errors |
| `PYTHON` | `python3` | Python 3.9 or later, for the SDK and the content tools. They use the standard library only. |
| `CONTENT` | `content` | The content tree `card-tree` copies and `check-content` checks |

## How the targets reuse Pip-OS

- `all` includes the SDK's `pipos-app.mk` with `KIND=rom`, `NAME=DEITRIX` and `TITLE=Deitrix`.
- `test-host` includes RobCo-Core's `mk/host.mk` for the C tests.
- `kernel`, `sdimage`, `run` and `bootdir` call the same targets in Pip-OS's Makefile, with `BUILD=build/pipos`. This is the interface of Pip-OS's [Using Pip-OS from another repository](https://github.com/RobCo-Industries/Pip-OS/blob/5175bbd0d2bf5ed04230f905707298fc53b30740/docs/ROM_DEVELOPMENT.md#using-pip-os-from-another-repository).
- `sdimage`, `run` and `bootdir` pass `ROM=build/ROM1.ROM`, `TAPE=build/holo/CULVERT.HTP` and `CARD_DIR=build/card`. `bootdir` also passes `DIR`, `FIRMWARE` and `CLEAN`. Pip-OS writes the same files to the card and to the folder.
- Pip-OS checks the staged card. Two paths that differ only in case stop the build, and a name longer than 8.3 gets a warning. `tools/check_content.py` already refuses any name under `PIPBOY/DATA/` that is not upper-case 8.3, so the Deitrix content passes with no warning.
- `check-pins` runs RobCo-Core's `tools/check_pins.sh` on the CI workflow, then its `tools/check_doc_links.sh` twice: once with `--repo RobCo-Industries/Pip-OS` for `external/pip-os`, once for the RobCo-Core inside it.

## Common build errors

| Message | Cause | What to do |
|---|---|---|
| `external/pip-os is empty: run git submodule update --init --recursive.  Stop.` | The clone has no submodules | Run `git submodule update --init --recursive` |
| `arm-none-eabi-gcc: No such file or directory` | The ARM compiler is not installed or not on `PATH` | Install it, see [Tools](#tools) |
| `nopython: Command not found`, then `tools/mkpcm.py --list printed no tracks.  Stop.` | `PYTHON` names a program that does not exist. `nopython` stands for the name you gave. | Install Python 3, or pass `PYTHON=` with a working one |
| `DIR is not set: make bootdir DIR=<path>.  Stop.` | `make bootdir` without a folder | Pass `DIR=<path>` |
| `run make gallery.  Stop.` | `make gallery-card` called by hand | Run `make gallery` |
| A `check_content.py` line such as `build/card/PIPBOY/DATA/ITEMS.INI:12: ...`, then a make error | A content file breaks a rule | Fix the line it names. [CONTENT.md](CONTENT.md) gives the rules. |
| `bootdir` stops and lists files | The folder holds files from an earlier run | Use a new folder, or add `CLEAN=1` |
