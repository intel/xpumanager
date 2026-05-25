/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * ECC / RAS metrics: ECC mode, aggregate error counts, and per-category RAS counters.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::ecc {

[[nodiscard]] std::span<const QueryMetric> getEccMetrics() noexcept;

} // namespace metrics::ecc
