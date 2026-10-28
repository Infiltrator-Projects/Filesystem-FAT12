// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>

/* Application paths are an installation contract, not a Linux FHS assumption.
 * The build sets one root shared by the executable, sources and helpers.
 * Only the selected compiled .so belongs under /System/Filesystems/.
 */
#ifndef FILESYSTEMS_APPLICATION_ROOT
#define FILESYSTEMS_APPLICATION_ROOT "/Programs/Filesystems"
#endif

namespace filesystems {
inline std::string application_helper(const std::string_view name)
{
    return std::string(FILESYSTEMS_APPLICATION_ROOT) + "/Helpers/" +
           std::string(name);
}
} // namespace filesystems
