# Contributing

## Build and test

```sh
git submodule update --init --recursive
make CI=1 all test-host
make test
```

[docs/BUILD.md](docs/BUILD.md) lists the tools and targets. [docs/TESTING.md](docs/TESTING.md) explains the tests and the goldens.

## Code rules

- Only `src/platform/` calls Pip-OS. Code anywhere else uses `platform.h`, so the host tests can run it.
- Pure logic goes in `src/model/` with a host test.
- Every change that alters behaviour comes with a test that fails without it.
- The build uses the Pip-OS SDK flags: ARMv6, soft float, no C library beyond `libpip`.
- Do not edit `external/pip-os`. Move the submodule pin in its own commit, then run `make check-pins`.

## Commits

- Small commits that each build and pass `make CI=1 all test-host`.
- A plain subject in the imperative: "Add the INV list", not "Added" or "Adds".
- Stage files by name.

## Prose

Docs, comments and commit messages use short, plain sentences in the active voice. No filler, no hype. Every claim must match the code. Headings use sentence case.

## Content

All artwork, fonts, sounds and text must be original. Do not copy anything from the Fallout games: no quotes, logos, names or art.
