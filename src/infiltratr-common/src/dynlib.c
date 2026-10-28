// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file dynlib.c
 * @brief POSIX/Win32 implementation of the shared dynamic-library boundary.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/dynlib.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

static wchar_t *utf8_library_name(const char *name)
{
    const int characters = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                                name, -1, NULL, 0);
    if (characters <= 0 ||
        (size_t)characters > SIZE_MAX / sizeof(wchar_t))
        return NULL;

    wchar_t *wide = malloc((size_t)characters * sizeof(*wide));
    if (!wide) return NULL;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1,
                            wide, characters) != characters) {
        free(wide);
        return NULL;
    }
    return wide;
}
#else
#include <dlfcn.h>
#endif

bool infiltratr_dynlib_open(InfiltratrDynlib *library, const char *name)
{
    if (library == NULL || name == NULL || name[0] == '\0' ||
        library->handle != NULL) {
        return false;
    }

#ifdef _WIN32
    wchar_t *wide = utf8_library_name(name);
    if (!wide) return false;
    library->handle = (void *)LoadLibraryW(wide);
    free(wide);
#else
    library->handle = dlopen(name, RTLD_NOW | RTLD_LOCAL);
#endif
    return library->handle != NULL;
}

bool infiltratr_dynlib_is_open(const InfiltratrDynlib *library)
{
    return library != NULL && library->handle != NULL;
}

void infiltratr_dynlib_close(InfiltratrDynlib *library)
{
    if (library == NULL || library->handle == NULL) {
        return;
    }

#ifdef _WIN32
    if (FreeLibrary((HMODULE)library->handle) != 0) library->handle = NULL;
#else
    if (dlclose(library->handle) == 0) library->handle = NULL;
#endif
}

bool infiltratr_dynlib_symbol(const InfiltratrDynlib *library,
                              const char *name,
                              void *destination,
                              size_t destination_size)
{
    void *symbol = NULL;

    if (!infiltratr_dynlib_is_open(library) || name == NULL ||
        name[0] == '\0' || destination == NULL ||
        destination_size != sizeof(symbol)) {
        return false;
    }

#ifdef _WIN32
    {
        const FARPROC procedure =
            GetProcAddress((HMODULE)library->handle, name);

        _Static_assert(sizeof(symbol) == sizeof(procedure),
                       "Win32 function and data pointers must have equal size");
        if (procedure == NULL) {
            return false;
        }
        memcpy(&symbol, &procedure, sizeof(symbol));
    }
#else
    (void)dlerror();
    symbol = dlsym(library->handle, name);
    if (dlerror() != NULL) return false;
#endif

    memcpy(destination, &symbol, sizeof(symbol));
    return true;
}

bool infiltratr_dynlib_bind_symbols(const InfiltratrDynlib *library,
                                    const InfiltratrDynlibBinding *bindings,
                                    size_t binding_count)
{
    void **resolved;

    if (binding_count == 0U) return true;
    if (!infiltratr_dynlib_is_open(library) || bindings == NULL ||
        binding_count > SIZE_MAX / sizeof(*resolved))
        return false;

    resolved = calloc(binding_count, sizeof(*resolved));
    if (resolved == NULL) return false;

    for (size_t index = 0U; index < binding_count; ++index) {
        const InfiltratrDynlibBinding *binding = &bindings[index];

        if (binding->name == NULL || binding->name[0] == '\0' ||
            binding->destination == NULL ||
            binding->destination_size != sizeof(resolved[index])) {
            free(resolved);
            return false;
        }

        if (!infiltratr_dynlib_symbol(library, binding->name,
                                      &resolved[index],
                                      sizeof(resolved[index])) &&
            binding->required) {
            free(resolved);
            return false;
        }
    }

    for (size_t index = 0U; index < binding_count; ++index)
        memcpy(bindings[index].destination, &resolved[index],
               sizeof(resolved[index]));

    free(resolved);
    return true;
}
