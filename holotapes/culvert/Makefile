# Culvert, a holotape built with the Pip-OS SDK. The top Makefile calls
# it with `make holotapes`; BUILD is then build/holo. robco.toml beside
# it is the same project for RobCo Studio: keep NAME, TITLE, VERSION,
# ICON, SUBTYPE, ABI and SRCS equal to it, and change ABI in both at once.
PIPOS_SDK ?= ../../external/pip-os/sdk
BUILD     ?= ../../build/holo
IMAGE     := CULVERT.HTP
KIND      := holotape
SUBTYPE   := game
NAME      := CULVERT
TITLE     := Culvert
VERSION   := 1.0.0
ICON      := icon.pbm
ABI       := 1.4
SRCS      := main.c game.c logic.c draw.c
include $(PIPOS_SDK)/pipos-app.mk
