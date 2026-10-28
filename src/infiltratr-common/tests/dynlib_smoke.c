// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file dynlib_smoke.c
 * @brief Contract smoke test for the native dynamic-library adapter.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/dynlib.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>

int main(void)
{
    InfiltratrDynlib library = INFILTRATR_DYNLIB_INIT;
    double (*cosine)(double) = NULL;

#ifdef _WIN32
    const char *library_name = "msvcrt.dll";
    const char *symbol_name = "cos";
#else
    const char *library_name = "libm.so.6";
    const char *symbol_name = "cos";
#endif

    assert(!infiltratr_dynlib_is_open(&library));
    assert(!infiltratr_dynlib_open(NULL, library_name));
    assert(!infiltratr_dynlib_open(&library, ""));
#ifdef _WIN32
    {
        static const char invalid_utf8[] = {(char)0xff, '\0'};
        assert(!infiltratr_dynlib_open(&library, invalid_utf8));
    }
#endif
    assert(infiltratr_dynlib_open(&library, library_name));
    assert(infiltratr_dynlib_is_open(&library));
    assert(!infiltratr_dynlib_open(&library, library_name));
    assert(infiltratr_dynlib_symbol(&library,
                                    symbol_name,
                                    &cosine,
                                    sizeof(cosine)));
    assert(cosine != NULL);
    assert(fabs(cosine(0.0) - 1.0) < 1.0e-12);
    double (*const resolved_cosine)(double) = cosine;
    assert(!infiltratr_dynlib_symbol(&library,
                                     "infiltratr_symbol_that_does_not_exist",
                                     &cosine,
                                     sizeof(cosine)));
    assert(cosine == resolved_cosine);

    double (*bound_cosine)(double) = NULL;
    double (*optional_missing)(double) = resolved_cosine;
    const InfiltratrDynlibBinding successful_bindings[] = {
        { symbol_name, &bound_cosine, sizeof(bound_cosine), true },
        { "infiltratr_optional_symbol_that_does_not_exist",
          &optional_missing, sizeof(optional_missing), false }
    };
    assert(infiltratr_dynlib_bind_symbols(
        &library, successful_bindings,
        sizeof(successful_bindings) / sizeof(successful_bindings[0])));
    assert(bound_cosine != NULL);
    assert(fabs(bound_cosine(0.0) - 1.0) < 1.0e-12);
    assert(optional_missing == NULL);

    double (*atomic_first)(double) = resolved_cosine;
    double (*atomic_required_missing)(double) = resolved_cosine;
    const InfiltratrDynlibBinding failing_bindings[] = {
        { symbol_name, &atomic_first, sizeof(atomic_first), true },
        { "infiltratr_required_symbol_that_does_not_exist",
          &atomic_required_missing, sizeof(atomic_required_missing), true }
    };
    assert(!infiltratr_dynlib_bind_symbols(
        &library, failing_bindings,
        sizeof(failing_bindings) / sizeof(failing_bindings[0])));
    assert(atomic_first == resolved_cosine);
    assert(atomic_required_missing == resolved_cosine);
    assert(infiltratr_dynlib_bind_symbols(&library, NULL, 0U));

    infiltratr_dynlib_close(&library);
    assert(!infiltratr_dynlib_is_open(&library));
    infiltratr_dynlib_close(&library);
    return 0;
}
