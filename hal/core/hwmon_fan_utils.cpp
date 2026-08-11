/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "hwmon_fan_utils.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace xpum {
namespace hwmon {

FanRpmSource decideFanRpmSource(ze_result_t enumerationResult, uint32_t fanCount)
{
	if (enumerationResult != ZE_RESULT_SUCCESS) {
		// When Level Zero exposes no fan handle or denies access to an unprivileged
		// process, read the world-readable PCI hwmon node instead; genuine
		// enumeration/runtime errors are still propagated.
		if (enumerationResult == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE ||
			enumerationResult == ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS) {
			return FanRpmSource::Sysfs;
		}
		return FanRpmSource::PropagateEnumerationError; // never silently sysfs on a genuine enum failure
	}
	if (fanCount == 0) {
		return FanRpmSource::Sysfs; // Battlemage/xe: no Level Zero fan handle
	}
	return FanRpmSource::LevelZero; // a fan handle exists and is authoritative
}

ze_result_t parseFanInputRpm(const std::string &content, int32_t &rpm)
{
	std::istringstream iss(content);
	long value = 0;
	if (!(iss >> value)) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; // empty or non-numeric
	}
	std::string trailing;
	if (iss >> trailing) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; // extra tokens => malformed
	}
	if (value < 0 || value > static_cast<long>(INT32_MAX)) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; // negative or absurd RPM
	}
	rpm = static_cast<int32_t>(value); // 0 preserved: a stopped fan is a valid reading
	return ZE_RESULT_SUCCESS;
}

ze_result_t readFanRpmFromPath(const std::string &path, int32_t *rpm)
{
	if (rpm == nullptr) {
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER; // a null out-param is a caller bug, not "unsupported"
	}
	if (path.empty()) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; // no resolved hwmon fanN_input path
	}
	// The path is already resolved by the oal getHwmonInputPaths walk; construct a
	// std::filesystem::path so the same reader works against a temp file in unit
	// tests on any platform. This is a portable read, not an OS-specific traversal.
	const std::filesystem::path inputPath{path};
	std::ifstream vf(inputPath);
	if (!vf.is_open()) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE; // missing node
	}
	std::string content;
	std::getline(vf, content);
	return parseFanInputRpm(content, *rpm);
}

} // namespace hwmon
} // namespace xpum
