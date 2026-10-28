// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>

namespace filesystems::linux_host {

// Linux compatibility helpers are not native InfiltratorOS application paths.
// Any future native host must implement and qualify its own backend.
std::string native_module_helper_path();
std::string package_removal_guard_path();

} // namespace filesystems::linux_host
