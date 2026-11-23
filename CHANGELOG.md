# Changelog

All notable changes to Deitrix are listed here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Deitrix follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Pip-OS as the only submodule, at `4178b7f`, with RobCo-Core `222f4e4` inside it.
- `build/ROM1.ROM` built with the Pip-OS SDK: a frame with five placeholder tabs, a UTC clock and a key hint. The dial, LEFT and RIGHT change tabs.
- `make sdimage`, `run` and `bootdir` through Pip-OS's Makefile, with the `content/` tree on the card.
- Host unit tests, QEMU tests on four boards with screen goldens, and a card check.
- `make check-pins` and `tools/check_pipos_links.sh`.
- CI through RobCo-Core's reusable workflow.
- README, build, testing, architecture and contributing docs.
