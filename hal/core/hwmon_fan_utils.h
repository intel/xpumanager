/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _HWMON_FAN_UTILS_H
#define _HWMON_FAN_UTILS_H

#include <cstdint>
#include <os.h>
#include <string>
#include <zes_api.h>

// -----------------------------------------------------------------------------
// Internal, device-free helpers for the Battlemage/xe sysfs hwmon fan RPM path.
//
// These live in their own namespace (not the public `fan` class) so they carry
// no device/driver state and can be unit-tested directly against a temp file and
// against synthetic enumeration results. The `fan` class keeps the real,
// stateful getters (getSpeedRpm / getSpeedRpmById / getAllSpeedsRpm) and calls
// into these helpers.
//
// OS-specific sysfs traversal (walking the PCI device's hwmon nodes to enumerate
// fanN_input paths) does NOT live here: it moved to the OS Abstraction Layer
// (oal getHwmonInputPaths, oal/lin/hwmon_fan.cpp) so hal stays portable. This
// header keeps only the OS-agnostic decision/parse/read-a-resolved-path logic.
// -----------------------------------------------------------------------------

namespace xpum {
namespace hwmon {

// Where a fan RPM reading must be sourced from, decided purely from the fan
// enumeration outcome. Keeping this a pure decision lets the selection contract
// be unit-tested without a Level Zero device.
enum class FanRpmSource {
	LevelZero,				  // a Level Zero fan handle exists; its reading is authoritative
	Sysfs,					  // enumeration reported zero handles; use the hwmon fallback
	PropagateEnumerationError // enumeration itself failed; surface that error, never sysfs
};

// Decide the fan RPM source from the enumeration result and handle count:
//   * enumerationResult == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
//       or ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS          -> Sysfs
//   * any other enumerationResult != ZE_RESULT_SUCCESS       -> PropagateEnumerationError
//   * fanCount == 0                                          -> Sysfs
//   * otherwise (fanCount >= 1)                              -> LevelZero
//
// Invariant: when Level Zero exposes no fan handle OR denies access to an
// unprivileged process, read the world-readable PCI hwmon node; genuine
// enumeration/runtime errors are still propagated. A non-zero fan count never
// routes to Sysfs -- the inconsistent "fanCount > 0 but the handle array is
// null" state still returns LevelZero here; the caller (fan::getAllSpeedsRpm)
// guards the null array and returns an internal error rather than silently
// falling through to sysfs.
FanRpmSource decideFanRpmSource(ze_result_t enumerationResult, uint32_t fanCount);

// Parse the textual contents of a hwmon fanN_input node into an RPM value.
// A fanN_input node reports a single non-negative base-10 integer RPM where 0
// means the fan is stopped -- a valid, operationally important reading, NOT an
// error. Returns ZE_RESULT_SUCCESS with rpm >= 0 for well-formed content, or
// ZE_RESULT_ERROR_UNSUPPORTED_FEATURE for empty / non-numeric / negative /
// out-of-range / extra-token content (a malformed read is never coerced to 0).
ze_result_t parseFanInputRpm(const std::string &content, int32_t &rpm);

// Read a resolved hwmon fanN_input path (supplied by the oal getHwmonInputPaths
// walk) and parse it with parseFanInputRpm. Portable ifstream + std::filesystem,
// device/driver-free, so the sysfs read path is unit-testable against a temp
// file. The traversal that discovers these paths lives in oal; this reader is
// deliberately path-in, RPM-out. Returns:
//   * ZE_RESULT_ERROR_INVALID_NULL_POINTER   if rpm == nullptr (a caller bug)
//   * ZE_RESULT_ERROR_UNSUPPORTED_FEATURE    empty path / missing node / malformed
//   * ZE_RESULT_SUCCESS with rpm >= 0        on a well-formed reading
// rpm is left unchanged on failure.
ze_result_t readFanRpmFromPath(const std::string &path, int32_t *rpm);

} // namespace hwmon
} // namespace xpum

#endif
