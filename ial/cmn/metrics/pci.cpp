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
#include "pci_class.h"
#include "pci_link.h"
#include "ze_api.h"
#include "zes_api.h"
#include <cstdint>
#include <pci.h>
#include <array>
#include "utility/compat/format.h"
#include <span>
#include <optional>
#include <string>

namespace metrics::pci {

namespace {

/**
 * @brief Reads one PCIe byte counter's throughput over the sampling window.
 *
 * A driver can leave both the counters and their timestamp unchanged while the link is idle;
 * that reads as zero rather than as no value. Counters that moved under an unchanged
 * timestamp give no rate.
 *
 * @tparam COUNTER   The byte counter, PcieSnapshot::tx or PcieSnapshot::rx.
 * @tparam PER_SECOND Bytes per second divided by this is the unit: 1000000 for MB/s, 1000 for kB/s.
 *
 * @retval ZE_RESULT_SUCCESS   @p out holds the throughput, MB/s as a decimal, kB/s as a whole number.
 * @retval ZE_RESULT_NOT_READY The counters were not read at both ends of the window, moved under
 *                             an unchanged timestamp, or went backwards (e.g. reset).
 */
template <uint64_t PcieSnapshot::*COUNTER, uint64_t PER_SECOND>
ze_result_t throughputGetter(devInfo & /*d*/, MetricValue &out, const MetricCache &cache)
{
	if (!cache.pcieAvail || !cache.pcieBandwidthAvail) {
		return ZE_RESULT_NOT_READY;
	}
	// Unsigned differences of values that went backwards would read as enormous rates.
	if (cache.pcieAfter.timeUs < cache.pcieBefore.timeUs || cache.pcieAfter.*COUNTER < cache.pcieBefore.*COUNTER) {
		return ZE_RESULT_NOT_READY;
	}
	const uint64_t dt = cache.pcieAfter.timeUs - cache.pcieBefore.timeUs;
	const uint64_t bytes = cache.pcieAfter.*COUNTER - cache.pcieBefore.*COUNTER;
	if (dt == 0) {
		if (bytes != 0) {
			return ZE_RESULT_NOT_READY;
		}
		out = "0";
		return ZE_RESULT_SUCCESS;
	}
	if constexpr (PER_SECOND == 1000000ULL) {
		out = xpum::compat::format("{}", static_cast<double>(bytes) / static_cast<double>(dt));
	} else {
		// Bytes per second without forming bytes * 10^6, which overflows past ~18 TB.
		constexpr uint64_t usPerSecond = 1000000ULL;
		const uint64_t perSecond = bytes / dt * usPerSecond + bytes % dt * usPerSecond / dt;
		out = xpum::compat::format("{}", perSecond / PER_SECOND);
	}
	return ZE_RESULT_SUCCESS;
}

constexpr auto LINK_GEN_MAX =
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
						out = xpum::compat::format("{}", props.maxSpeed.gen);
					}
					return r;
				}};

constexpr auto LINK_WIDTH_MAX =
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
						out = xpum::compat::format("{}", props.maxSpeed.width);
					}
					return r;
				}};

constexpr auto LINK_GEN_CURRENT =
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
						out = xpum::compat::format("{}", speed.gen);
					}
					return r;
				}};

constexpr auto LINK_WIDTH_CURRENT =
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
						out = xpum::compat::format("{}", speed.width);
					}
					return r;
				}};

constexpr auto TX_THROUGHPUT = QueryMetric{// NOLINT(readability-identifier-naming)
										   .name = "pcie.tx.throughput",
										   .unit = "MB/s",
										   .description = "PCIe transmit throughput",
										   .source = MetricSource::Live,
										   .groups = MetricGroup::PCI,
										   .inputs = MetricInput::PCIE,
										   .getter = throughputGetter<&PcieSnapshot::tx, 1000000ULL>};

