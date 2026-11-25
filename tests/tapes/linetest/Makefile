# A test holotape for the DATA tab: it draws a line, waits for SELECT
# and exits with status 42. tests/test_data.py builds it with
# PIPOS_SDK and BUILD set.
PIPOS_SDK ?= ../../../external/pip-os/sdk
IMAGE     := LINETEST.HTP
KIND      := holotape
NAME      := LINETEST
TITLE     := Line test tape
VERSION   := 1.0.0
ICON      := icon.pbm
SRCS      := main.c
include $(PIPOS_SDK)/pipos-app.mk
