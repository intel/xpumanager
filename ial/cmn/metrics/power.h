/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Power metrics: instantaneous draw (card and GPU domain), accumulated energy, and power limits.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::power {

[[nodiscard]] std::span<const QueryMetric> getPowerMetrics() noexcept;

} // namespace metrics::power
