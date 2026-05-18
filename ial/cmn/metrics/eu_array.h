/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * EU Array metrics: Xe EU active/stall/idle fractions.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::eu_array {

[[nodiscard]] std::span<const QueryMetric> getEuArrayMetrics() noexcept;

} // namespace metrics::eu_array
