/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Memory metrics: total/used/free capacity and read/write bandwidth.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::memory {

[[nodiscard]] std::span<const QueryMetric> getMemoryMetrics() noexcept;

} // namespace metrics::memory
