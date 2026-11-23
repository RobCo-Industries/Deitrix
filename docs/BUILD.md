# Building Deitrix

## Tools

Deitrix needs the same tools as Pip-OS: the `arm-none-eabi` GCC toolchain, GNU Make, Python 3, QEMU with the Raspberry Pi boards, mtools and a host C compiler. RobCo-Core's [getting started guide](https://github.com/RobCo-Industries/RobCo-Core/blob/222f4e44f67cdca2a65da12c7b0a90ea0d9aa087/docs/guide/GETTING_STARTED.md) lists them per system. CI uses RobCo-Core's `robco-ci` container, which has all of them.

## Sources

Pip-OS is the only submodule, in `external/pip-os`. RobCo-Core comes with it, in `external/pip-os/external/robco-core`. One command fetches both:

```sh
git submodule update --init --recursive
```

The ROM is built with the Pip-OS SDK, `external/pip-os/sdk/pipos-app.mk`. Its guide is Pip-OS's [ROM_DEVELOPMENT.md](https://github.com/RobCo-Industries/Pip-OS/blob/4178b7f96f8c7051c8d75d5bc18f611dce30a999/docs/ROM_DEVELOPMENT.md).

## Make targets

| Target | What it does |
|---|---|
| `make`, `make all` | Builds `build/ROM1.ROM` with the SDK |
| `make test-host` | Host unit tests, `check-pins`, the pin script test and the card check |
| `make test` | `test-host`, then `test-target` |
| `make test-target` | The QEMU tests, see [TESTING.md](TESTING.md) |
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
| `CI` | unset | `CI=1` turns compiler warnings into errors |
| `PYTHON` | `python3` | Python for the SDK tools |

## How the targets reuse Pip-OS

- `all` includes the SDK's `pipos-app.mk` with `KIND=rom`, `NAME=DEITRIX` and `TITLE=Deitrix`.
- `test-host` includes RobCo-Core's `mk/host.mk` for the C tests.
- `kernel`, `sdimage`, `run` and `bootdir` call the same targets in Pip-OS's Makefile, with `BUILD=build/pipos`. `sdimage` and `run` pass `ROM=build/ROM1.ROM`, `TAPE=none` and `SDIMAGE_DIR=content`.
- `bootdir` builds all of Pip-OS first, because Pip-OS's `bootdir` needs its `all`. Then it copies `content/` and the ROM into the folder.
- `check-pins` runs RobCo-Core's `tools/check_pins.sh`, Pip-OS's `tools/check_doc_links.sh` and Deitrix's `tools/check_pipos_links.sh`.

## The card

`make sdimage` writes a 64 MiB FAT32 card:

| Path | From |
|---|---|
| `PIPOS.IMG`, `config.txt` | The Pip-OS kernel and its boot config |
| `PIPBOY/PIPOS.CFG` | Pip-OS's sample settings, green scheme |
| `PIPBOY/ROM1.ROM` | `build/ROM1.ROM` |
| `PIPBOY/HOLO/` | Empty |
| everything else | The `content/` tree, copied to the card root |

To put a file on the card, add it under `content/` at the path it should have. Today `content/` holds `PIPBOY/DATA/README.TXT`.