constexpr auto RX_THROUGHPUT = QueryMetric{// NOLINT(readability-identifier-naming)
										   .name = "pcie.rx.throughput",
										   .unit = "MB/s",
										   .description = "PCIe receive throughput",
										   .source = MetricSource::Live,
										   .groups = MetricGroup::PCI,
										   .inputs = MetricInput::PCIE,
										   .getter = throughputGetter<&PcieSnapshot::rx, 1000000ULL>};

constexpr auto REPLAY_COUNTER =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.replay.counter",
				.unit = "",
				.description = "PCIe replay error count",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.inputs = MetricInput::PCIE,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (!cache.pcieAvail) {
						return ZE_RESULT_NOT_READY;
					}
					// The driver says the device has no replay counter.
					if (!cache.pcieReplayAvail) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					out = xpum::compat::format("{}", cache.pcieReplay);
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto RX_THROUGHPUT_KBS = QueryMetric{// NOLINT(readability-identifier-naming)
											   .name = "pcie.rx.throughput.kbs",
											   .unit = "kB/s",
											   .description = "PCIe receive throughput (kB/s)",
											   .source = MetricSource::Live,
											   .groups = MetricGroup::PCI,
											   .inputs = MetricInput::PCIE,
											   .getter = throughputGetter<&PcieSnapshot::rx, 1000ULL>};

constexpr auto TX_THROUGHPUT_KBS = QueryMetric{// NOLINT(readability-identifier-naming)
											   .name = "pcie.tx.throughput.kbs",
											   .unit = "kB/s",
											   .description = "PCIe transmit throughput (kB/s)",
											   .source = MetricSource::Live,
											   .groups = MetricGroup::PCI,
											   .inputs = MetricInput::PCIE,
											   .getter = throughputGetter<&PcieSnapshot::tx, 1000ULL>};

/**
 * @brief Reads one byte of the device's PCI class code.
 *
 * @tparam BYTE PciClassCode::baseClass or PciClassCode::subClass.
 *
 * @retval ZE_RESULT_SUCCESS                   @p out holds the byte as "0x" and two hex digits.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No PCI HAL, or the OS does not report the class code.
 */
template <uint8_t PciClassCode::*BYTE> ze_result_t classCodeGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	auto *p = d.dev->getPCI();
	if (p == nullptr) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	const auto code = getPciClassCode(p->getBDFStr());
	if (!code) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	out = xpum::compat::format("0x{:02X}", (*code).*BYTE);
	return ZE_RESULT_SUCCESS;
}

constexpr auto BASE_CLASS = QueryMetric{// NOLINT(readability-identifier-naming)
										.name = "pci.base_class",
										.unit = "",
										.description = "PCI base class code, in hex: 0x03 display controller, "
													   "0x12 processing accelerator",
										.source = MetricSource::Static,
										.groups = MetricGroup::PCI,
										.getter = classCodeGetter<&PciClassCode::baseClass>};

constexpr auto SUB_CLASS = QueryMetric{// NOLINT(readability-identifier-naming)
									   .name = "pci.sub_class",
									   .unit = "",
									   .description = "PCI subclass code, in hex",
									   .source = MetricSource::Static,
									   .groups = MetricGroup::PCI,
									   .getter = classCodeGetter<&PciClassCode::subClass>};

/// The BDF of the device, or nothing without a PCI HAL.
[[nodiscard]] std::optional<std::string> bdfOf(devInfo &d)
{
	auto *p = d.dev->getPCI();
	return p == nullptr ? std::nullopt : std::optional<std::string>{p->getBDFStr()};
}

/**
 * @brief Reads one part of the root port's link capability.
 *
 * @tparam PART PcieLinkMax::generation or PcieLinkMax::width.
 *
 * @retval ZE_RESULT_SUCCESS                   @p out holds the value.
 * @retval ZE_RESULT_ERROR_NOT_AVAILABLE       No PCI HAL, no root port above the device, or the OS
 *                                             does not report it (Windows).
 */
