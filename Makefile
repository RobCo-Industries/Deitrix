# Deitrix, ROM 1 for Pip-OS. Built with the Pip-OS SDK in external/pip-os.
#
#   make                      build/ROM1.ROM
#   make test-host            host unit tests, the pin checks and the card check
#   make test                 test-host, then the QEMU tests on every board
#   make test BOARD=raspi0    the QEMU tests on one board
#   make test-target          the QEMU tests only
#   make gallery              build/gallery/: the widget gallery ROM and its
#                             card, for the QEMU tests only
#   make check-pins           CI pins and doc links match the submodules
#   make sdimage              build/pipos/sd.img: the Pip-OS kernel, its sample
#                             PIPOS.CFG, ROM1.ROM and the card tree
#   make card-tree            build/card/: content/ plus the generated radio
#                             tracks, the tree that goes to the card root
#   make check-content        tools/check_content.py on build/card/
#   make run                  boot the card in QEMU on this terminal
#                             (BOARD=<name>, WINDOW=1 opens the display)
#   make bootdir DIR=<path>   the files of a real card (FIRMWARE=0 leaves
#                             the Raspberry Pi firmware out)
#   make clean

PIP_OS     := external/pip-os
ROBCO_CORE := $(PIP_OS)/external/robco-core
BUILD      ?= build
BOARD      ?= all
PYTHON     ?= python3

ifeq ($(wildcard $(PIP_OS)/sdk/pipos-app.mk $(ROBCO_CORE)/mk/host.mk),)
$(error $(PIP_OS) is empty: run git submodule update --init --recursive)
endif

.DEFAULT_GOAL := all

