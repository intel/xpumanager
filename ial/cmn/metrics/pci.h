/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * PCI metrics: link speed/width (max and negotiated), Rx/Tx throughput, and replay counter.
 * NOTE: bytes/µs == MB/s because zes_pci_stats_t timestamps are in microseconds.
 */

#pragma once

#include "metrics_registry.h"
#include <span>

namespace metrics::pci {

[[nodiscard]] std::span<const QueryMetric> getPciMetrics() noexcept;

} // namespace metrics::pci
