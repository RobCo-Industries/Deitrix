# A test holotape for the DATA tab that faults on purpose: Pip-OS ends
# it and holo_launch returns -PIP_EFAULT. tests/test_data.py builds it.
PIPOS_SDK ?= ../../../external/pip-os/sdk
IMAGE     := FAULT.HTP
KIND      := holotape
NAME      := FAULT
TITLE     := Fault test tape
VERSION   := 1.0.0
SUBTYPE   := utility
SRCS      := main.c
include $(PIPOS_SDK)/pipos-app.mk
