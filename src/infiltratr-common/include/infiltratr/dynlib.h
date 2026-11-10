// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file dynlib.h
 * @brief Small cross-platform dynamic-library loader for shared C backends.
 *
 * The interface deliberately exposes no POSIX or Win32 loader types. It owns
 * only module lifetime and safe symbol-byte transfer into caller storage;
 * library names, version probing and required/optional symbol policy remain
 * application responsibilities.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATR_COMMON_DYNLIB_H
#define INFILTRATR_COMMON_DYNLIB_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque native module handle stored without leaking platform headers. */
typedef struct {
    void *handle;
} InfiltratrDynlib;

#define INFILTRATR_DYNLIB_INIT { .handle = NULL }

/**
 * One symbol-binding request for infiltratr_dynlib_bind_symbols().
 *
 * destination points to caller-owned function/data-pointer storage and
 * destination_size must equal the native symbol-pointer representation size.
 * Missing optional symbols are committed as NULL only when the whole binding
 * set otherwise succeeds.
 */
typedef struct {
    const char *name;
    void *destination;
    size_t destination_size;
    bool required;
} InfiltratrDynlibBinding;

/**
 * Open one native dynamic library by UTF-8 file name.
 *
 * POSIX forwards the UTF-8 byte sequence to the native loader. Windows
 * validates UTF-8, converts it to UTF-16 and calls LoadLibraryW, avoiding the
 * process ANSI code page. The object must not already own a library. On
 * failure it remains closed.
 */
bool infiltratr_dynlib_open(InfiltratrDynlib *library, const char *name);

/** Return whether the object currently owns a native library handle. */
bool infiltratr_dynlib_is_open(const InfiltratrDynlib *library);

/**
 * Close an owned library. NULL and already-closed objects are safe no-ops.
 * The handle is cleared only when the native loader reports a successful
 * unload; an unload failure therefore remains observable through is_open().
 */
void infiltratr_dynlib_close(InfiltratrDynlib *library);

/**
 * Resolve a symbol and copy its pointer representation into caller storage.
 *
 * This avoids non-portable direct casts from `dlsym`/`GetProcAddress` results
 * to function pointers. `destination_size` must match the native symbol-pointer
 * representation size. Caller storage is left unchanged on failure. On POSIX,
 * loader errors are detected with dlerror(), so a successfully resolved symbol
 * whose value is NULL is not confused with lookup failure.
 */
bool infiltratr_dynlib_symbol(const InfiltratrDynlib *library,
                              const char *name,
                              void *destination,
                              size_t destination_size);

/**
 * Resolve a table of required/optional symbols as one atomic caller update.
 *
 * Binding records are validated before any caller destination is modified.
 * If a required symbol cannot be resolved, false is returned and every
 * destination retains its original value. On success, resolved symbols are
 * written to their destinations and missing optional symbols become NULL.
 * A zero binding_count is a successful no-op.
 */
bool infiltratr_dynlib_bind_symbols(const InfiltratrDynlib *library,
                                    const InfiltratrDynlibBinding *bindings,
                                    size_t binding_count);

#ifdef __cplusplus
}
#endif

#endif