template <std::optional<int> PcieLinkMax::*PART>
ze_result_t hostMaxGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	const auto bdf = bdfOf(d);
	const auto value = bdf ? getRootPortMaxLink(*bdf).*PART : std::nullopt;
	if (!value) {
		return ZE_RESULT_ERROR_NOT_AVAILABLE;
	}
	out = xpum::compat::format("{}", *value);
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Reads one AER error counter of the device.
 *
 * @tparam COUNT The counter, e.g. PcieAerCounts::correctable.
 *
 * @retval ZE_RESULT_SUCCESS                   @p out holds the count.
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE No PCI HAL, the device has no AER counters, or the
 *                                             OS does not report them (Windows).
 */
template <uint64_t PcieAerCounts::*COUNT> ze_result_t aerGetter(devInfo &d, MetricValue &out, const MetricCache &)
{
	const auto bdf = bdfOf(d);
	const auto counts = bdf ? getPcieAerCounts(*bdf) : std::nullopt;
	if (!counts) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	out = xpum::compat::format("{}", (*counts).*COUNT);
	return ZE_RESULT_SUCCESS;
}

/// The AtomicOp support of the device, or nothing when its configuration space cannot be read.
[[nodiscard]] std::optional<PcieAtomicOps> atomicOpsOf(devInfo &d)
{
	const auto bdf = bdfOf(d);
	return bdf ? getPcieAtomicOps(*bdf) : std::nullopt;
}

constexpr auto LINK_GEN_HOST_MAX =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.link.gen.host_max",
				.unit = "",
				.description = "Maximum PCIe generation of the root port the device is under (Linux); a switch in "
							   "between can limit the link further",
				.source = MetricSource::Static,
				.groups = MetricGroup::PCI,
				.getter = hostMaxGetter<&PcieLinkMax::generation>};

constexpr auto LINK_WIDTH_HOST_MAX =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.link.width.host_max",
				.unit = "",
				.description = "Maximum PCIe link width of the root port the device is under (Linux)",
				.source = MetricSource::Static,
				.groups = MetricGroup::PCI,
				.getter = hostMaxGetter<&PcieLinkMax::width>};

constexpr auto AER_CORRECTABLE =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.errors.correctable",
				.unit = "",
				.description = "PCIe correctable errors the device reported since it was enumerated (AER, Linux)",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = aerGetter<&PcieAerCounts::correctable>};

constexpr auto AER_NON_FATAL =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.errors.non_fatal",
				.unit = "",
				.description = "PCIe uncorrectable non-fatal errors since enumeration (AER, Linux)",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = aerGetter<&PcieAerCounts::nonFatal>};

constexpr auto AER_FATAL = QueryMetric{// NOLINT(readability-identifier-naming)
									   .name = "pcie.errors.fatal",
									   .unit = "",
									   .description = "PCIe uncorrectable fatal errors since enumeration (AER, Linux)",
									   .source = MetricSource::Live,
									   .groups = MetricGroup::PCI,
									   .getter = aerGetter<&PcieAerCounts::fatal>};

constexpr auto AER_REPLAY_ROLLOVER =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.errors.replay_rollovers",
				.unit = "",
				.description = "PCIe REPLAY_NUM rollovers since enumeration, a correctable link error (AER, Linux)",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = aerGetter<&PcieAerCounts::replayRollover>};

constexpr auto AER_REPLAY_TIMEOUT =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.errors.replay_timeouts",
				.unit = "",
				.description = "PCIe replay timer timeouts since enumeration, a correctable link error (AER, Linux)",
				.source = MetricSource::Live,
				.groups = MetricGroup::PCI,
				.getter = aerGetter<&PcieAerCounts::replayTimeout>};

