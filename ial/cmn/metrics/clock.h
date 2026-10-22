/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Clock metrics: current GPU/media frequencies, throttle reason, and
 * maximum hardware clock frequencies. Aliases for common field names
 * (clocks.current.sm, clocks.current.video, clocks.max.sm) are attached
 * to their canonical counterparts and invisible to group expansion but
 * resolvable by name.
 */

#pragma once

#include "metrics_registry.h"
#include <span>
#include <string>

namespace metrics::clock {

/**
 * @brief Describe a list of supported clocks in one line without commas.
 *
 * Clocks spaced at an even step, which is how the driver lists them, are summarised as
 * "low-high step N" so the line stays short however many clocks there are. Any other list
 * is written out, separated by "; ".
 *
 * @param[in] clocks Supported clocks in MHz, in the order the driver reported them.
 *
 * @return The description; empty when @p clocks is empty.
 */
[[nodiscard]] std::string describeClockList(std::span<const double> clocks);

[[nodiscard]] std::span<const QueryMetric> getClockMetrics() noexcept;

} // namespace metrics::clock
