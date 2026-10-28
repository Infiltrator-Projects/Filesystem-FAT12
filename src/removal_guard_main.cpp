// SPDX-License-Identifier: GPL-3.0-or-later
#include "installer.hpp"
#include "removal_guard.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const char* const locked = std::getenv("DPKG_FRONTEND_LOCKED");
    if (locked == nullptr || std::string(locked) != "true") {
        std::cerr << "Removal guard requires APT's locked transaction.\n";
        return 1;
    }

    const std::vector<std::string> approved(argv + 1, argv + argc);
    std::string reason;
    if (!filesystems::approved_removal_is_allowed(approved, &reason) ||
        !filesystems::removal_hook_matches_approval(std::cin, approved, &reason)) {
        std::cerr << reason << '\n';
        return 1;
    }
    return 0;
}
