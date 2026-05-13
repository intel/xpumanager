/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Utilization metrics: engine groups (ALL/compute/render/media/copy) and memory utilization %.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::utilization {

[[nodiscard]] std::span<const QueryMetric> getUtilizationMetrics() noexcept;

} // namespace metrics::utilization
