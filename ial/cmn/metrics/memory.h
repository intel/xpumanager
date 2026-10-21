/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Memory metrics: total/used/free capacity, read/write bandwidth, memory health and
 * the number of retired pages.
 */

#pragma once

#include "metrics_registry.h"
#include "zes_api.h"
#include <span>
#include <string_view>

namespace metrics::memory {

/**
 * @brief Name a memory health state for display.
 *
 * @param[in] health The state the driver reported.
 *
 * @return "OK", "Degraded", "Critical" or "Replace"; "Unknown" for an unknown state or one this build does not name.
 */
[[nodiscard]] constexpr std::string_view memoryHealthName(zes_mem_health_t health) noexcept
{
	switch (health) {
	case ZES_MEM_HEALTH_OK:
		return "OK";
	case ZES_MEM_HEALTH_DEGRADED:
		return "Degraded";
	case ZES_MEM_HEALTH_CRITICAL:
		return "Critical";
	case ZES_MEM_HEALTH_REPLACE:
		return "Replace";
	case ZES_MEM_HEALTH_UNKNOWN:
	default:
		return "Unknown";
	}
}

[[nodiscard]] std::span<const QueryMetric> getMemoryMetrics() noexcept;

} // namespace metrics::memory
