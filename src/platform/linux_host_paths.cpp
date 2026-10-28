// SPDX-License-Identifier: GPL-3.0-or-later
#include "linux_host_paths.hpp"

#include <glib.h>

#include <initializer_list>
#include <string>

#if !defined(__linux__)
#error "This Linux helper backend must not be compiled for native InfiltratorOS."
#endif

namespace filesystems::linux_host {
namespace {

std::string first_executable(std::initializer_list<const char*> candidates)
{
    for (const char* path : candidates) {
        // Same qualified Linux host order and file tests as the old callers.
        if (g_file_test(path, G_FILE_TEST_IS_REGULAR) &&
            g_file_test(path, G_FILE_TEST_IS_EXECUTABLE)) {
            return path;
        }
    }
    return {};
}

} // namespace

std::string native_module_helper_path()
{
    return first_executable({
        "/usr/lib/filesystems/native-module-helper",
        "/usr/local/lib/filesystems/native-module-helper",
    });
}

std::string package_removal_guard_path()
{
    return first_executable({
        "/usr/lib/filesystems/package-removal-guard",
        "/usr/local/lib/filesystems/package-removal-guard",
    });
}

} // namespace filesystems::linux_host
