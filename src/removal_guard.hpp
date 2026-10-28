// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <istream>
#include <string>
#include <vector>

namespace filesystems {

// Canonical identities include the installed architecture, including "all".
// Unqualified names are deliberately insufficient authority for an APT hook.
bool removal_identity_is_valid(const std::string& identity);
std::string removal_package_name(const std::string& identity);

bool removal_plan_matches_approval(
    const std::vector<std::string>& approved_packages,
    const std::vector<std::string>& current_packages);

// Consume APT's version-3 Pre-Install-Pkgs stream. APT holds its frontend
// lock across this check and the following dpkg operations; a nonzero hook
// exit aborts that same transaction before dpkg receives any removal.
bool removal_hook_matches_approval(
    std::istream& input,
    const std::vector<std::string>& approved_packages,
    std::string* reason);

} // namespace filesystems
