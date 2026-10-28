// SPDX-License-Identifier: GPL-3.0-or-later
#include "linux_host_paths.hpp"

#include <glib.h>
#include <string>

namespace {

bool valid_linux_helper(const std::string& found, const char* packaged,
                        const char* local)
{
    // Absence is legitimate on a development host; alternate locations are not.
    return found.empty() ||
           ((found == packaged || found == local) &&
            g_file_test(found.c_str(), G_FILE_TEST_IS_REGULAR) &&
            g_file_test(found.c_str(), G_FILE_TEST_IS_EXECUTABLE));
}

} // namespace

int main()
{
    return valid_linux_helper(
               filesystems::linux_host::native_module_helper_path(),
               "/usr/lib/filesystems/native-module-helper",
               "/usr/local/lib/filesystems/native-module-helper") &&
           valid_linux_helper(
               filesystems::linux_host::package_removal_guard_path(),
               "/usr/lib/filesystems/package-removal-guard",
               "/usr/local/lib/filesystems/package-removal-guard")
        ? 0 : 1;
}
