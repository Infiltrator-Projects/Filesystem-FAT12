// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file posix_path_smoke.c
 * @brief Regression coverage for POSIX lexical-path helpers.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 1993-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/posix_path.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(strcmp(infiltratr_path_basename("alpha"), "alpha") == 0);
    assert(strcmp(infiltratr_path_basename("/alpha/beta"), "beta") == 0);
    assert(strcmp(infiltratr_path_basename("/alpha/beta/"), "") == 0);
    assert(strcmp(infiltratr_path_basename("/"), "") == 0);
    assert(strcmp(infiltratr_path_basename(""), "") == 0);
    assert(strcmp(infiltratr_path_basename(NULL), "") == 0);

    char parent[32];
    assert(infiltratr_path_dirname("/alpha/beta", parent, sizeof(parent)));
    assert(strcmp(parent, "/alpha") == 0);
    assert(infiltratr_path_dirname("alpha/beta/", parent, sizeof(parent)));
    assert(strcmp(parent, "alpha") == 0);
    assert(infiltratr_path_dirname("alpha//beta", parent, sizeof(parent)));
    assert(strcmp(parent, "alpha") == 0);
    assert(infiltratr_path_dirname("/alpha", parent, sizeof(parent)));
    assert(strcmp(parent, "/") == 0);
    assert(infiltratr_path_dirname("alpha", parent, sizeof(parent)));
    assert(strcmp(parent, ".") == 0);
    assert(infiltratr_path_dirname("", parent, sizeof(parent)));
    assert(strcmp(parent, ".") == 0);
    assert(!infiltratr_path_dirname(NULL, parent, sizeof(parent)));
    assert(parent[0] == '\0');
    char tiny[2] = {'x', '\0'};
    assert(!infiltratr_path_dirname("/alpha/beta", tiny, sizeof(tiny)));
    assert(tiny[0] == '\0');
    puts("Libraries POSIX path tests passed.");
    return 0;
}
