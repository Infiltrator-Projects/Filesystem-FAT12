// SPDX-License-Identifier: GPL-3.0-or-later
#include "removal_guard.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const std::string kRemoval = "ntfs-3g 1 amd64 none > - - none **REMOVE**\n";
const std::string kOther = "hfsplus 1 amd64 none > - - none **REMOVE**\n";

bool accepts(const std::string& stream,
             const std::vector<std::string>& approved = {"ntfs-3g:amd64"})
{
    std::istringstream input(stream);
    std::string reason;
    const bool result = filesystems::removal_hook_matches_approval(input, approved, &reason);
    if (!result && reason.empty()) {
        std::cerr << "Rejected transaction has no diagnostic.\n";
        std::exit(1);
    }
    return result;
}

} // namespace

int main()
{
    const std::string header = "VERSION 3\nAPT::Architecture=amd64\n\n";
    if (!accepts(header + kRemoval) ||
        !accepts(header + kOther + kRemoval, {"ntfs-3g:amd64", "hfsplus:amd64"}) ||
        !accepts("VERSION 3\n\nntfs-3g 1 all foreign > - - none **REMOVE**\n",
                 {"ntfs-3g:all"})) {
        std::cerr << "Exact approved removal was refused.\n";
        return 1;
    }

    for (const auto& stream : std::vector<std::string> {
             "", "VERSION 2\n\n" + kRemoval, "VERSION 3\nAPT::Architecture=amd64\n",
             header, header + kOther, header + kRemoval + kOther,
             header + kRemoval + kRemoval,
             header + "ntfs-3g 1 i386 none > - - none **REMOVE**\n",
             header + "ntfs-3g 1 amd64 none = 1 amd64 none **CONFIGURE**\n",
             header + "ntfs-3g 1 amd64 none < 2 amd64 none /tmp/package.deb\n",
             header + "ntfs-3g 1 amd64 none > - - none **REMOVE** extra\n",
             header + "ntfs-3g 1 amd64 none > - - none\n",
             header + "ntfs-3g 1 - none > - - none **REMOVE**\n",
             header + kRemoval + '\n'}) {
        if (accepts(stream)) {
            std::cerr << "Changed or malformed transaction was accepted: " << stream;
            return 1;
        }
    }
    if (accepts(header + kRemoval, {}) ||
        accepts(header + kRemoval, {"ntfs-3g"}) ||
        accepts(header + kRemoval, {"ntfs-3g:amd64", "ntfs-3g:amd64"})) {
        std::cerr << "Invalid approval was accepted.\n";
        return 1;
    }
    std::cout << "APT removal transaction protocol passed.\n";
    return 0;
}
