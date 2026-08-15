/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _HWMON_TEMPERATURE_UTILS_H
#define _HWMON_TEMPERATURE_UTILS_H

#include <zes_api.h>
#include <string>

// Internal (non-exported) helpers backing the sysfs hwmon temperature fallback
// used on the single-tile, single-fan Intel Arc Pro B70 (xe driver). They are
// deliberately kept OUT of the exported `temperature` class / DLL surface: they
// are pure functions shared by temperature.cpp and the unit-test target, not
// part of the public HAL API. The test links libxpum statically and calls them
// through this header, so the exported class does not grow to make them testable.
namespace xpum
{
namespace hwmon
{

// Physical-validity bounds for a hwmon temperature reading, in Celsius. The
// lower bound is absolute zero: the fallback is condition-based (it is NOT gated
// on a specific PCI id), so a legitimately sub-zero reading on another matching
// system must not be discarded -- only physically impossible values are. The
// upper bound mirrors MAX_REASONABLE_TEMP_CELSIUS (150 C) from temperature.h.
constexpr double kMinimumPlausibleTemperatureC = -273.15;
constexpr double kMaximumReasonableTemperatureC = 150.0;

// How a temperature should be sourced after a Level Zero getState() attempt.
enum class TempSource
{
	UseLevelZero,  // Level Zero returned a value; use it and never fall back.
	UseSysfs,      // Level Zero has no matching sensor OR denied an unprivileged
	               // process; read the world-readable sysfs node.
	PropagateError // A genuine Level Zero error (device loss, bad state, unknown).
};

// Central fallback contract. Fall back to the world-readable PCI hwmon node when
// Level Zero reports the sensor type as unsupported (it never enumerated a
// matching sensor) OR denies access to an unprivileged process (insufficient
// permissions); in both cases sysfs is the correct source. SUCCESS keeps the
// Level Zero value; genuine device/runtime errors stay authoritative and
// propagate so a device-loss is never masked by a stale sysfs value. Mapping:
// SUCCESS -> UseLevelZero; UNSUPPORTED_FEATURE / INSUFFICIENT_PERMISSIONS ->
// UseSysfs; anything else -> PropagateError.
TempSource decideTempSource(ze_result_t l0Result);

// Per-tile fallback predicate. Fall back only when no matching-type sensor was
// enumerated; equivalent to !matchingSensorFound, named for intent at the call
// site. A matched sensor whose state read failed leaves the tile map empty
// WITHOUT meaning the type is unsupported, so emptiness alone must not trigger
// the fallback.
bool perTileShouldFallback(bool matchingSensorFound);

// Read a hwmon tempN_input file (integer milli-Celsius), convert to Celsius and
// validate against [kMinimumPlausibleTemperatureC, kMaximumReasonableTemperatureC).
// Writes *celsius ONLY after a successful parse + bounds check. Returns
// ZE_RESULT_SUCCESS on success, ZE_RESULT_ERROR_INVALID_NULL_POINTER for a null
// out pointer, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the file does not open,
// does not parse, or is out of range.
//
// NOTE: the hwmon sysfs traversal that discovers the label -> tempN_input paths
// is platform-specific and now lives in the OS Abstraction Layer
// (oal::getHwmonLabelPaths, oal/lin/hwmon.cpp). Only the portable read / bounds
// / decision helpers remain here.
ze_result_t readTempInputFile(const std::string &inputPath, double *celsius);

} // namespace hwmon
} // namespace xpum

#endif
