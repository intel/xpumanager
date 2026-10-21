/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * PCI metrics: link speed/width (max and negotiated), Rx/Tx throughput, and replay counter.
 * NOTE: bytes/µs == MB/s because zes_pci_stats_t timestamps are in microseconds.
 */

#pragma once

#include "metrics_registry.h"
#include "zes_api.h"
#include <span>
#include <string_view>

namespace metrics::pci {

/**
 * @brief Name a PCIe link status for display.
 *
 * @param[in] status The status the driver reported.
 *
 * @return "good", "quality issues" or "stability issues"; "unknown" for any other status.
 */
[[nodiscard]] constexpr std::string_view linkStatusName(zes_pci_link_status_t status) noexcept
{
	switch (status) {
	case ZES_PCI_LINK_STATUS_GOOD:
		return "good";
	case ZES_PCI_LINK_STATUS_QUALITY_ISSUES:
		return "quality issues";
	case ZES_PCI_LINK_STATUS_STABILITY_ISSUES:
		return "stability issues";
	default:
		return "unknown";
	}
}

[[nodiscard]] std::span<const QueryMetric> getPciMetrics() noexcept;

} // namespace metrics::pci
