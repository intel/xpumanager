/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef METRICS_REGISTRY_H
#define METRICS_REGISTRY_H

#include "device.h"
#include "ze_api.h"
#include <cstddef>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <metric.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

namespace metrics {

namespace detail {

/** Duration of the measurement window used by populateMetricCache() and runMetrics(). */
inline constexpr auto SAMPLE_WINDOW = std::chrono::milliseconds{200};

/** Generic underlying-type cast — equivalent to C++23 std::to_underlying. */
template <typename Enum> [[nodiscard]] constexpr auto toUnderlying(Enum e) noexcept
{
	return static_cast<std::underlying_type_t<Enum>>(e);
}

/** Trim leading and trailing ASCII spaces and tabs from a string_view. */
[[nodiscard]] constexpr std::string_view trim(std::string_view s) noexcept
{
	const auto first = s.find_first_not_of(" \t");
	if (first == std::string_view::npos) {
		return {};
	}
	const auto last = s.find_last_not_of(" \t");
	return s.substr(first, last - first + 1);
}

} // namespace detail

// ── MetricGroup: display-section bitmask (-d style) ─────────────────────────────

enum class MetricGroup : uint32_t
{
	NONE = 0,
	IDENTITY = 1U << 0,	   /**< name, index, uuid, serial, driver_version, vbios_version + PCI IDs */
	MEMORY = 1U << 1,	   /**< memory.total/used/free, utilization.memory */
	UTILIZATION = 1U << 2, /**< utilization.gpu/compute/render/media/copy */
	TEMPERATURE = 1U << 3, /**< temperature.gpu/memory */
	POWER = 1U << 4,	   /**< power.draw, energy.consumed */
	CLOCK = 1U << 5,	   /**< clocks.current.graphics/memory */
	PCI = 1U << 6,		   /**< pci.*, pcie.link.*, pcie.tx/rx.throughput, pcie.replay */
	ECC = 1U << 7,		   /**< ecc.mode.current, ecc.errors.corrected/uncorrected; analogous to dmon -s e */
	EU_ARRAY = 1U << 8,	   /**< eu.active/stall/idle (Intel Xe-only) */
	FAN = 1U << 9,		   /**< fan.speed */
	ALL = ~0U,
};

[[nodiscard]] constexpr MetricGroup operator|(MetricGroup a, MetricGroup b) noexcept
{
	return static_cast<MetricGroup>(detail::toUnderlying(a) | detail::toUnderlying(b));
}
[[nodiscard]] constexpr MetricGroup operator&(MetricGroup a, MetricGroup b) noexcept
{
	return static_cast<MetricGroup>(detail::toUnderlying(a) & detail::toUnderlying(b));
}
[[nodiscard]] constexpr bool hasGroup(MetricGroup field, MetricGroup mask) noexcept
{
	return (field & mask) != MetricGroup::NONE;
}

// ── MetricSource ───────────────────────────────────────────────────────────────

enum class MetricSource
{
	Static, /**< Device identity / firmware — cheap, no sampling delay */
	Live,	/**< Runtime telemetry — may require a measurement window */
};

// ── MetricCache: one set of before/after delta samples per device per tick ────
//
// Fields that need a measurement window (utilization, power.draw) read from
// this cache, which is populated ONCE per device per tick by runMetrics()
// before any getters are called.  Static fields ignore it.

// ── Snapshot types: a single point-in-time reading ────────────────────────────

/** Single engine utilisation reading. */
struct EngineSnapshot
{
	uint64_t active = 0, ts = 0;
};

/** Single energy reading for one power domain. */
struct EnergySnapshot
{
	uint64_t energy = 0, ts = 0;
};

/** PCIe byte counters and timestamp at a single point in time. */
struct PcieSnapshot
{
	uint64_t tx = 0, rx = 0, timeUs = 0;
};

/** Memory bandwidth counters and timestamp at a single point in time. */
struct MemSnapshot
{
	uint64_t read = 0, write = 0, ts = 0;
};

// ── Delta sample types: before/after snapshot pair for rate calculation ────────

/** Before/after pair for a single engine group, used to compute utilisation %. */
struct EngineSample
{
	EngineSnapshot before{};
	EngineSnapshot after{};
};

/** Named slots for each sampled engine group. */
struct EngineCache
{
	EngineSample all{};
	EngineSample compute{};
	EngineSample render{};
	EngineSample media{};
	EngineSample copy{};
};

struct MetricCache
{
	EngineCache engines{};
	EnergySnapshot cardPowerBefore{}, cardPowerAfter{}; /**< whole-card energy domain (forGPU=false) */
	EnergySnapshot gpuPowerBefore{}, gpuPowerAfter{};	/**< GPU energy domain (forGPU=true) */
	PcieSnapshot pcieBefore{}, pcieAfter{};				/**< PCIe Rx/Tx byte counters + µs timestamp */
	uint64_t pcieReplay = 0;
	bool pcieAvail = false;
	bool pcieBandwidthAvail = false;	 /**< true when device reports PCIe bandwidth counters */
	bool pcieReplayAvail = false;		 /**< true when device reports PCIe replay counters */
	MemSnapshot memBefore{}, memAfter{}; /**< memory read/write counters + µs timestamp */
	uint64_t memMaxBandwidth = 0;		 /**< peak bandwidth in bytes/s */
	bool memAvail = false;
	bool engineAvail = false; /**< true when engine HAL present and both snapshots taken */
	bool powerAvail = false;  /**< true when power HAL is present */
	/** EU active/stall/idle — populated once per tick by populateMetricCacheEnd */
	EuMetricsData euSample{};
	bool euAvail = false; /**< true when getEuActiveStallIdle succeeded */
	bool populated = false;
};

/**
 * Convenience one-shot wrapper: takes before-samples, sleeps @ref detail::SAMPLE_WINDOW, then takes
 * after-samples. Suitable for one-shot / single-device use.
 *
 * @param dev  Device to sample. Must not be null.
 * @return     A fully-populated MetricCache with @c populated == true.
 * @param window  How long to sleep between before- and after-samples. Defaults to
 *                @ref detail::SAMPLE_WINDOW. Pass a shorter value (e.g. zero) in unit tests
 *                to keep the suite fast.
 * @note         Blocks for approximately @p window. For multi-device use, prefer
 *               @ref populateMetricCacheBegin / @ref populateMetricCacheEnd to amortise
 *               the sleep across all devices.
 */
[[nodiscard]] MetricCache populateMetricCache(devInfo &dev, std::chrono::milliseconds window = detail::SAMPLE_WINDOW);

/**
 * Take the "before" half of a delta sample. Returns immediately without sleeping.
 * Pair with @ref populateMetricCacheEnd after waiting at least @ref detail::SAMPLE_WINDOW.
 *
 * @param dev  Device to sample. Must not be null.
 * @return     A partially-populated MetricCache containing only before-samples.
 *             @c populated is @c false until @ref populateMetricCacheEnd is called.
 */
[[nodiscard]] MetricCache populateMetricCacheBegin(devInfo &dev);

/**
 * Take the "after" half of a delta sample and mark the cache as ready.
 * Must be called after at least @ref detail::SAMPLE_WINDOW has elapsed since the matching
 * @ref populateMetricCacheBegin call.
 *
 * @param dev    Device to sample. Must be the same device passed to the matching
 *               @ref populateMetricCacheBegin call.
 * @param cache  In/out. The MetricCache returned by @ref populateMetricCacheBegin.
 *               On return, all after-sample fields are written and
 *               @c cache.populated is set to @c true.
 * @note         All availability flags are recomputed from scratch on each call.
 *               @c powerAvail requires both @c populateMetricCacheBegin and this call
 *               to have produced valid, advancing timestamps.
 */
void populateMetricCacheEnd(devInfo &dev, MetricCache &cache);

/**
 * Persistent-delta variant for continuous / dmon-loop mode.
 * Copies @p prev's after-samples into the before-slots of a fresh cache, then
 * immediately takes new after-samples from the device — no sleep overhead.
 *
 * @param dev   Device to sample. Must not be null.
 * @param prev  The cache from the previous iteration. After-samples are
 *              promoted to before-samples in the returned cache.
 * @return      A fully-populated MetricCache representing the interval
 *              [prev's after-sample timestamp, now].
 * @pre         @p prev must have been produced by a prior call that set
 *              @c prev.populated == true.
 */
[[nodiscard]] MetricCache populateMetricCacheContinuous(devInfo &dev, const MetricCache &prev);

// ── MetricValue ───────────────────────────────────────────────────────────────

/** The output type written by every getter lambda. */
using MetricValue = std::string;

// ── QueryMetric ────────────────────────────────────────────────────────────────

struct QueryMetric
{
	std::string_view name; /**< xpu-smi query field name, e.g. "temperature.gpu" */
	std::span<const std::string_view> aliases =
		{};						  /**< Alternative names resolved by findMetric; invisible to group expansion */
	std::string_view unit;		  /**< "C", "W", "MHz", "%", "MiB", or "" for dimensionless */
	std::string_view description; /**< Shown by --list-fields / --help-query-gpu */
	MetricSource source;
	MetricGroup groups; /**< Bitmask of display sections this field belongs to */
	ze_result_t (*getter)(devInfo &d, MetricValue &out, const MetricCache &cache);
};

// ── Group name table (detail — not part of the public API) ──────────────────────

namespace detail {

struct MetricGroupEntry
{
	std::string_view name;	   /**< canonical long name for -d filtering and --list-fields */
	std::string_view shortcut; /**< xpu-smi dmon single-letter code ("" if none) */
	MetricGroup group;
};

/**
 * All recognised section names with their optional single-letter shortcuts.
 * "p" appears on both POWER and TEMPERATURE so that parsing "p" yields POWER|TEMPERATURE
 * (parseGroupMask does not break on first match).
 * "v" (throttle violations) is intentionally omitted — use "c" (CLOCK) instead.
 */
inline constexpr auto GROUP_TABLE = std::to_array<MetricGroupEntry>({
	{"IDENTITY", "", MetricGroup::IDENTITY},
	{"MEMORY", "m", MetricGroup::MEMORY},
	{"UTILIZATION", "u", MetricGroup::UTILIZATION},
	{"TEMPERATURE", "p", MetricGroup::TEMPERATURE}, /**< "p" shared with POWER */
	{"POWER", "p", MetricGroup::POWER},				/**< "p" shared with TEMPERATURE */
	{"CLOCK", "c", MetricGroup::CLOCK},
	{"PCI", "t", MetricGroup::PCI},
	{"ECC", "e", MetricGroup::ECC},
	{"EU_ARRAY", "x", MetricGroup::EU_ARRAY},
	{"FAN", "f", MetricGroup::FAN},
	{"ALL", "", MetricGroup::ALL},
});

} // namespace detail

/**
 * Parse a comma-separated list of section names into a @ref MetricGroup bitmask.
 *
 * Tokens are matched case-insensitively against both canonical long names
 * (e.g. @c MEMORY) and single-letter shortcuts (e.g. @c m). Unknown tokens are
 * silently ignored. Leading and trailing whitespace around each token is stripped.
 *
 * Multi-character shorthand strings (e.g. @c "pu") are expanded character-by-character
 * when the whole token has no match, contains no dot, and every character individually
 * maps to a known single-letter shortcut.
 *
 * @note The shortcut @c "p" matches both @c POWER and @c TEMPERATURE in one token.
 *
 * @param sv  Comma-separated token list, e.g. @c "MEMORY,u" or @c "power, ECC".
 * @return    Bitmask of all matched MetricGroup values, or @c MetricGroup::NONE if
 *            no token matched.
 */
[[nodiscard]] constexpr MetricGroup parseGroupMask(std::string_view sv) noexcept
{
	// Case-insensitive single-character comparison (no capture — usable in constexpr).
	constexpr auto icharEq = [](char a, char b) constexpr noexcept {
		constexpr auto up = [](char c) constexpr noexcept -> char {
			return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c;
		};
		return up(a) == up(b);
	};

	constexpr auto toLower = [](char x) constexpr noexcept -> char {
		return (x >= 'A' && x <= 'Z') ? static_cast<char>(x + 32) : x;
	};

	MetricGroup result = MetricGroup::NONE;
	while (!sv.empty()) {
		const auto comma = sv.find(',');
		const auto token = detail::trim(sv.substr(0, comma));

		bool matched = false;
		for (const auto &entry : detail::GROUP_TABLE) {
			// Match single-letter shortcut.
			const bool shortcutMatch = !entry.shortcut.empty() && entry.shortcut.size() == token.size() &&
									   std::equal(entry.shortcut.begin(), entry.shortcut.end(), token.begin(), icharEq);
			// Match canonical long name.
			const bool nameMatch = entry.name.size() == token.size() &&
								   std::equal(entry.name.begin(), entry.name.end(), token.begin(), icharEq);
			if (shortcutMatch || nameMatch) {
				result = result | entry.group;
				matched = true;
			}
		}
		// Multi-char shorthand (e.g. "pu" → POWER | TEMPERATURE | UTILIZATION):
		// expand each character only when the whole token had no match, contains no dot,
		// and *every* character individually maps to a known single-char shortcut.
		if (!matched && !token.empty() && token.find('.') == std::string_view::npos) {
			bool allKnown = true;
			for (char const ch : token) {
				const char lo = toLower(ch);
				bool found = false;
				for (const auto &entry : detail::GROUP_TABLE) {
					if (!entry.shortcut.empty() && icharEq(entry.shortcut[0], lo)) {
						found = true;
						break;
					}
				}
				if (!found) {
					allKnown = false;
					break;
				}
			}
			if (allKnown) {
				for (char const ch : token) {
					const char lo = toLower(ch);
					for (const auto &entry : detail::GROUP_TABLE) {
						if (!entry.shortcut.empty() && icharEq(entry.shortcut[0], lo)) {
							result = result | entry.group;
						}
					}
				}
			}
		}

		if (comma == std::string_view::npos) {
			break;
		}
		sv.remove_prefix(comma + 1);
	}
	return result;
}

// ── Registry API ──────────────────────────────────────────────────────────────

/**
 * Returns all registered metrics in declaration order.
 *
 * @return  Non-owning span over the global metric table. Valid for the lifetime
 *          of the process; elements are stable (never reallocated).
 */
[[nodiscard]] std::span<const QueryMetric> getQueryMetrics();

/**
 * Returns pointers to all metrics whose group bitmask overlaps @p mask.
 *
 * Two special values are handled before the overlap test:
 * - @c MetricGroup::NONE — always returns an empty vector.
 * - @c MetricGroup::ALL  — returns every metric regardless of its own group
 *   membership (including metrics whose @c groups field is @c MetricGroup::NONE).
 *
 * @param mask  A @ref MetricGroup bitmask.
 * @return      Pointers into the global metric table, in declaration order.
 *              Never contains null pointers.
 */
[[nodiscard]] std::vector<const QueryMetric *> getMetricsByGroup(MetricGroup mask);

/**
 * Case-insensitive lookup by metric name or alias.
 *
 * @param name  Metric name to search for, e.g. @c "temperature.gpu".
 *              Also matches entries in @c QueryMetric::aliases.
 * @return      Reference to the matching @ref QueryMetric, or empty optional if not found.
 */
[[nodiscard]] std::optional<std::reference_wrapper<const QueryMetric>> findMetric(std::string_view name);

/**
 * Unified query resolver: handles field names, aliases, and group tokens in one call.
 *
 * Each comma-separated token is resolved as follows:
 *  - Token containing @c '.' → individual metric lookup via @ref findMetric
 *    (canonical name and aliases are both matched).
 *  - Token without @c '.' → group name / alias expansion via @ref parseGroupMask
 *    (including multi-character expansion). If no group matches, falls back to
 *    @ref findMetric for dotless names like @c "timestamp" or @c "name".
 *
 * Duplicate pointers are removed; declaration order is preserved.
 *
 * @param csv  Comma-separated query string, e.g. @c "temperature.gpu,POWER" or @c "pu".
 * @return     Deduplicated, ordered list of matching metric pointers.
 *             Unknown tokens are silently ignored.
 */
[[nodiscard]] std::vector<const QueryMetric *> resolveQuery(std::string_view csv);

/**
 * Format the canonical group names for the given bitmask as a comma-separated string.
 *
 * Only canonical long names (e.g. @c POWER, @c MEMORY) are emitted; aliases and
 * @c ALL are excluded.
 *
 * @param groups  Bitmask of @ref MetricGroup values.
 * @return        Comma-separated string, e.g. @c "POWER, TEMPERATURE".
 *                Returns an empty string for @c MetricGroup::NONE.
 */
[[nodiscard]] std::string formatGroups(MetricGroup groups);

// ── MetricOutput concept ───────────────────────────────────────────────────────

/** Any type satisfying MetricOutput can serve as a sink for evaluated metric results.
 *  Pass an instance to @ref runMetrics() or @ref runMetricsWithCaches(). */
template <typename T>
concept MetricOutput =
	requires(T &t, std::span<const QueryMetric *> fields, devInfo &dev, const QueryMetric &f, const std::string &val) {
		t.onBegin(fields);
		t.onBeginDevice(dev);
		t.onMetric(f, val);
		t.onEndDevice(dev);
		t.onEnd();
	};

// ── Metric evaluation free functions ──────────────────────────────────────────

namespace detail {

/**
 * Inner loop shared by runMetrics and runMetricsWithCaches.
 * Not part of the public API — call runMetrics() or runMetricsWithCaches() instead.
 */
template <MetricOutput Output>
void emitMetrics(Output &output, std::span<const QueryMetric *> fields, std::span<devInfo> devices,
				 std::span<const MetricCache> caches)
{
	output.onBegin(fields);
	for (std::size_t i = 0; i < devices.size(); ++i) {
		output.onBeginDevice(devices[i]);
		for (const QueryMetric *f : fields) {
			std::string val{"[N/A]"};
			if (f->getter(devices[i], val, caches[i]) != ZE_RESULT_SUCCESS) {
				val = "[N/A]";
			}
			output.onMetric(*f, val);
		}
		output.onEndDevice(devices[i]);
	}
	output.onEnd();
}

} // namespace detail

/**
 * Evaluate every metric for every device and route results to @p output.
 *
 * All before-samples are taken for every device first, then a single
 * @ref detail::SAMPLE_WINDOW sleep is observed, then all after-samples are taken.
 * This keeps the total blocking time constant regardless of device count.
 *
 * Metrics tagged @ref MetricSource::Static skip sampling entirely.
 * Metrics whose getter returns a non-success result emit @c "[N/A]".
 *
 * @tparam Output  Any type satisfying @ref MetricOutput.
 * @param output   Sink that receives the structured results.
 * @param fields   Ordered list of metrics to evaluate. Must not contain null pointers.
 * @param devices  Devices to iterate over. May be empty.
 * @note  Blocks for approximately @ref detail::SAMPLE_WINDOW if any @c Live metric is selected.
 */
template <MetricOutput Output>
void runMetrics(Output &output, std::span<const QueryMetric *> fields, std::span<devInfo> devices)
{
	const bool hasLive =
		std::ranges::any_of(fields, [](const QueryMetric *f) { return f->source == MetricSource::Live; });

	std::vector<MetricCache> caches(devices.size());
	if (hasLive) {
		const auto deadline = std::chrono::steady_clock::now() + detail::SAMPLE_WINDOW;
		for (std::size_t i = 0; i < devices.size(); ++i) {
			caches[i] = populateMetricCacheBegin(devices[i]);
		}
		std::this_thread::sleep_until(deadline);
		for (std::size_t i = 0; i < devices.size(); ++i) {
			populateMetricCacheEnd(devices[i], caches[i]);
		}
	}

	detail::emitMetrics(output, fields, devices, caches);
}

/**
 * Evaluate metrics using pre-built caches — for continuous / dmon-loop mode.
 *
 * Unlike @ref runMetrics, this overload does not perform any sampling or sleeping;
 * the caller is responsible for keeping @p caches up to date (e.g. via
 * @ref populateMetricCacheContinuous).
 *
 * @tparam Output  Any type satisfying @ref MetricOutput.
 * @param output   Sink that receives the structured results.
 * @param fields   Ordered list of metrics to evaluate.
 * @param devices  Devices to iterate over.
 * @param caches   Pre-populated caches, one per device.
 * @pre   @c caches.size() >= @c devices.size().
 */
template <MetricOutput Output>
void runMetricsWithCaches(Output &output, std::span<const QueryMetric *> fields, std::span<devInfo> devices,
						  std::span<const MetricCache> caches)
{
	detail::emitMetrics(output, fields, devices, caches);
}

} // namespace metrics

#endif // METRICS_REGISTRY_H
