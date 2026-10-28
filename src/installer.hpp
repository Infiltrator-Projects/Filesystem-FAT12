// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "removal_guard.hpp"

#include <functional>
#include <string>
#include <vector>

namespace filesystems {

using ActionCompletion = std::function<void(bool, const std::string&)>;

struct RemovalPlan {
    bool allowed = false;
    std::vector<std::string> planned_packages;
    std::vector<std::string> affected_entries;
    std::string reason;
};

using RemovalPlanCompletion = std::function<void(RemovalPlan)>;

RemovalPlan plan_package_removal(const std::vector<std::string>& packages);
void plan_package_removal_async(const std::vector<std::string>& packages,
                                RemovalPlanCompletion completion);

// Shared by the unprivileged planner and the locked APT hook. Protection
// metadata must be checked again after authentication, before dpkg mutates it.
bool approved_removal_is_allowed(
    const std::vector<std::string>& canonical_packages,
    std::string* reason);

void install_packages_async(const std::vector<std::string>& packages,
                            ActionCompletion completion);
void remove_packages_async(
    const std::vector<std::string>& packages,
    const std::vector<std::string>& approved_planned_packages,
    ActionCompletion completion);
void manage_fat16_module_async(bool remove, ActionCompletion completion);
void manage_fat12_module_async(bool remove, ActionCompletion completion);
void manage_fat32_module_async(bool remove, ActionCompletion completion);
void manage_ofs_module_async(bool remove, ActionCompletion completion);
void manage_ext2_dropin_async(bool remove, ActionCompletion completion);
void load_module_async(const std::string& module,
                       ActionCompletion completion);
void unload_module_async(const std::string& module,
                         ActionCompletion completion);
void install_native_module_async(const std::string& filesystem_id,
                                 const std::string& module,
                                 ActionCompletion completion);
void remove_native_module_async(const std::string& filesystem_id,
                                const std::string& module,
                                ActionCompletion completion);

} // namespace filesystems
