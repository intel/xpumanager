/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Clock metrics: current GPU/media frequencies, throttle reason, and
 * maximum hardware clock frequencies. Aliases for nvidia-smi-compatible
 * field names (clocks.current.sm, clocks.current.video, clocks.max.sm)
 * are attached to their canonical counterparts and invisible to group
 * expansion but resolvable by name.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::clock {

[[nodiscard]] std::span<const QueryMetric> getClockMetrics() noexcept;

} // namespace metrics::clock