# Every .c under src/ builds into the ROM. Only src/platform/ calls Pip-OS,
# so the rest is pure and the host tests link it. GALLERY=1 builds the
# widget gallery instead: the screens of tests/gallery/ in place of
# src/screens/. `make gallery` sets it, with BUILD=build/gallery.
DX_SRCS   := $(sort $(wildcard src/*.c src/*/*.c))
PURE_SRCS := $(filter-out src/platform/%,$(DX_SRCS))
ifeq ($(GALLERY),1)
DX_SRCS   := $(filter-out src/screens/%,$(DX_SRCS)) $(sort $(wildcard tests/gallery/*.c))
endif

# The icons: assets/*.pbm turned into C at build time.
ICONS     := $(sort $(wildcard assets/*.pbm))
ICONS_C   := $(BUILD)/gen/icons.c

# ---- The ROM, through the Pip-OS SDK --------------------------------------

PIPOS_SDK    := $(PIP_OS)/sdk
IMAGE        := ROM1.ROM
KIND         := rom
NAME         := DEITRIX
TITLE        := Deitrix
VERSION      := 0.1.0
SRCS         := $(DX_SRCS) $(ICONS_C)
CFLAGS_EXTRA := -Isrc
include $(PIPOS_SDK)/pipos-app.mk

$(ICONS_C): tools/pbm2c.py $(ICONS)
	$(PYTHON) tools/pbm2c.py $@ $(ICONS)

# ---- Host unit tests, with RobCo-Core's host.mk ---------------------------

HOST_TEST_DIR := tests/host
HOST_SRCS     := $(PURE_SRCS) $(ICONS_C) tests/host/fake_platform.c
HOST_INCLUDES := src
include $(ROBCO_CORE)/mk/host.mk

# host.mk only sees the headers at the top of src/.
$(HOST_TESTS): $(wildcard src/*/*.h) tests/host/fake_platform.h

# QEMU runs and builds below share build/; keep top-level goals serial.
.NOTPARALLEL:

# ---- The card tree: content/ plus the generated radio tracks ------------

# tools/mkpcm.py writes the two .PCM tracks into build/pcm/; they are not
# in git. The card tree is copied again on every run, so an edit in
# content/ always reaches it, even within the same second.
CARD_TREE  := $(BUILD)/card
PCM_DIR    := $(BUILD)/pcm
PCM_STAMP  := $(PCM_DIR)/.stamp

$(PCM_STAMP): tools/mkpcm.py
	$(PYTHON) tools/mkpcm.py $(PCM_DIR)
	touch $@

.PHONY: card-tree
card-tree: $(PCM_STAMP)
	rm -rf $(CARD_TREE)
	mkdir -p $(CARD_TREE)
	cp -R content/. $(CARD_TREE)/
	cp $(PCM_DIR)/*.PCM $(CARD_TREE)/PIPBOY/DATA/RADIO/

# ---- Pip-OS: kernel, card, QEMU, real card --------------------------------

# Pip-OS's own Makefile, building into build/pipos. ROM and TAPE pick the
# card's ROM and holotape. SDIMAGE_DIR is RobCo-Core's sdimage input: a
# tree copied to the card root before the ROM.
PIPOS_BUILD := $(abspath $(BUILD))/pipos
PIPOS_MAKE  := $(MAKE) --no-print-directory -C $(PIP_OS) BUILD=$(PIPOS_BUILD) PYTHON=$(PYTHON)
CARD_VARS   := ROM=$(abspath $(PIP_OUT)) TAPE=none SDIMAGE_DIR=$(abspath $(CARD_TREE))
KERNEL      := $(PIPOS_BUILD)/pipos.img
CARD        := $(PIPOS_BUILD)/sd.img

.PHONY: kernel sdimage run bootdir
kernel:
	$(PIPOS_MAKE) $(KERNEL)

sdimage: all card-tree
	$(PIPOS_MAKE) sdimage $(CARD_VARS)

run: all card-tree
	$(PIPOS_MAKE) run $(CARD_VARS) BOARD=$(BOARD) WINDOW=$(WINDOW)

# Pip-OS's bootdir builds all of Pip-OS first, then writes the kernel,
# config.txt, the firmware and a sample PIPOS.CFG. Deitrix adds its files.
bootdir: all card-tree
	$(if $(strip $(DIR)),,$(error DIR is not set: make bootdir DIR=<path>))
	$(PIPOS_MAKE) bootdir DIR=$(abspath $(DIR)) $(if $(FIRMWARE),FIRMWARE=$(FIRMWARE))
	cp -R $(CARD_TREE)/. $(DIR)/
	cp $(PIP_OUT) $(DIR)/PIPBOY/ROM1.ROM

# ---- The widget gallery: a second ROM and card, for tests only ----------

GALLERY_BUILD := $(BUILD)/gallery

.PHONY: gallery gallery-card
gallery:
	$(MAKE) --no-print-directory GALLERY=1 BUILD=$(GALLERY_BUILD) gallery-card

# Run by `make gallery`, with BUILD=build/gallery: its card is
# build/gallery/pipos/sd.img.
gallery-card: all card-tree
	$(if $(filter 1,$(GALLERY)),,$(error run make gallery))
	$(PIPOS_MAKE) sdimage $(CARD_VARS)

# ---- Checks and tests ------------------------------------------------------

.PHONY: test test-target check-pins check-pins-test pbm2c-test sdimage-test venv
.PHONY: check-content tools-test

check-pins:
	$(ROBCO_CORE)/tools/check_pins.sh .github/workflows/ci.yml
	ROBCO_CORE=$(ROBCO_CORE) $(PIP_OS)/tools/check_doc_links.sh $(CURDIR)
	tools/check_pipos_links.sh

check-pins-test:
	tests/host/check_pipos_links_test.sh

pbm2c-test:
	$(PYTHON) tests/host/test_pbm2c.py

sdimage-test: sdimage
	tests/host/sdimage_test.sh $(CARD) $(PIP_OUT) $(CARD_TREE)

check-content: card-tree
	$(PYTHON) tools/check_content.py $(CARD_TREE)

# The tests of the Python tools: unittest, so no pytest is needed.
tools-test:
	$(PYTHON) -m unittest discover -s tests/tools -p 'test_*.py'

# The C host tests come from host.mk's test-host; these run first.
test-host: check-pins check-pins-test pbm2c-test check-content tools-test sdimage-test

VENV := .venv
venv: $(VENV)/.installed
$(VENV)/.installed: $(ROBCO_CORE)/tests/requirements.txt
	$(PYTHON) -m venv $(VENV)
	$(VENV)/bin/pip install -q -r $<
	touch $@

test: test-host
	$(MAKE) --no-print-directory test-target

test-target: sdimage kernel gallery venv
	$(VENV)/bin/pytest tests -v --board $(BOARD) --build-dir $(BUILD)
