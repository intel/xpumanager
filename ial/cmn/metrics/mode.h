/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Mode metrics: how the device is configured to schedule work, to enter standby, and the
 * health status the driver assigns to it.
 */

#pragma once

#include "device.h"
#include "metrics_registry.h"
#include "zes_api.h"
#include <span>
#include <string_view>

namespace metrics::mode {

/**
 * @brief Name a scheduler mode for display, in the words the config command uses.
 *
 * @param[in] schedulerMode The mode the driver reported.
 *
 * @return "timeout", "timeslice" or "exclusive"; "unknown" for any other mode, including the deprecated one.
 */
[[nodiscard]] constexpr std::string_view schedulerModeName(zes_sched_mode_t schedulerMode) noexcept
{
	switch (schedulerMode) {
	case ZES_SCHED_MODE_TIMEOUT:
		return "timeout";
	case ZES_SCHED_MODE_TIMESLICE:
		return "timeslice";
	case ZES_SCHED_MODE_EXCLUSIVE:
		return "exclusive";
	default:
		return "unknown";
	}
}

/**
 * @brief Name a standby promotion mode for display, in the words the config command uses.
 *
 * @param[in] promotion The mode the driver reported.
 *
 * @return "default" or "never"; "unknown" for any other mode.
 */
[[nodiscard]] constexpr std::string_view standbyModeName(zes_standby_promo_mode_t promotion) noexcept
{
	switch (promotion) {
	case ZES_STANDBY_PROMO_MODE_DEFAULT:
		return "default";
	case ZES_STANDBY_PROMO_MODE_NEVER:
		return "never";
	default:
		return "unknown";
	}
}

/**
 * @brief Name a device health status for display, in the words the stats command uses.
 *
 * @param[in] status The status the driver reported.
 *
 * @return "OK", "Warning", "Critical" or "Failed"; "Unknown" for any other status.
 */
[[nodiscard]] constexpr std::string_view healthStatusName(zes_device_health_status_ext_t status) noexcept
{
	switch (status) {
	case ZES_DEVICE_HEALTH_STATUS_EXT_OK:
		return "OK";
	case ZES_DEVICE_HEALTH_STATUS_EXT_WARNING:
		return "Warning";
	case ZES_DEVICE_HEALTH_STATUS_EXT_CRITICAL:
		return "Critical";
	case ZES_DEVICE_HEALTH_STATUS_EXT_FAILED:
		return "Failed";
	default:
		return "Unknown";
	}
}

[[nodiscard]] std::span<const QueryMetric> getModeMetrics() noexcept;

} // namespace metrics::mode
