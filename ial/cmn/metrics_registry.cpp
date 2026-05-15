/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "metrics_registry.h"
#include "metrics/temperature_metrics.h"
#include "metrics/utilization.h"
#include "metrics/pci.h"
#include "metrics/eu_array.h"
#include "metrics/fan.h"
#include "metrics/memory.h"
#include "metrics/power.h"
#include "metrics/ecc.h"
#include "metrics/identity.h"
#include "metrics/clock.h"
#include "device.h"
#include "zes_api.h"
#include "ze_api.h"
#include <enginegroup.h>
#include <functional>
#include <iterator>
#include <memory.h>
#include <metric.h>
#include <numeric>
#include <optional>
#include <pci.h>
#include <power.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <iterator>
#include <numeric>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

bool iequal(std::string_view a, std::string_view b) noexcept
{
	return std::ranges::equal(
		a, b, {}, [](char c) noexcept { return std::tolower(static_cast<unsigned char>(c)); },
		[](char c) noexcept { return std::tolower(static_cast<unsigned char>(c)); });
}

} // namespace

namespace metrics {

using EngineSlot = EngineSample EngineCache::*;

struct EngineEntry
{
	EngineSlot slot;
	zes_engine_group_t group;
};

static constexpr auto ENGINE_MAP = std::to_array<EngineEntry>({
	{&EngineCache::all, ZES_ENGINE_GROUP_ALL},
	{&EngineCache::compute, ZES_ENGINE_GROUP_COMPUTE_ALL},
	{&EngineCache::render, ZES_ENGINE_GROUP_RENDER_ALL},
	{&EngineCache::media, ZES_ENGINE_GROUP_MEDIA_ALL},
	{&EngineCache::copy, ZES_ENGINE_GROUP_COPY_ALL},
});

MetricCache populateMetricCacheBegin(devInfo &dev)
{
	MetricCache cache;
	enginegroup *eg = dev.dev->getEngineGroup();
	auto *pw = dev.dev->getPower();
	auto *mem = dev.dev->getMemory();
	auto *p = dev.dev->getPCI();

	for (const auto &entry : ENGINE_MAP) {
		auto &s = cache.engines.*entry.slot;
		if (eg != nullptr) {
			const auto [r, active, ts] = eg->getUtilization(std::span{&entry.group, 1});
			if (r == ZE_RESULT_SUCCESS) {
				s.before.active = active;
				s.before.ts = ts;
			}
		}
	}
	if (pw != nullptr) {
		pw->getEnergy(&cache.cardPowerBefore.energy, &cache.cardPowerBefore.ts, false);
		pw->getEnergy(&cache.gpuPowerBefore.energy, &cache.gpuPowerBefore.ts, true);
	}
	zes_pci_stats_t ps{};
	const bool pcieBeginOk =
		(p != nullptr && dev.zesDeviceHdl != nullptr && p->getStats(dev.zesDeviceHdl, &ps) == ZE_RESULT_SUCCESS);
	cache.pcieAvail = pcieBeginOk;
	if (pcieBeginOk) {
		cache.pcieBefore.tx = ps.txCounter;
		cache.pcieBefore.rx = ps.rxCounter;
		cache.pcieBefore.timeUs = ps.timestamp;
	}
	if (p != nullptr && dev.zesDeviceHdl != nullptr) {
		zes_pci_properties_t props{};
		if (p->getProperties(dev.zesDeviceHdl, &props) == ZE_RESULT_SUCCESS) {
			cache.pcieBandwidthAvail = props.haveBandwidthCounters;
			cache.pcieReplayAvail = props.haveReplayCounters;
		}
	}
	if (mem != nullptr) {
		mem->getMemoryRW(&cache.memBefore.read, &cache.memBefore.write, &cache.memMaxBandwidth, &cache.memBefore.ts);
	}
	return cache;
}

void populateMetricCacheEnd(devInfo &dev, MetricCache &cache)
{
	enginegroup *eg = dev.dev->getEngineGroup();
	auto *pw = dev.dev->getPower();
	auto *mem = dev.dev->getMemory();
	auto *p = dev.dev->getPCI();

	for (const auto &entry : ENGINE_MAP) {
		auto &s = cache.engines.*entry.slot;
		if (eg != nullptr) {
			const auto [r, active, ts] = eg->getUtilization(std::span{&entry.group, 1});
			if (r == ZE_RESULT_SUCCESS) {
				s.after.active = active;
				s.after.ts = ts;
			}
		}
	}
	bool cardPowerAfterOk = false;
	if (pw != nullptr) {
		cardPowerAfterOk =
			(pw->getEnergy(&cache.cardPowerAfter.energy, &cache.cardPowerAfter.ts, false) == ZE_RESULT_SUCCESS);
		pw->getEnergy(&cache.gpuPowerAfter.energy, &cache.gpuPowerAfter.ts, true);
	}
	zes_pci_stats_t ps{};
	const bool pcieEndOk =
		(p != nullptr && dev.zesDeviceHdl != nullptr && p->getStats(dev.zesDeviceHdl, &ps) == ZE_RESULT_SUCCESS);
	if (pcieEndOk) {
		cache.pcieAfter.tx = ps.txCounter;
		cache.pcieAfter.rx = ps.rxCounter;
		cache.pcieAfter.timeUs = ps.timestamp;
		cache.pcieReplay = ps.replayCounter;
	} else {
		// Reset replay count so a stale value is never visible alongside pcieAvail=false.
		cache.pcieReplay = 0;
	}
	cache.pcieAvail = cache.pcieAvail && pcieEndOk && (cache.pcieAfter.timeUs > cache.pcieBefore.timeUs);
	if (mem != nullptr) {
		const bool ok = (mem->getMemoryRW(&cache.memAfter.read, &cache.memAfter.write, nullptr, &cache.memAfter.ts) ==
						 ZE_RESULT_SUCCESS);
		cache.memAvail = ok && (cache.memBefore.ts != 0) && (cache.memAfter.ts > cache.memBefore.ts);
	}
	cache.engineAvail = (eg != nullptr) && (cache.engines.all.before.ts != 0) &&
						(cache.engines.all.after.ts > cache.engines.all.before.ts);
	// Mirror pcieAvail/memAvail: require a successful HAL call in both Begin and End, plus
	// advancing timestamps. cardPowerBefore.ts == 0 when the Begin call failed or Begin was
	// never called, which correctly prevents powerAvail from being set.
	cache.powerAvail =
		cardPowerAfterOk && (cache.cardPowerBefore.ts != 0) && (cache.cardPowerAfter.ts > cache.cardPowerBefore.ts);

	// EU active/stall/idle — sampled once per tick so all three getters share one HAL call.
	// Unconditionally reset before the attempt so a reused or pre-populated cache never
	// leaks stale EU data when the HAL call fails.
	cache.euAvail = false;
	cache.euSample = {};
	{
		metric *m = dev.dev->getMetric();
		if (m != nullptr && dev.deviceHdl != nullptr) {
			std::vector<EuMetricsData> euVec;
			if (m->getEuActiveStallIdle(dev.deviceHdl, dev.dev->getDriverHandle(), euVec) == ZE_RESULT_SUCCESS &&
				!euVec.empty()) {
				cache.euSample = euVec[0];
				cache.euAvail = true;
			}
		}
	}

	cache.populated = true;
}

MetricCache populateMetricCache(devInfo &dev, std::chrono::milliseconds window)
{
	const auto deadline = std::chrono::steady_clock::now() + window;
	MetricCache cache = populateMetricCacheBegin(dev);
	std::this_thread::sleep_until(deadline);
	populateMetricCacheEnd(dev, cache);
	return cache;
}

MetricCache populateMetricCacheContinuous(devInfo &dev, const MetricCache &prev)
{
	MetricCache curr;

	// Promote prev's after-snapshots into curr's before-slots (no sleep needed).
	for (const auto &entry : ENGINE_MAP) {
		(curr.engines.*entry.slot).before = (prev.engines.*entry.slot).after;
	}
	curr.cardPowerBefore = prev.cardPowerAfter;
	curr.gpuPowerBefore = prev.gpuPowerAfter;
	curr.pcieBefore = prev.pcieAfter;
	// Derive before-validity from whether prev.pcieAfter actually holds a snapshot,
	// not from prev.pcieAvail (which reflects the previous *delta*, not the snapshot).
	// Using prev.pcieAvail would permanently latch pcieAvail=false after any single
	// bad tick (non-advancing timestamp, HAL failure, etc.).
	curr.pcieAvail = (prev.pcieAfter.timeUs != 0);
	curr.pcieBandwidthAvail = prev.pcieBandwidthAvail;
	curr.pcieReplayAvail = prev.pcieReplayAvail;

	curr.memBefore = prev.memAfter;
	curr.memMaxBandwidth = prev.memMaxBandwidth;
	// EU metrics are re-sampled fresh each tick; no before/after state to carry over.

	populateMetricCacheEnd(dev, curr);
	return curr;
}

//  Metric registry — assembled from per-group metric headers
//
//  Registry API implementations

// Each metrics/X.h owns one group and exposes getXMetrics() returning a span.
std::span<const QueryMetric> getQueryMetrics()
{
	static const std::vector<QueryMetric> metrics = []() {
		const auto identityMetrics = metrics::identity::getIdentityMetrics();
		const auto temperatureMetrics = metrics::temperature::getTemperatureMetrics();
		const auto utilizationMetrics = metrics::utilization::getUtilizationMetrics();
		const auto pciMetrics = metrics::pci::getPciMetrics();
		const auto euArrayMetrics = metrics::eu_array::getEuArrayMetrics();
		const auto fanMetrics = metrics::fan::getFanMetrics();
		const auto memoryMetrics = metrics::memory::getMemoryMetrics();
		const auto powerMetrics = metrics::power::getPowerMetrics();
		const auto eccMetrics = metrics::ecc::getEccMetrics();
		const auto clockMetrics = metrics::clock::getClockMetrics();
		const auto groups = std::to_array<std::span<const QueryMetric>>(
			{identityMetrics, temperatureMetrics, utilizationMetrics, pciMetrics, euArrayMetrics, fanMetrics,
			 memoryMetrics, powerMetrics, eccMetrics, clockMetrics});
		std::vector<QueryMetric> v;
		v.reserve(std::transform_reduce(groups.begin(), groups.end(), std::size_t{0}, std::plus<>{},
										[](const auto &s) { return s.size(); }));
		std::ranges::copy(groups | std::views::join, std::back_inserter(v));
		return v;
	}();

	return metrics;
}

std::vector<const QueryMetric *> getMetricsByGroup(MetricGroup mask)
{
	// MetricGroup::NONE matches nothing by definition.
	if (mask == MetricGroup::NONE) {
		return {};
	}
	// MetricGroup::ALL means "every metric": bypass the overlap filter so that
	// metrics whose groups == MetricGroup::NONE are included rather than excluded
	// (NONE & ALL == NONE, which would fail the hasGroup test).
	if (mask == MetricGroup::ALL) {
		auto view =
			getQueryMetrics() | std::views::transform([](const QueryMetric &f) -> const QueryMetric * { return &f; });
		return {view.begin(), view.end()};
	}
	auto view = getQueryMetrics() |
				std::views::filter([mask](const QueryMetric &f) { return hasGroup(f.groups, mask); }) |
				std::views::transform([](const QueryMetric &f) -> const QueryMetric * { return &f; });
	return {view.begin(), view.end()};
}

std::optional<std::reference_wrapper<const QueryMetric>> findMetric(std::string_view name)
{
	const auto allMetrics = getQueryMetrics();
	auto it = std::ranges::find_if(allMetrics, [&](const QueryMetric &f) noexcept {
		if (iequal(f.name, name)) {
			return true;
		}
		return std::ranges::any_of(f.aliases, [&](std::string_view alias) noexcept { return iequal(alias, name); });
	});
	return it != allMetrics.end() ? std::optional<std::reference_wrapper<const QueryMetric>>{*it} : std::nullopt;
}

std::vector<const QueryMetric *> resolveQuery(std::string_view csv)
{
	std::vector<const QueryMetric *> result;

	const auto addUnique = [&](const QueryMetric *f) {
		if (std::ranges::find(result, f) == result.end()) {
			result.push_back(f);
		}
	};

	for (const auto tokenRange : csv | std::views::split(',')) {
		const auto token = detail::trim(std::string_view{tokenRange.begin(), tokenRange.end()});
		if (token.empty()) {
			continue;
		}
		if (token.find('.') != std::string_view::npos) {
			// Dotted token → individual metric lookup (canonical name or alias).
			if (auto f = findMetric(token); f.has_value()) {
				addUnique(&f->get());
			}
		} else {
			// Group name / shortcut → expand via parseGroupMask (includes per-char expansion).
			const MetricGroup mask = parseGroupMask(token);
			if (mask != MetricGroup::NONE) {
				for (const QueryMetric *f : getMetricsByGroup(mask)) {
					addUnique(f);
				}
			} else {
				// Fall back to individual lookup for dotless names like "timestamp", "name".
				if (auto f = findMetric(token); f.has_value()) {
					addUnique(&f->get());
				}
			}
		}
	}
	return result;
}

std::string formatGroups(MetricGroup groups)
{
	std::string result;
	for (const auto &name : detail::GROUP_TABLE | std::views::filter([groups](const detail::MetricGroupEntry &e) {
								return e.group != MetricGroup::ALL && hasGroup(groups, e.group);
							}) | std::views::transform([](const detail::MetricGroupEntry &e) { return e.name; })) {
		if (!result.empty()) {
			result += ", ";
		}
		result.append(name);
	}
	return result;
}

} // namespace metrics
