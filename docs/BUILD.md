# Building Deitrix

## Tools

Deitrix needs the same tools as Pip-OS: the `arm-none-eabi` GCC toolchain, GNU Make, Python 3, QEMU with the Raspberry Pi boards, mtools and a host C compiler. RobCo-Core's [getting started guide](https://github.com/RobCo-Industries/RobCo-Core/blob/00d9a2d1efa10b08ea1c14f087f7e3316647d6a1/docs/guide/GETTING_STARTED.md) lists them per system. CI uses RobCo-Core's `robco-ci` container, which has all of them.

## Sources

Pip-OS is the only submodule, in `external/pip-os`. RobCo-Core comes with it, in `external/pip-os/external/robco-core`. One command fetches both:

```sh
git submodule update --init --recursive
```

The ROM is built with the Pip-OS SDK, `external/pip-os/sdk/pipos-app.mk`. Its guide is Pip-OS's [ROM_DEVELOPMENT.md](https://github.com/RobCo-Industries/Pip-OS/blob/bd5fa36c68a46ede244f31a348e2446a8daa9f0d/docs/ROM_DEVELOPMENT.md).

## Make targets

| Target | What it does |
|---|---|
| `make`, `make all` | Builds `build/ROM1.ROM` and the holotapes with the SDK |
| `make holotapes` | Builds the holotapes only: `build/holo/CULVERT.HTP` |
| `make test-host` | Host unit tests, `check-pins`, the `check-pins` wiring test, the `pbm2c.py` test, `check-content`, `tools-test` and the card check |
| `make card-tree` | `build/card/`: a fresh copy of `content/` plus the PCM tracks from `tools/mkpcm.py`, kept in `build/pcm/`. Then `tools/check_content.py` checks it, and any problem stops the build. `sdimage`, `run`, `bootdir` and `check-content` run it first. |
| `make check-content` | `tools/check_content.py` on `content/`, where the generated tracks may be missing, and on `build/card/`, where they must be there. See [CONTENT.md](CONTENT.md) |
| `make tools-test` | The tests in `tests/tools/`, with Python's `unittest` |
| `make test` | `test-host`, then `test-target` |
| `make test-target` | The QEMU tests, see [TESTING.md](TESTING.md) |
| `make gallery` | `build/gallery/`: the widget gallery, a test-only ROM, and its card |
| `make hello` | `build/hello/ROM1.ROM`: the SDK's hello ROM, which the Culvert tests use to start the tape |
| `make check-pins` | The CI pins and the commit links in the docs match the submodules |
| `make kernel` | Builds the Pip-OS kernel into `build/pipos/pipos.img` |
| `make sdimage` | Builds the card, `build/pipos/sd.img` |
| `make run` | Builds the card and boots it in QEMU on this terminal |
| `make bootdir DIR=<path>` | Writes the files of a real card into a folder |
| `make clean` | Removes `build/` |

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

## How the targets reuse Pip-OS

- `all` includes the SDK's `pipos-app.mk` with `KIND=rom`, `NAME=DEITRIX` and `TITLE=Deitrix`.
- `test-host` includes RobCo-Core's `mk/host.mk` for the C tests.
- `kernel`, `sdimage`, `run` and `bootdir` call the same targets in Pip-OS's Makefile, with `BUILD=build/pipos`. This is the interface of Pip-OS's [Using Pip-OS from another repository](https://github.com/RobCo-Industries/Pip-OS/blob/bd5fa36c68a46ede244f31a348e2446a8daa9f0d/docs/ROM_DEVELOPMENT.md#using-pip-os-from-another-repository).
- `sdimage`, `run` and `bootdir` pass `ROM=build/ROM1.ROM`, `TAPE=none` and `CARD_DIR=build/card`. `bootdir` also passes `DIR`, `FIRMWARE` and `CLEAN`. Pip-OS writes the same files to the card and to the folder.
- Pip-OS checks the staged card. Two paths that differ only in case stop the build, and a name longer than 8.3 gets a warning. `tools/check_content.py` already refuses any name under `PIPBOY/DATA/` that is not upper-case 8.3, so the Deitrix content passes with no warning.
- `bootdir` keeps a `PIPBOY/PIPOS.CFG` that is already in the folder. Its other rules, `BOOTDIR.LST` and `CLEAN=1` are in Pip-OS's [A real card](https://github.com/RobCo-Industries/Pip-OS/blob/bd5fa36c68a46ede244f31a348e2446a8daa9f0d/docs/BUILD.md#a-real-card).
- `check-pins` runs RobCo-Core's `tools/check_pins.sh` on the CI workflow, then its `tools/check_doc_links.sh` twice: once with `--repo RobCo-Industries/Pip-OS` for `external/pip-os`, once for the RobCo-Core inside it.

## The card

`make sdimage` writes a 64 MiB FAT32 card:

| Path | From |
|---|---|
| `PIPOS.IMG`, `config.txt` | The Pip-OS kernel and its boot config |
| `PIPBOY/PIPOS.CFG` | Pip-OS's sample settings, green scheme |
| `PIPBOY/ROM1.ROM` | `build/ROM1.ROM` |
| `PIPBOY/HOLO/CULVERT.HTP` | `build/holo/CULVERT.HTP`, through Pip-OS's `TAPE=` list |
| everything else | `build/card/`, copied to the card root |

`build/card/` is `content/` plus `PIPBOY/DATA/RADIO/HEARTH.PCM` and `DRIFT.PCM`. `tools/mkpcm.py` generates the two tracks into `build/pcm/`, 250 KiB each, so git does not hold them. `tools/mkpcm.py --list` names the tracks, and each one is its own make target. A track runs again when it is missing or when `tools/mkpcm.py` changes. The copy of `content/` runs every time. The checker then runs on `build/card/`, so a station whose file is missing stops `sdimage`, `run` and `bootdir`. To put a file on the card, add it under `content/` at the path it should have. [CONTENT.md](CONTENT.md) lists the files Deitrix reads and their formats.
