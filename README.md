# Deitrix

Deitrix is the Pip-Boy user environment. [Pip-OS](https://github.com/RobCo-Industries/Pip-OS) loads it from ROM bank 1 at boot, which is the `LOAD ROM(1): DEITRIX 303` line on the boot screen.

## Status

Nothing is implemented yet. This repo holds a README and a licence.

Deitrix depends on two things that do not exist yet. Pip-OS has to load a ROM from the SD card and answer system calls, and today it does neither. The ROM format and the system call interface also have to be written down, in [RobCo-Core](https://github.com/RobCo-Industries/RobCo-Core). Work here starts once both are done.

## What it will be

A ROM image that runs in user mode on top of Pip-OS and draws the Pip-Boy interface.

- Five tabs: STAT, INV, DATA, MAP and RADIO.
- Navigation with the dial and buttons.
- Vector graphics on a 320x240 monochrome surface.
- Sound through the Pip-OS audio calls.
- Save data on the SD card.
- A holotape launcher, with one small game to prove it works.

Deitrix never touches hardware. Everything goes through Pip-OS system calls.

## Assets

All artwork, fonts, sounds and text in this repo are original. Nothing is copied from the Fallout games.

## Licence

MIT. See [LICENSE](LICENSE).
