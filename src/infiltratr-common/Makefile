# SPDX-License-Identifier: GPL-3.0-or-later
CC ?= cc
AR ?= ar
BUILD_DIR ?= build
COMMON_VERSION := $(shell tr -d '[:space:]' < VERSION)
CPPFLAGS += -Iinclude
override CPPFLAGS += -DINFILTRATR_COMMON_LIBRARY_ARCHITECTURE=\"$(shell $(CC) -dumpmachine)\"
CFLAGS ?= -O2 -g
override CFLAGS += -std=c11 -fPIC -Wall -Wextra -Wpedantic -Werror \
	-Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes
PORTABLE_OBJECTS := $(BUILD_DIR)/core.o $(BUILD_DIR)/arithmetic.o \
	$(BUILD_DIR)/config.o $(BUILD_DIR)/i18n.o $(BUILD_DIR)/token.o \
	$(BUILD_DIR)/timing.o $(BUILD_DIR)/temporal.o $(BUILD_DIR)/temporal_clock.o $(BUILD_DIR)/format.o $(BUILD_DIR)/quantity.o $(BUILD_DIR)/graphics.o \
	$(BUILD_DIR)/escape.o $(BUILD_DIR)/design.o
OBJECTS := $(PORTABLE_OBJECTS) $(BUILD_DIR)/dynlib.o $(BUILD_DIR)/posix.o \
	$(BUILD_DIR)/posix_path.o $(BUILD_DIR)/posix_io.o $(BUILD_DIR)/posix_numeric.o \
	$(BUILD_DIR)/temporal_posix.o $(BUILD_DIR)/temporal_calendar_posix.o
PORTABLE_ARCHIVE := $(BUILD_DIR)/libinfiltratr-portable.a
ARCHIVE := $(BUILD_DIR)/libinfiltratr-common.a
SHARED := $(BUILD_DIR)/libinfiltratr-common.so.$(COMMON_VERSION)
DYNLIB_LIBS := -ldl

.PHONY: all check clean portable portable-check shared version-check
version-check:
	@grep -qx '#define INFILTRATR_COMMON_VERSION "$(COMMON_VERSION)"' include/infiltratr/core.h || { echo "VERSION and INFILTRATR_COMMON_VERSION disagree" >&2; exit 1; }
all: version-check $(ARCHIVE)
$(BUILD_DIR):
	mkdir -p "$@"
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(PORTABLE_ARCHIVE): $(PORTABLE_OBJECTS)
	$(AR) rcsD $@ $(PORTABLE_OBJECTS)
portable: version-check $(PORTABLE_ARCHIVE)
$(ARCHIVE): $(OBJECTS)
	$(AR) rcsD $@ $(OBJECTS)
shared: version-check $(SHARED)
$(SHARED): $(OBJECTS)
	$(CC) -shared -Wl,-soname,libinfiltratr-common.so.1 -o $@ $(OBJECTS) -lm $(DYNLIB_LIBS)

$(BUILD_DIR)/core-smoke: tests/core_smoke.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm -o $@
$(BUILD_DIR)/arithmetic-smoke: tests/arithmetic_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/config-smoke: tests/config_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/i18n-smoke: tests/i18n_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/token-smoke: tests/token_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/timing-smoke: tests/timing_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/temporal-smoke: tests/temporal_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/graphics-smoke: tests/graphics_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/escape-smoke: tests/escape_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/design-smoke: tests/design_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/format-smoke: tests/format_smoke.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm -o $@
$(BUILD_DIR)/dynlib-smoke: tests/dynlib_smoke.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm $(DYNLIB_LIBS) -o $@
$(BUILD_DIR)/portable-smoke: tests/portable_smoke.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/portable-contract: tests/portable_contract.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/encoding-contract: tests/encoding_contract.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/completion-contract: tests/completion_contract.c $(PORTABLE_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(PORTABLE_ARCHIVE) -lm -o $@
$(BUILD_DIR)/posix-contract: tests/posix_contract.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm -o $@
$(BUILD_DIR)/posix-path-smoke: tests/posix_path_smoke.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm -o $@
$(BUILD_DIR)/posix-io-contract: tests/posix_io_contract.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm -o $@
$(BUILD_DIR)/posix-numeric-contract: tests/posix_numeric_contract.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm -o $@
$(BUILD_DIR)/temporal-posix-smoke: tests/temporal_posix_smoke.c $(ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(ARCHIVE) -lm -o $@
$(BUILD_DIR)/temporal-date-fixture.so: tests/temporal_date_fixture.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared $< -o $@
$(BUILD_DIR)/temporal-date-smoke: tests/temporal_date_smoke.c $(ARCHIVE) $(BUILD_DIR)/temporal-date-fixture.so
	$(CC) $(CPPFLAGS) $(CFLAGS) -DTEMPORAL_DATE_FIXTURE_PATH=\"$(abspath $(BUILD_DIR)/temporal-date-fixture.so)\" $< $(ARCHIVE) -lm $(DYNLIB_LIBS) -o $@
portable-check: version-check $(BUILD_DIR)/portable-smoke $(BUILD_DIR)/portable-contract \
	$(BUILD_DIR)/encoding-contract $(BUILD_DIR)/completion-contract \
	$(BUILD_DIR)/arithmetic-smoke $(BUILD_DIR)/config-smoke $(BUILD_DIR)/i18n-smoke \
	$(BUILD_DIR)/token-smoke $(BUILD_DIR)/timing-smoke $(BUILD_DIR)/temporal-smoke $(BUILD_DIR)/graphics-smoke \
	$(BUILD_DIR)/escape-smoke $(BUILD_DIR)/design-smoke
	./$(BUILD_DIR)/portable-smoke
	./$(BUILD_DIR)/portable-contract
	./$(BUILD_DIR)/encoding-contract
	./$(BUILD_DIR)/completion-contract
	./$(BUILD_DIR)/arithmetic-smoke
	./$(BUILD_DIR)/config-smoke
	./$(BUILD_DIR)/i18n-smoke
	./$(BUILD_DIR)/token-smoke
	./$(BUILD_DIR)/timing-smoke
	./$(BUILD_DIR)/temporal-smoke
	./$(BUILD_DIR)/graphics-smoke
	./$(BUILD_DIR)/escape-smoke
	./$(BUILD_DIR)/design-smoke
check: portable-check $(BUILD_DIR)/core-smoke $(BUILD_DIR)/format-smoke \
	$(BUILD_DIR)/dynlib-smoke $(BUILD_DIR)/posix-contract \
	$(BUILD_DIR)/posix-path-smoke $(BUILD_DIR)/posix-io-contract \
	$(BUILD_DIR)/posix-numeric-contract $(BUILD_DIR)/temporal-posix-smoke $(BUILD_DIR)/temporal-date-smoke
	./$(BUILD_DIR)/core-smoke
	./$(BUILD_DIR)/format-smoke
	./$(BUILD_DIR)/dynlib-smoke
	./$(BUILD_DIR)/posix-contract
	./$(BUILD_DIR)/posix-path-smoke
	./$(BUILD_DIR)/posix-io-contract
	./$(BUILD_DIR)/posix-numeric-contract
	./$(BUILD_DIR)/temporal-posix-smoke
	./$(BUILD_DIR)/temporal-date-smoke
clean:
	rm -rf "$(BUILD_DIR)"