constexpr auto ATOMIC_CAPS =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.atomic.completer",
				.unit = "",
				.description = "PCIe AtomicOp requests the device completes (Device Capabilities 2), e.g. "
							   "\"FETCHADD_32 SWAP_32 CAS_32\"; reading it needs root on Linux",
				.source = MetricSource::Static,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					const auto ops = atomicOpsOf(d);
					if (!ops) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					std::string text;
					const auto add = [&text](std::string_view word) {
						if (!text.empty()) {
							text += ' ';
						}
						text += word;
					};
					if (ops->completer32) {
						add("FETCHADD_32 SWAP_32 CAS_32");
					}
					if (ops->completer64) {
						add("FETCHADD_64 SWAP_64 CAS_64");
					}
					if (ops->completer128Cas) {
						add("CAS_128");
					}
					out = text.empty() ? "None" : text;
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto ATOMIC_REQUESTS =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pcie.atomic.requests",
				.unit = "",
				.description = "Whether the device may send PCIe AtomicOp requests (Device Control 2): Enabled or "
							   "Disabled; reading it needs root on Linux",
				.source = MetricSource::Static,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					const auto ops = atomicOpsOf(d);
					if (!ops) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					out = ops->requesterEnabled ? "Enabled" : "Disabled";
					return ZE_RESULT_SUCCESS;
				}};

/// The device's largest memory BAR, or nothing when it has none or the driver does not say.
[[nodiscard]] std::optional<MemoryBar> memoryBarOf(devInfo &d)
{
	auto *p = d.dev->getPCI();
	return p == nullptr ? std::nullopt : p->getMemoryBar(d.zesDeviceHdl).bar;
}

constexpr auto MEMORY_BAR_SIZE =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "pci.bar.memory.size",
				.unit = "MiB",
				.description = "Size of the device's largest memory BAR, the window through which the host reaches its "
							   "local memory",
				.source = MetricSource::Static,
				.groups = MetricGroup::PCI,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					const auto bar = memoryBarOf(d);
					if (!bar) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					out = xpum::compat::format("{}", static_cast<double>(bar->size) / (1024.0 * 1024.0));
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto RESIZABLE_BAR = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "pci.bar.resizable",
	.unit = "",
	.description = "Resizable BAR on the largest memory BAR: Enabled or Disabled. N/A where the device does "
				   "not support it or support cannot be read: the runtime reads it from extended configuration "
				   "space, which needs root",
	.source = MetricSource::Static,
	.groups = MetricGroup::PCI,
	.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
		const auto bar = memoryBarOf(d);
		if (!bar) {
			return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
		}
		// An unprivileged runtime cannot read the capability and reports it as unsupported, so
		// "unsupported" is not known to be true.
		if (!bar->resizableSupported) {
			return ZE_RESULT_ERROR_NOT_AVAILABLE;
		}
		out = bar->resizableEnabled ? "Enabled" : "Disabled";
		return ZE_RESULT_SUCCESS;
	}};

constexpr auto ALL = std::to_array<QueryMetric>({
	BASE_CLASS,			 SUB_CLASS,		  LINK_GEN_MAX,	   LINK_WIDTH_MAX,	  LINK_GEN_CURRENT,	   LINK_WIDTH_CURRENT,
	TX_THROUGHPUT,		 RX_THROUGHPUT,	  REPLAY_COUNTER,  RX_THROUGHPUT_KBS, TX_THROUGHPUT_KBS,   LINK_GEN_HOST_MAX,
	LINK_WIDTH_HOST_MAX, AER_CORRECTABLE, AER_NON_FATAL,   AER_FATAL,		  AER_REPLAY_ROLLOVER, AER_REPLAY_TIMEOUT,
	ATOMIC_CAPS,		 ATOMIC_REQUESTS, MEMORY_BAR_SIZE, RESIZABLE_BAR,
});

} // namespace

std::span<const QueryMetric> getPciMetrics() noexcept { return ALL; }

} // namespace metrics::pci
