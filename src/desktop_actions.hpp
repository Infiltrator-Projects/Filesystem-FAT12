// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "catalog.hpp"
#include "probe.hpp"

#include <functional>
#include <string>
#include <vector>

namespace filesystems {

using DesktopActionCompletion =
    std::function<void(bool, const std::string&)>;

struct DesktopAction;
using DesktopActionPrepared = std::function<void(DesktopAction)>;

enum class DesktopActionDisposition {
    None,
    PlanningRemoval,
    Execute,
    ConfirmRemoval,
    Refresh,
    Blocked
};

enum class DesktopActionKind {
    None,
    InstallPackages,
    RemovePackages,
    InstallNativeModule,
    RemoveNativeModule,
    InstallDropIn,
    RemoveDropIn,
    LoadModule,
    UnloadModule
};

struct DesktopAction {
    DesktopActionDisposition disposition = DesktopActionDisposition::None;
    DesktopActionKind kind = DesktopActionKind::None;
    std::string busy_label;
    std::string failure_title;
    std::string message;
    std::vector<std::string> planned_packages;
    std::vector<std::string> affected_entries;
    std::vector<std::string> packages;
    std::string filesystem_id;
    std::string module;
};

DesktopAction prepare_package_desktop_action(
    const FilesystemDescriptor& descriptor,
    const ProbeResult& probe);

void prepare_package_desktop_action_async(
    const FilesystemDescriptor& descriptor,
    const ProbeResult& probe,
    DesktopActionPrepared completion);

DesktopAction prepare_module_desktop_action(
    const FilesystemDescriptor& descriptor,
    const ProbeResult& probe);

/* A fixed-purpose FAT variant action; GTK never calls installer primitives. */
void execute_classic_fat_variant_async(
    bool fat12, bool remove, DesktopActionCompletion completion);

/* Separate EXT2.so action; this does not change native ext2.ko. */
void execute_ext2_dropin_async(
    bool remove, DesktopActionCompletion completion);

void execute_desktop_action_async(
    DesktopAction action,
    DesktopActionCompletion completion);

} // namespace filesystems
