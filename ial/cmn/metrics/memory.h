/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Memory metrics: total/used/free capacity, read/write bandwidth and the number of
 * pages taken offline.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::memory {

[[nodiscard]] std::span<const QueryMetric> getMemoryMetrics() noexcept;

} // namespace metrics::memory
