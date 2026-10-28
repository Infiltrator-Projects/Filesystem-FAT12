.DEFAULT_GOAL := all
PYTHON ?= python3
WIDTH := $(shell cat standalone/WIDTH)
MODULE := FAT$(WIDTH).so
.PHONY: all check release clean
all:
	$(PYTHON) standalone/build.py --profile portable --output build/$(MODULE)
check:
	$(PYTHON) standalone/build.py --profile portable --output build/$(MODULE) --check
release:
	$(PYTHON) standalone/release.py release
clean:
	rm -rf build dist
