/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Mode metrics: how the device is configured to schedule work.
 */

#pragma once

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

[[nodiscard]] std::span<const QueryMetric> getModeMetrics() noexcept;

} // namespace metrics::mode
