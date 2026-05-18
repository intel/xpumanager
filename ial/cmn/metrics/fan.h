/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Fan metrics: speed as a percentage of maximum.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::fan {

[[nodiscard]] std::span<const QueryMetric> getFanMetrics() noexcept;

} // namespace metrics::fan
