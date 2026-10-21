/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Fan metrics: speed as a percentage of maximum.
 */

#pragma once

#include "metrics_registry.h"
#include "zes_api.h"
#include <span>
#include <string_view>

namespace metrics::fan {

/**
 * @brief Name a fan speed mode for display.
 *
 * @param[in] fanMode The mode the driver reported.
 *
 * @return "default", "fixed" or "table"; "unknown" for any other mode.
 */
[[nodiscard]] constexpr std::string_view fanModeName(zes_fan_speed_mode_t fanMode) noexcept
{
	switch (fanMode) {
	case ZES_FAN_SPEED_MODE_DEFAULT:
		return "default";
	case ZES_FAN_SPEED_MODE_FIXED:
		return "fixed";
	case ZES_FAN_SPEED_MODE_TABLE:
		return "table";
	default:
		return "unknown";
	}
}

[[nodiscard]] std::span<const QueryMetric> getFanMetrics() noexcept;

} // namespace metrics::fan
