// SPDX-License-Identifier: GPL-3.0-or-later
#include "removal_guard.hpp"

#include <algorithm>
#include <array>
#include <sstream>

namespace filesystems {
namespace {

bool identifier(const std::string& value, const bool architecture)
{
    if (value.empty() || value.front() < 'a' || value.front() > 'z') {
        // Debian package and architecture names may also begin with a digit.
        if (value.empty() || value.front() < '0' || value.front() > '9') {
            return false;
        }
    }
    return std::all_of(value.begin(), value.end(), [architecture](const char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '-' || (!architecture && (c == '+' || c == '.'));
    });
}

bool refuse(std::string* reason, const char* message)
{
    if (reason != nullptr) {
        *reason = message;
    }
    return false;
}

} // namespace

bool removal_identity_is_valid(const std::string& identity)
{
    const auto colon = identity.find(':');
    return colon != std::string::npos &&
           identifier(identity.substr(0U, colon), false) &&
           identifier(identity.substr(colon + 1U), true);
}

std::string removal_package_name(const std::string& identity)
{
    return identity.substr(0U, identity.find(':'));
}

bool removal_plan_matches_approval(
    const std::vector<std::string>& approved_packages,
    const std::vector<std::string>& current_packages)
{
    std::vector<std::string> approved = approved_packages;
    std::vector<std::string> current = current_packages;
    std::sort(approved.begin(), approved.end());
    std::sort(current.begin(), current.end());
    return approved == current;
}

bool removal_hook_matches_approval(
    std::istream& input,
    const std::vector<std::string>& approved_packages,
    std::string* reason)
{
    if (approved_packages.empty() ||
        !std::all_of(approved_packages.begin(), approved_packages.end(),
                     removal_identity_is_valid)) {
        return refuse(reason, "The approved removal identities are invalid.");
    }

    std::string line;
    if (!std::getline(input, line) || line != "VERSION 3") {
        return refuse(reason, "APT did not provide its architecture-aware removal protocol.");
    }

    // Configuration values are percent-encoded and cannot contain literal
    // newlines. The first empty line therefore unambiguously begins actions.
    while (std::getline(input, line) && !line.empty()) {}
    if (!input || !line.empty()) {
        return refuse(reason, "APT supplied an incomplete removal transaction.");
    }

    std::vector<std::string> removals;
    while (std::getline(input, line)) {
        std::istringstream action(line);
        std::array<std::string, 9U> fields;
        for (auto& field : fields) {
            if (!(action >> field)) {
                return refuse(reason, "APT supplied a malformed package action.");
            }
        }
        std::string extra;
        if ((action >> extra) || fields[8U] != "**REMOVE**" ||
            fields[1U] == "-" || fields[5U] != "-" ||
            fields[6U] != "-" || fields[4U] != ">") {
            return refuse(reason, "APT proposed an operation outside the approved removal.");
        }
        const std::string identity = fields[0U] + ':' + fields[2U];
        if (!removal_identity_is_valid(identity)) {
            return refuse(reason, "APT supplied an invalid installed-package identity.");
        }
        removals.push_back(identity);
    }
    if (!input.eof() || !removal_plan_matches_approval(approved_packages, removals)) {
        return refuse(reason,
            "The Debian removal plan changed after confirmation. Nothing was removed; "
            "review and confirm the new plan.");
    }
    return true;
}

} // namespace filesystems
