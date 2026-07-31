/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "hwmon_temperature_utils.h"

#include <fstream>
#include <string>

namespace xpum
{
namespace hwmon
{

/**
 * @brief Decide how to source a temperature after a Level Zero getState() call.
 *
 * Fall back to the world-readable PCI hwmon node when Level Zero reports the
 * sensor type as unsupported (it never enumerated a matching sensor) OR denies
 * access to an unprivileged process (insufficient permissions): in both cases
 * the sysfs node is the correct source. Genuine device/runtime errors (device
 * loss, bad state, unknown) still propagate, so a device-loss is never masked
 * by a stale sysfs value.
 */
TempSource decideTempSource(ze_result_t l0Result)
{
	if (l0Result == ZE_RESULT_SUCCESS) {
		return TempSource::UseLevelZero;
	}
	if (l0Result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE ||
		l0Result == ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS) {
		return TempSource::UseSysfs;
	}
	return TempSource::PropagateError;
}

/**
 * @brief Per-tile fallback predicate.
 *
 * Fall back only when no matching-type sensor was enumerated. A matched sensor
 * whose state read failed leaves the tile map empty WITHOUT meaning the type is
 * unsupported, so emptiness alone must not trigger the fallback.
 */
bool perTileShouldFallback(bool matchingSensorFound) { return !matchingSensorFound; }

/**
 * @brief Read + validate a hwmon tempN_input file (shared read path).
 *
 * Every caller goes through this one function so validation is identical
 * everywhere. Steps: open, parse an integer milli-Celsius value, convert to
 * Celsius, bounds-check against [kMinimumPlausibleTemperatureC,
 * kMaximumReasonableTemperatureC), and assign *celsius ONLY after all checks
 * pass. The lower bound is absolute zero (a physical-validity floor), not a
 * device-specific 0 C floor: the fallback is condition-based, so a legitimately
 * sub-zero reading on another matching system must be accepted.
 *
 * @param inputPath Absolute path to a hwmon tempN_input file.
 * @param celsius Out-param receiving the validated Celsius value.
 * @return ZE_RESULT_SUCCESS on success; ZE_RESULT_ERROR_INVALID_NULL_POINTER if
 *         `celsius` is null; ZE_RESULT_ERROR_UNSUPPORTED_FEATURE if the file
 *         does not open, does not parse, or is out of range.
 */
ze_result_t readTempInputFile(const std::string &inputPath, double *celsius)
{
	if (celsius == nullptr) {
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	std::ifstream f(inputPath);
	if (!f.is_open()) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	long long milli = 0;
	if (!(f >> milli)) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; // malformed / non-numeric
	}
	const double value = (double)milli / 1000.0;
	if (value < kMinimumPlausibleTemperatureC || value >= kMaximumReasonableTemperatureC) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; // physically implausible
	}
	*celsius = value;
	return ZE_RESULT_SUCCESS;
}

} // namespace hwmon
} // namespace xpum
