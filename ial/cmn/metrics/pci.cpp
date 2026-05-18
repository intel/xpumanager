/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * PCI metrics: link speed/width (max and negotiated), Rx/Tx throughput, and replay counter.
 * NOTE: bytes/µs == MB/s because zes_pci_stats_t timestamps are in microseconds.
 */

#include "pci.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include "zes_api.h"
#include <cstdint>
#include <pci.h>
#include <array>
#include <format>
#include <span>
#include <string>

namespace metrics::pci {

namespace {

constexpr auto link_gen_max =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.link.gen.max",
				.unit = "",
				.description = "Maximum PCIe generation supported",
				.source = MetricSource::Static,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					auto *p = d.dev->getPCI();
					if (p == nullptr) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					zes_pci_properties_t props{};
					auto const r = p->getProperties(d.zesDeviceHdl, &props);
					if (r == ZE_RESULT_SUCCESS) {
						out = std::format("{}", props.maxSpeed.gen);
					}
					return r;
				}};

constexpr auto link_width_max =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.link.width.max",
				.unit = "",
				.description = "Maximum PCIe link width supported",
				.source = MetricSource::Static,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					auto *p = d.dev->getPCI();
					if (p == nullptr) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					zes_pci_properties_t props{};
					auto const r = p->getProperties(d.zesDeviceHdl, &props);
					if (r == ZE_RESULT_SUCCESS) {
						out = std::format("{}", props.maxSpeed.width);
					}
					return r;
				}};

constexpr auto link_gen_current =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.link.gen.current",
				.unit = "",
				.description = "Current PCIe generation (negotiated)",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					auto *p = d.dev->getPCI();
					if (p == nullptr) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					zes_pci_speed_t speed{};
					auto const r = p->getCurrentLinkSpeed(d.zesDeviceHdl, speed);
					if (r == ZE_RESULT_SUCCESS) {
						out = std::format("{}", speed.gen);
					}
					return r;
				}};

constexpr auto link_width_current =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.link.width.current",
				.unit = "",
				.description = "Current PCIe link width (negotiated)",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					auto *p = d.dev->getPCI();
					if (p == nullptr) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					zes_pci_speed_t speed{};
					auto const r = p->getCurrentLinkSpeed(d.zesDeviceHdl, speed);
					if (r == ZE_RESULT_SUCCESS) {
						out = std::format("{}", speed.width);
					}
					return r;
				}};

constexpr auto tx_throughput =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.tx.throughput",
				.unit = "MB/s",
				.description = "PCIe transmit throughput",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (!cache.pcieAvail || !cache.pcieBandwidthAvail) {
						return ZE_RESULT_NOT_READY;
					}
					const auto dt = static_cast<double>(cache.pcieAfter.timeUs - cache.pcieBefore.timeUs);
					const auto tx = static_cast<double>(cache.pcieAfter.tx - cache.pcieBefore.tx);
					out = std::format("{}", tx / dt);
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto rx_throughput =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.rx.throughput",
				.unit = "MB/s",
				.description = "PCIe receive throughput",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (!cache.pcieAvail || !cache.pcieBandwidthAvail) {
						return ZE_RESULT_NOT_READY;
					}
					const auto dt = static_cast<double>(cache.pcieAfter.timeUs - cache.pcieBefore.timeUs);
					const auto rx = static_cast<double>(cache.pcieAfter.rx - cache.pcieBefore.rx);
					out = std::format("{}", rx / dt);
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto replay_counter =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.replay.counter",
				.unit = "",
				.description = "PCIe replay error count",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (!cache.pcieAvail || !cache.pcieReplayAvail) {
						return ZE_RESULT_NOT_READY;
					}
					out = std::format("{}", cache.pcieReplay);
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto rx_throughput_kbs =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.rx.throughput.kbs",
				.unit = "kB/s",
				.description = "PCIe receive throughput (kB/s)",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (!cache.pcieAvail || !cache.pcieBandwidthAvail) {
						return ZE_RESULT_NOT_READY;
					}
					const uint64_t dt = cache.pcieAfter.timeUs - cache.pcieBefore.timeUs;
					const uint64_t rx = cache.pcieAfter.rx - cache.pcieBefore.rx;
					out = std::format("{}", 1000000ULL * rx / dt / 1000ULL);
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto tx_throughput_kbs =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.tx.throughput.kbs",
				.unit = "kB/s",
				.description = "PCIe transmit throughput (kB/s)",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (!cache.pcieAvail || !cache.pcieBandwidthAvail) {
						return ZE_RESULT_NOT_READY;
					}
					const uint64_t dt = cache.pcieAfter.timeUs - cache.pcieBefore.timeUs;
					const uint64_t tx = cache.pcieAfter.tx - cache.pcieBefore.tx;
					out = std::format("{}", 1000000ULL * tx / dt / 1000ULL);
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto ALL = std::to_array<QueryMetric>({
	link_gen_max,
	link_width_max,
	link_gen_current,
	link_width_current,
	tx_throughput,
	rx_throughput,
	replay_counter,
	rx_throughput_kbs,
	tx_throughput_kbs,
});

} // namespace

std::span<const QueryMetric> getPciMetrics() noexcept { return ALL; }

} // namespace metrics::pci
