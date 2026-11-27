# Deitrix, ROM 1 for Pip-OS. Built with the Pip-OS SDK in external/pip-os.
#
#   make                      build/ROM1.ROM
#   make test-host            host unit tests, the pin checks and the card check
#   make test                 test-host, then the QEMU tests on every board
#   make test BOARD=raspi0    the QEMU tests on one board
#   make test-target          the QEMU tests only
#   make holotest-kernel      build/holotest/pipos.img: Pip-OS with its test
#                             backend in the holotape slot, for the QEMU tests
#   make holotapes            build/holo/CULVERT.HTP, the Culvert holotape
#   make gallery              build/gallery/: the widget gallery ROM and its
#                             card, for the QEMU tests only
#   make check-pins           CI pins and doc links match the submodules
#   make weep-table           write src/services/weep_table.c and .h again
#   make weep-table-check     fail when the committed weep table is stale
#   make sdimage              build/pipos/sd.img: the Pip-OS kernel, its sample
#                             PIPOS.CFG, ROM1.ROM and the card tree
#   make card-tree            build/card/: content/ plus the generated radio
#                             tracks, checked; the tree that goes to the card root
#   make check-content        tools/check_content.py on content/ and build/card/
#   make run                  boot the card in QEMU on this terminal
#                             (BOARD=<name>, WINDOW=1 opens the display)
#   make bootdir DIR=<path>   the files of a real card (FIRMWARE=0 leaves
#                             the Raspberry Pi firmware out, CLEAN=1
#                             removes files an earlier run wrote)
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
CFLAGS_EXTRA := -Isrc -DDX_VERSION=$(VERSION)
include $(PIPOS_SDK)/pipos-app.mk

# ---- Holotapes ---------------------------------------------------------------

# Each tape has its own Makefile with the SDK's pipos-app.mk, built into
# build/holo/. `make` builds them with the ROM; the card gets them through
# Pip-OS's TAPE= list.
HOLO_BUILD := $(abspath $(BUILD))/holo
TAPES      := $(HOLO_BUILD)/CULVERT.HTP

.PHONY: holotapes
holotapes:
	$(MAKE) --no-print-directory -C holotapes/culvert BUILD=$(HOLO_BUILD) CI=$(CI) PYTHON=$(PYTHON)

all: holotapes

# ---- The weep's table ---------------------------------------------------

# src/services/weep_table.c and .h come from tools/weep_params.json and
# are committed. `make weep-table` writes them again. The check runs
# before the ROM and the host tests build, so a stale table stops both.
.PHONY: weep-table weep-table-check
all: weep-table-check
$(call pip_obj,src/services/weep_table.c): | weep-table-check
weep-table:
	$(PYTHON) -I tools/mkweep.py

weep-table-check:
	$(PYTHON) -I tools/mkweep.py --check

$(ICONS_C): tools/pbm2c.py $(ICONS)
	$(PYTHON) tools/pbm2c.py $@ $(ICONS)

# ---- Host unit tests, with RobCo-Core's host.mk ---------------------------

# C fixtures from the Python reference tools: the notes tun2notes.py makes
# of each tune, and the broken trees of the checker's test table.
EXPECT_H := $(BUILD)/gen/expect.h
$(EXPECT_H): tests/host/gen_expect.py tools/tun2notes.py tests/tools/test_check_content.py \
             $(shell find content -type f)
	@mkdir -p $(dir $@)
	$(PYTHON) tests/host/gen_expect.py $@

HOST_TEST_DIR := tests/host
# Culvert's pure files build into every host test too.
CV_DIR        := holotapes/culvert
CV_PURE_SRCS  := $(CV_DIR)/logic.c $(CV_DIR)/game.c
HOST_SRCS     := $(PURE_SRCS) $(ICONS_C) $(CV_PURE_SRCS) tests/host/fake_platform.c tests/host/tfmt.c
HOST_INCLUDES := src $(BUILD)/gen $(CV_DIR)
HOST_DEFINES  := DX_VERSION=$(VERSION)
include $(ROBCO_CORE)/mk/host.mk

$(HOST_TESTS): | weep-table-check

