/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Temperature metrics: GPU core and memory temperatures.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::temperature {

[[nodiscard]] std::span<const QueryMetric> getTemperatureMetrics() noexcept;

} // namespace metrics::temperature
