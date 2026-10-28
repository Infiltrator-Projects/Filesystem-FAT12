// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal_posix.h
 * @brief POSIX persistence and provider-discovery adapter for temporal policy.
 *
 * Temporal schema/validation stays in temporal.h. This adapter owns the
 * canonical XDG persistence location and the installed-provider capability
 * marker used by POSIX desktop consumers.
 */
#ifndef INFILTRATR_COMMON_TEMPORAL_POSIX_H
#define INFILTRATR_COMMON_TEMPORAL_POSIX_H

#include "infiltratr/posix.h"
#include "infiltratr/temporal.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef INFILTRATR_TEMPORAL_PROVIDER_MARKER
#define INFILTRATR_TEMPORAL_PROVIDER_MARKER \
    "/usr/share/infiltrator/policy-providers/temporal-v3"
#endif

/** Resolve the per-user directory containing presentation.conf. */
bool infiltratr_temporal_posix_policy_directory(char *destination,
                                                size_t size);

/** Resolve the canonical per-user temporal policy document path. */
bool infiltratr_temporal_posix_policy_path(char *destination, size_t size);

/**
 * Resolve the installed temporal-provider capability marker path.
 *
 * The compiled default may be overridden by the absolute
 * INFILTRATR_TEMPORAL_PROVIDER_MARKER_PATH environment value. This is intended
 * for staged/custom-prefix installations and deterministic qualification.
 */
bool infiltratr_temporal_posix_provider_marker_path(char *destination,
                                                    size_t size);

/**
 * True only when the provider marker exists and declares the exact supported
 * provider, policy version and temporal contract.
 */
bool infiltratr_temporal_posix_provider_available(void);

/**
 * Load and validate the current user's temporal policy.
 *
 * Missing policy is a successful no-policy state: @found is false and @policy
 * receives v3 defaults. Malformed or unreadable policy returns a rich I/O
 * result and leaves @found false.
 */
InfiltratrIoResult infiltratr_temporal_posix_policy_load(
    InfiltratrTemporalPolicyV3 *policy,
    bool *found);

/**
 * Atomically persist a validated v3 policy in the canonical XDG location.
 *
 * Returns zero on success or an errno-style failure code.
 */
int infiltratr_temporal_posix_policy_save(
    const InfiltratrTemporalPolicyV3 *policy);

#ifdef __cplusplus
}
#endif

#endif