# host.mk only sees the headers at the top of src/.
$(HOST_TESTS): $(wildcard src/*/*.h) $(wildcard $(CV_DIR)/*.h) tests/host/fake_platform.h tests/host/tfmt.h $(EXPECT_H) $(wildcard tests/host/*.inc)

# The turns that solve a Culvert level, for the QEMU tests.
CV_SOLVE := $(BUILD)/host/culvert_solve
$(CV_SOLVE): tests/tools/culvert_solve.c $(CV_DIR)/logic.c $(CV_DIR)/logic.h
	@mkdir -p $(dir $@)
	$(HOST_CC) -std=gnu11 -Wall -Wextra -Werror -O1 -I $(CV_DIR) -o $@ $< $(CV_DIR)/logic.c

# QEMU runs and builds below share build/; keep top-level goals serial.
.NOTPARALLEL:

# ---- The card tree: content/ plus the generated radio tracks ------------

# tools/mkpcm.py writes the .PCM tracks into build/pcm/; they are not in
# git. Its --list gives their names, so the list lives in one place. Each
# track is its own target: a missing one is made again. The card tree is
# copied again on every run, so an edit in content/ always reaches it,
# even within the same second. Then the checker runs on it: a station
# whose file is missing stops the build.
CONTENT    ?= content
CARD_TREE  := $(BUILD)/card
PCM_DIR    := $(BUILD)/pcm
PCM_NAMES  := $(shell $(PYTHON) tools/mkpcm.py --list)
PCM_FILES  := $(addprefix $(PCM_DIR)/,$(PCM_NAMES))

ifeq ($(strip $(PCM_NAMES)),)
$(error tools/mkpcm.py --list printed no tracks)
endif

$(PCM_DIR)/%.PCM: tools/mkpcm.py
	$(PYTHON) tools/mkpcm.py $(PCM_DIR) --only $*

.PHONY: card-tree
card-tree: $(PCM_FILES)
	rm -rf $(CARD_TREE)
	mkdir -p $(CARD_TREE)
	cp -R $(CONTENT)/. $(CARD_TREE)/
	cp $(PCM_FILES) $(CARD_TREE)/PIPBOY/DATA/RADIO/
	$(PYTHON) tools/check_content.py $(CARD_TREE)

# ---- Pip-OS: kernel, card, QEMU, real card --------------------------------

# Pip-OS's own Makefile, building into build/pipos, through the interface
# of its ROM_DEVELOPMENT.md, "Using Pip-OS from another repository". ROM
# and TAPE pick the card's ROM and holotapes. CARD_DIR is the card tree,
# copied to the card root after the standard files. Pip-OS checks the
# staged card: a case clash stops the build, a name longer than 8.3 warns.
PIPOS_BUILD := $(abspath $(BUILD))/pipos
PIPOS_MAKE  := $(MAKE) --no-print-directory -C $(PIP_OS) BUILD=$(PIPOS_BUILD) PYTHON=$(PYTHON)
CARD_VARS   := ROM=$(abspath $(PIP_OUT)) TAPE="$(TAPES)" CARD_DIR=$(abspath $(CARD_TREE))
KERNEL      := $(PIPOS_BUILD)/pipos.img
CARD        := $(PIPOS_BUILD)/sd.img

.PHONY: kernel holotest-kernel sdimage run bootdir
kernel:
	$(PIPOS_MAKE) kernel

# A test kernel with Pip-OS's RAM card backend in the holotape slot, for
# tests/test_slot.py. Pip-OS's own tests build it the same way: its
# Makefile adds a command-line EXTRA_CFLAGS to its own. It is never
# shipped: the slot here is a test backend, not the real driver.
HOLOTEST_BUILD := $(abspath $(BUILD))/holotest
holotest-kernel:
	$(MAKE) --no-print-directory -C $(PIP_OS) BUILD=$(HOLOTEST_BUILD) PYTHON=$(PYTHON) \
	    EXTRA_CFLAGS=-DPIPOS_HOLO_TEST kernel

sdimage: all card-tree
	$(PIPOS_MAKE) sdimage $(CARD_VARS)

run: all card-tree
	$(PIPOS_MAKE) run $(CARD_VARS) BOARD=$(BOARD) WINDOW=$(WINDOW)

# The same files as the card, firmware included. FIRMWARE=0 leaves the
# firmware out, CLEAN=1 removes files an earlier run wrote.
bootdir: all card-tree
	$(if $(strip $(DIR)),,$(error DIR is not set: make bootdir DIR=<path>))
	$(PIPOS_MAKE) bootdir $(CARD_VARS) DIR=$(abspath $(DIR)) FIRMWARE=$(FIRMWARE) CLEAN=$(CLEAN)

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
.PHONY: check-content tools-test check-format

# RobCo-Core's scripts: the CI pins, then the doc links once per pinned
# repository, Pip-OS and the RobCo-Core inside it.
check-pins:
	$(ROBCO_CORE)/tools/check_pins.sh .github/workflows/ci.yml
	$(ROBCO_CORE)/tools/check_doc_links.sh --repo RobCo-Industries/Pip-OS . $(PIP_OS)
	$(ROBCO_CORE)/tools/check_doc_links.sh . $(ROBCO_CORE)

check-pins-test:
	tests/host/check_pins_test.sh

# No unchecked libc string writer in src/ or tests/host/ (docs/TESTING.md).
check-format:
	$(PYTHON) tests/host/check_format_calls.py

pbm2c-test:
	$(PYTHON) tests/host/test_pbm2c.py

sdimage-test: sdimage
	tests/host/sdimage_test.sh $(CARD) $(PIP_OUT) $(CARD_TREE) $(TAPES)

# content/ alone: the generated tracks may be missing there. card-tree
# checks the built tree, where they must be present.
check-content: card-tree
	$(PYTHON) tools/check_content.py $(addprefix --generated ,$(PCM_NAMES)) $(CONTENT)

# The tests of the Python tools: unittest, so no pytest is needed.
tools-test:
	$(PYTHON) -m unittest discover -s tests/tools -p 'test_*.py'

# The C host tests come from host.mk's test-host; these run first.
test-host: weep-table-check check-format check-pins check-pins-test pbm2c-test check-content tools-test sdimage-test

VENV := .venv
venv: $(VENV)/.installed
$(VENV)/.installed: $(ROBCO_CORE)/tests/requirements.txt
	$(PYTHON) -m venv $(VENV)
	$(VENV)/bin/pip install -q -r $<
	touch $@

test: test-host
	$(MAKE) --no-print-directory test-target

test-target: sdimage kernel holotest-kernel gallery $(CV_SOLVE) venv
	$(VENV)/bin/pytest tests -v --board $(BOARD) --build-dir $(BUILD)
