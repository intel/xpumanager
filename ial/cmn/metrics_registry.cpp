/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "metrics_registry.h"
#include "cmds.h"
#include "metrics/temperature_metrics.h"
#include "metrics/utilization.h"
#include "metrics/pci.h"
#include "metrics/eu_array.h"
#include "metrics/fan.h"
#include "metrics/memory.h"
#include "metrics/power.h"
#include "metrics/identity.h"
#include "metrics/clock.h"
#include "debug.h"
#include "metrics/mode.h"
#include "device.h"
#include "zes_api.h"
#include "ze_api.h"
#include <enginegroup.h>
#include <functional>
#include <future>
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
#include "utility/watchdog/watchdog.h"

namespace {

bool iequal(std::string_view a, std::string_view b) noexcept
{
	return std::ranges::equal(
		a, b, {}, [](char c) noexcept { return std::tolower(static_cast<unsigned char>(c)); },
		[](char c) noexcept { return std::tolower(static_cast<unsigned char>(c)); });
}

} // namespace

namespace metrics {

using EngineSlot = UtilSample EngineCache::*;

struct EngineEntry
{
	EngineSlot slot;
	zes_engine_group_t group;
};

/** The per-engine-class figures, each taken from the aggregated group the driver exposes. */
static constexpr auto ENGINE_MAP = std::to_array<EngineEntry>({
	{&EngineCache::compute, ZES_ENGINE_GROUP_COMPUTE_ALL},
	{&EngineCache::render, ZES_ENGINE_GROUP_RENDER_ALL},
	{&EngineCache::media, ZES_ENGINE_GROUP_MEDIA_ALL},
	{&EngineCache::copy, ZES_ENGINE_GROUP_COPY_ALL},
	// One video engine class does both decode and encode on Linux, so they share one figure.
	{&EngineCache::video, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE},
	{&EngineCache::video, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE},
	{&EngineCache::video, ZES_ENGINE_GROUP_MEDIA_CODEC_SINGLE},
	{&EngineCache::videoEnhance, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE},
});

EngineCache deriveEngineUtil(const std::vector<EngineActivitySample> &before,
							 const std::vector<EngineActivitySample> &after)
{
	EngineCache engines;
	if (const auto gpu = enginegroup::deviceUtilFromTiles(enginegroup::computeGpuUtilPerTile(before, after))) {
		engines.gpu = {.percent = *gpu, .valid = true};
	}
	for (const auto &entry : ENGINE_MAP) {
		const auto perTile = enginegroup::computeGroupUtilPerTile(before, after, entry.group);
		if (const auto util = enginegroup::deviceUtilFromTiles(perTile)) {
			auto &slot = engines.*entry.slot;
			// Several groups can feed one figure; it is the busiest of them.
			slot = {.percent = slot.valid ? std::max(slot.percent, *util) : *util, .valid = true};
		}
	}
	return engines;
}

/**
 * Energy consumed between two readings of one counter, in µJ, allowing for one wrap of a
 * @ref detail::ENERGY_WRAP_UJ counter.
 *
 * @param before  Reading taken at the start of the interval.
 * @param after   Reading taken at the end of the interval, from the same counter.
 * @return The delta, or @c std::nullopt when the counter went backwards in a way a wrap
 *         cannot explain.
 */
std::optional<uint64_t> energyDeltaUj(const EnergySnapshot &before, const EnergySnapshot &after) noexcept
{
	if (after.energy >= before.energy) {
		return after.energy - before.energy;
	}
	if (before.energy < detail::ENERGY_WRAP_UJ) {
		return after.energy + detail::ENERGY_WRAP_UJ - before.energy;
	}
	return std::nullopt;
}

/**
 * Whether @p s is a reading that can close a window opened by @p ref.
 *
 * A reading is rejected when it has no timestamp, an energy of exactly 0 (an accumulating
 * counter never reads 0 once running; a zero is a read of the wrong telemetry region), an
 * energy that is an exact multiple of @ref detail::ENERGY_WRAP_UJ (the same zero read after
 * Level Zero has extended it, taking the drop to 0 for a wrap), a timestamp delta that disagrees with the host-clock
 * delta by more than the slack, an energy regression that is not a wrap, a delta implying more than
 * @ref detail::ENERGY_MAX_PLAUSIBLE_WATTS, or any energy change at an unchanged timestamp.
 * With no reference (@c ref.ts == 0) only the first two apply; the clock check is skipped
 * when either side lacks @c hostUs.
 *
 * @param ref  The reading that opens the window; zeroed when there is none.
 * @param s    The candidate reading.
 * @return     @c true if @p s may be used to close the window.
 */
bool isPlausibleEnergySample(const EnergySnapshot &ref, const EnergySnapshot &s) noexcept
{
	if (s.ts == 0 || s.energy % detail::ENERGY_WRAP_UJ == 0) {
		return false;
	}
	if (ref.ts == 0) {
		return true;
	}
	if (s.ts < ref.ts) {
		return false;
	}
	const uint64_t devUs = s.ts - ref.ts;
	if (ref.hostUs != 0 && s.hostUs >= ref.hostUs) {
		const uint64_t hostUs = s.hostUs - ref.hostUs;
		const uint64_t drift = devUs > hostUs ? devUs - hostUs : hostUs - devUs;
		if (drift > detail::ENERGY_CLOCK_SLACK_US + (hostUs / detail::ENERGY_CLOCK_SLACK_DIVISOR)) {
			return false;
		}
	}
	const auto delta = energyDeltaUj(ref, s);
	if (!delta) {
		return false;
	}
	// The counter latches energy and timestamp together: no time on the counter, no energy.
	if (devUs == 0) {
		return *delta == 0;
	}
	return static_cast<double>(*delta) / static_cast<double>(devUs) <= detail::ENERGY_MAX_PLAUSIBLE_WATTS;
}

/**
 * Take one reading of an energy domain, re-reading up to @ref detail::ENERGY_READ_ATTEMPTS
 * times, @p pause apart, while it is implausible against @p ref (see
 * @ref isPlausibleEnergySample) or the HAL call fails. Re-reads that agree with each other
 * but not with @p ref give @c REBASELINE only when no remaining read closes the window
 * against @p ref, so a short burst of corrupt reads cannot discard a good @p ref.
 *
 * @param read   Takes one reading, stamped with @c hostUs; @c std::nullopt on HAL failure.
 * @param ref    The reading that opens the window; zeroed when there is none.
 * @param pause  How long to wait before each re-read; zero in unit tests.
 * @return       The outcome and, where @ref EnergySampleStatus allows, the reading to use.
 */
EnergySample sampleEnergy(const std::function<std::optional<EnergySnapshot>()> &read, const EnergySnapshot &ref,
						  std::chrono::microseconds pause)
{
	std::optional<EnergySnapshot> previous;
	// Two re-reads that agree with each other but not with ref say ref is the
	// corrupt one; keep waiting on it and the window could never close again. A burst of
	// equally corrupt reads agrees too, though, so spend any reads left on ref first.
	std::optional<EnergySnapshot> rebaseline;
	bool anyReading = false;
	for (unsigned attempt = 0; attempt < detail::ENERGY_READ_ATTEMPTS; ++attempt) {
		if (attempt > 0 && pause.count() > 0) {
			std::this_thread::sleep_for(pause);
		}
		const auto reading = read();
		if (!reading) {
			continue;
		}
		anyReading = true;
		if (isPlausibleEnergySample(ref, *reading)) {
			return {.status = EnergySampleStatus::ACCEPTED, .reading = *reading};
		}
		DBG("Discarding energy reading {} µJ at {} µs (window opened at {} µJ, {} µs)\n", reading->energy, reading->ts,
			ref.energy, ref.ts);
		if (previous && isPlausibleEnergySample(*previous, *reading) &&
			(!rebaseline || isPlausibleEnergySample(*rebaseline, *reading))) {
			rebaseline = reading;
		}
		// A reading with no timestamp or no energy cannot vouch for the next one: compared
		// against it, any reading would pass as "no reference" and trigger a false rebaseline.
		if (isPlausibleEnergySample({}, *reading)) {
			previous = reading;
		}
	}
	if (rebaseline) {
		return {.status = EnergySampleStatus::REBASELINE, .reading = *rebaseline};
	}
	if (!anyReading) {
		return {.status = EnergySampleStatus::FAILED};
	}
	return {.status = EnergySampleStatus::REJECTED, .reading = previous.value_or(EnergySnapshot{})};
}

namespace {

/**
 * Make a reader for @ref sampleEnergy that takes one reading of an energy domain and stamps it
 * with the host steady-clock time.
 *
 * @param pw      Power HAL of the device; must not be null and must outlive the reader.
 * @param forGPU  @c true for the GPU domain, @c false for the card domain.
 * @return        The reader; it yields @c std::nullopt when power::getEnergy() fails.
 */
std::function<std::optional<EnergySnapshot>()> energyReader(::power *pw, bool forGPU)
{
	return [pw, forGPU]() -> std::optional<EnergySnapshot> {
		EnergySnapshot s;
		if (pw->getEnergy(&s.energy, &s.ts, forGPU) != ZE_RESULT_SUCCESS) {
			return std::nullopt;
		}
		s.hostUs = static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
				.count());
		return s;
	};
}

/**
 * Take the reading that opens an energy measurement window.
 *
 * @param pw      Power HAL of the device; must not be null.
 * @param forGPU  @c true for the GPU domain, @c false for the card domain.
 * @return        The reading, or a zeroed snapshot when none is usable.
 */
EnergySnapshot openEnergyWindow(::power *pw, bool forGPU)
{
	const auto sample = sampleEnergy(energyReader(pw, forGPU), {});
	return sample.status == EnergySampleStatus::ACCEPTED ? sample.reading : EnergySnapshot{};
}

/**
 * Close the window @p before opened. @p after is left zeroed unless the reading is trusted, so
 * the getters report N/A for it and populateMetricCacheContinuous() keeps @p before as the next
 * window's opening. @p before is zeroed when the reading shows it to be the corrupt one.
 *
 * @param pw      Power HAL of the device; must not be null.
 * @param forGPU  @c true for the GPU domain, @c false for the card domain.
 * @param before  In/out. The reading that opened the window.
 * @param after   Out. The reading that closes it; zeroed unless trusted.
 * @return Whether [before, after] is a valid measurement window.
 */
bool closeEnergyWindow(::power *pw, bool forGPU, EnergySnapshot &before, EnergySnapshot &after)
{
	const auto sample = sampleEnergy(energyReader(pw, forGPU), before);
	switch (sample.status) {
	case EnergySampleStatus::ACCEPTED:
		after = sample.reading;
		return true;
	case EnergySampleStatus::REBASELINE:
		before = {};
		after = sample.reading;
		return false;
	case EnergySampleStatus::REJECTED:
	case EnergySampleStatus::FAILED:
		break;
	}
	return false;
}

} // namespace

/**
 * @brief Reads EU active/stall/idle for one device.
 *
 * Opens an OA metric streamer and blocks for its fixed monitor window. Takes what it needs by
 * value, so it can run on another thread for longer than the caller's devInfo lives.
 *
 * @retval sample  The first sub-device's figures.
 * @retval nullopt The device has no metric support or the HAL call failed.
 */
[[nodiscard]] static std::optional<EuMetricsData> sampleEu(metric *m, ze_device_handle_t device,
														   ze_driver_handle_t driver)
{
	if (m == nullptr || device == nullptr) {
		return std::nullopt;
	}
	std::vector<EuMetricsData> euVec;
	if (m->getEuActiveStallIdle(device, driver, euVec) != ZE_RESULT_SUCCESS || euVec.empty()) {
		return std::nullopt;
	}
	return euVec[0];
}

// The device's fdinfo snapshots: from the shared snapshot when the caller took one, otherwise
// from a walk of its own.
static std::vector<fdinfo::ProcessSnapshot> fdinfoOf(const devInfo &dev, const fdinfo::DeviceSnapshots *fdinfoNow)
{
	const std::string pci = devPciAddr(dev);
	return fdinfoNow != nullptr ? fdinfo::snapshotsFor(*fdinfoNow, pci) : fdinfo::capture(pci);
}

fdinfo::DeviceSnapshots captureFdinfo(MetricInput inputs)
{
	return hasInput(inputs, MetricInput::FDINFO) ? fdinfo::captureAll() : fdinfo::DeviceSnapshots{};
}

MetricCache populateMetricCacheBegin(devInfo &dev, MetricInput inputs, const fdinfo::DeviceSnapshots *fdinfoNow)
{
	MetricCache cache;
	cache.inputs = inputs;
	enginegroup *eg = dev.dev->getEngineGroup();
	auto *pw = dev.dev->getPower();
	auto *mem = dev.dev->getMemory();
	auto *p = dev.dev->getPCI();

	if (hasInput(inputs, MetricInput::ENGINE) && eg != nullptr &&
		eg->getAllEngineActivity(cache.engineSamplesBefore) != ZE_RESULT_SUCCESS) {
		// Nothing for the end-of-window snapshot to pair against, so every utilization
		// figure will report N/A. Cleared rather than trusted to be empty on failure.
		cache.engineSamplesBefore.clear();
	}
	if (hasInput(inputs, MetricInput::POWER) && pw != nullptr) {
		cache.cardPowerBefore = openEnergyWindow(pw, false);
		cache.gpuPowerBefore = openEnergyWindow(pw, true);
	}
	if (hasInput(inputs, MetricInput::PCIE)) {
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
	}
	if (hasInput(inputs, MetricInput::MEMBW) && mem != nullptr) {
		mem->getMemoryRW(&cache.memBefore.read, &cache.memBefore.write, &cache.memMaxBandwidth, &cache.memBefore.ts);
	}
	if (hasInput(inputs, MetricInput::FDINFO)) {
		cache.fdinfoSnap = fdinfoOf(dev, fdinfoNow);
	}
	if (hasInput(inputs, MetricInput::EU)) {
		cache.euPending =
			std::async(std::launch::async, sampleEu, dev.dev->getMetric(), dev.deviceHdl, dev.dev->getDriverHandle())
				.share();
	}
	return cache;
}

void populateMetricCacheEnd(devInfo &dev, MetricCache &cache, const fdinfo::DeviceSnapshots *fdinfoNow)
{
	const MetricInput inputs = cache.inputs;
	enginegroup *eg = dev.dev->getEngineGroup();
	auto *pw = dev.dev->getPower();
	auto *mem = dev.dev->getMemory();
	auto *p = dev.dev->getPCI();

	// Assigned wholesale rather than updated in place, so a cache that is sampled twice cannot
	// keep a figure derived from the previous window or a group the device no longer reports.
	cache.engineSamplesAfter.clear();
	cache.engines = {};
	if (hasInput(inputs, MetricInput::ENGINE) && eg != nullptr &&
		eg->getAllEngineActivity(cache.engineSamplesAfter) == ZE_RESULT_SUCCESS) {
		cache.engines = deriveEngineUtil(cache.engineSamplesBefore, cache.engineSamplesAfter);
	}
	bool cardPowerAfterOk = false;
	cache.cardPowerAfter = {};
	cache.gpuPowerAfter = {};
	if (hasInput(inputs, MetricInput::POWER) && pw != nullptr) {
		cardPowerAfterOk = closeEnergyWindow(pw, false, cache.cardPowerBefore, cache.cardPowerAfter);
		closeEnergyWindow(pw, true, cache.gpuPowerBefore, cache.gpuPowerAfter);
	}
	if (hasInput(inputs, MetricInput::PCIE)) {
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
		// An idle link may leave the timestamp where it was; the getters read that as zero
		// throughput. Only a timestamp that went backwards makes the window unusable.
		cache.pcieAvail = cache.pcieAvail && pcieEndOk && (cache.pcieAfter.timeUs >= cache.pcieBefore.timeUs);
	} else {
		cache.pcieAvail = false;
		cache.pcieReplay = 0;
		cache.pcieBandwidthAvail = false;
		cache.pcieReplayAvail = false;
	}
	cache.memAvail = false;
	cache.memAfter = {};
	if (hasInput(inputs, MetricInput::MEMBW) && mem != nullptr) {
		const bool ok = (mem->getMemoryRW(&cache.memAfter.read, &cache.memAfter.write, nullptr, &cache.memAfter.ts) ==
						 ZE_RESULT_SUCCESS);
		cache.memAvail = ok && (cache.memBefore.ts != 0) && (cache.memAfter.ts > cache.memBefore.ts);
	}
	cache.engineAvail = !cache.engineSamplesBefore.empty() && !cache.engineSamplesAfter.empty();
	// Mirror pcieAvail/memAvail: require a plausible reading in both Begin and End, plus
	// advancing timestamps. cardPowerBefore.ts == 0 when the Begin reading was unusable or Begin
	// was never called, which correctly prevents powerAvail from being set.
	cache.powerAvail =
		cardPowerAfterOk && (cache.cardPowerBefore.ts != 0) && (cache.cardPowerAfter.ts > cache.cardPowerBefore.ts);

	// EU active/stall/idle — sampled once per tick so all three getters share one HAL call.
	// Unconditionally reset before the attempt so a reused or pre-populated cache never
	// leaks stale EU data when the HAL call fails.
	//
	// Skipped entirely unless a selected field reads it: this opens an OA metric streamer and
	// blocks for a fixed monitor window per device, which dominates the cost of a tick.
	cache.euAvail = false;
	cache.euSample = {};
	if (hasInput(inputs, MetricInput::EU)) {
		const auto eu = cache.euPending.valid()
							? cache.euPending.get()
							: sampleEu(dev.dev->getMetric(), dev.deviceHdl, dev.dev->getDriverHandle());
		if (eu) {
			cache.euSample = *eu;
			cache.euAvail = true;
		}
	}
	cache.euPending = {};

	cache.fdinfoCompute = std::nullopt;
	cache.fdinfoRender = std::nullopt;
	cache.fdinfoMedia = std::nullopt;
	cache.fdinfoVideo = std::nullopt;
	cache.fdinfoVideoEnhance = std::nullopt;
	cache.fdinfoCopy = std::nullopt;
	if (hasInput(inputs, MetricInput::FDINFO)) {
		auto fdAfter = fdinfoOf(dev, fdinfoNow);
		const auto devUtil = fdinfo::aggregateDeviceUtil(cache.fdinfoSnap, fdAfter);
		cache.fdinfoSnap = std::move(fdAfter);
		cache.fdinfoCompute = devUtil.compute;
		cache.fdinfoRender = devUtil.render;
		cache.fdinfoMedia = devUtil.media;
		cache.fdinfoVideo = devUtil.video;
		cache.fdinfoVideoEnhance = devUtil.videoEnhance;
		cache.fdinfoCopy = devUtil.copy;
	}
	cache.populated = true;
}

MetricCache populateMetricCache(devInfo &dev, std::chrono::milliseconds window, MetricInput inputs)
{
	const auto deadline = std::chrono::steady_clock::now() + window;
	MetricCache cache = populateMetricCacheBegin(dev, inputs);
	if (inputs != MetricInput::NONE) {
		xpum::watchdog::sleepUntil(deadline);
	}
	populateMetricCacheEnd(dev, cache);
	return cache;
}

MetricInput inputsOf(std::span<const QueryMetric *const> fields) noexcept
{
	return std::accumulate(fields.begin(), fields.end(), MetricInput::NONE,
						   [](MetricInput acc, const QueryMetric *f) { return acc | f->inputs; });
}

std::optional<float> busiestEngineClass(std::optional<float> compute, std::optional<float> render,
										std::optional<float> media, std::optional<float> copy) noexcept
{
	std::optional<float> busiest;
	for (const auto cls : {compute, render, media, copy}) {
		if (cls) {
			busiest = std::max(busiest.value_or(0.0F), *cls);
		}
	}
	return busiest;
}

MetricCache populateMetricCacheContinuous(devInfo &dev, const MetricCache &prev, MetricInput inputs,
										  const fdinfo::DeviceSnapshots *fdinfoNow)
{
	MetricCache curr;
	curr.inputs = inputs;
	curr.serialNumber = prev.serialNumber;
	curr.serialRetryAt = prev.serialRetryAt;

	// Promote prev's after-snapshots into curr's before-slots (no sleep needed).
	// The engine figures are derived rather than carried over: what a new window needs is the
	// raw counters prev ended on, not the percentages they produced.
	curr.engineSamplesBefore = prev.engineSamplesAfter;
	// A reading populateMetricCacheEnd() discarded leaves the after-snapshot zeroed; hold the
	// last trusted one instead, so a corrupt read costs this tick's row and not the next one too.
	curr.cardPowerBefore = (prev.cardPowerAfter.ts != 0) ? prev.cardPowerAfter : prev.cardPowerBefore;
	curr.gpuPowerBefore = (prev.gpuPowerAfter.ts != 0) ? prev.gpuPowerAfter : prev.gpuPowerBefore;
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

	// fdinfo fallback: carry the previous snapshot forward so populateMetricCacheEnd()
	// can compute per-tick deltas; EU metrics are re-sampled fresh each tick.
	curr.fdinfoSnap = prev.fdinfoSnap;

	populateMetricCacheEnd(dev, curr, fdinfoNow);
	return curr;
}

//  Metric registry — assembled from per-group metric headers
//
//  Registry API implementations

// Each metrics/X.h owns one group and exposes getXMetrics() returning a span. The ECC group
// (metrics/ecc.h) is left out until its RAS reads are reliable: a RAS call the driver never
// answers stalls every command that samples it.

std::span<const QueryMetric> getQueryMetrics() noexcept
{
	static const std::vector<QueryMetric> registry = [] {
		const auto identity = identity::getIdentityMetrics();
		const auto temperature = temperature::getTemperatureMetrics();
		const auto utilization = utilization::getUtilizationMetrics();
		const auto pci = pci::getPciMetrics();
		const auto euArray = eu_array::getEuArrayMetrics();
		const auto fan = fan::getFanMetrics();
		const auto memory = memory::getMemoryMetrics();
		const auto power = power::getPowerMetrics();
		const auto clock = clock::getClockMetrics();
		const auto mode = mode::getModeMetrics();

		std::vector<QueryMetric> metricVec;
		for (const std::span<const QueryMetric> s :
			 {identity, temperature, utilization, pci, euArray, fan, memory, power, clock, mode}) {
			metricVec.insert(metricVec.end(), s.begin(), s.end());
		}
		return metricVec;
	}();
	return registry;
}

std::vector<const QueryMetric *> getMetricsByGroup(MetricGroup mask)
{
	// MetricGroup::NONE matches nothing by definition.
	if (mask == MetricGroup::NONE) {
		return {};
	}
	const auto all = getQueryMetrics();
	// MetricGroup::ALL means "every metric": bypass the overlap filter so that
	// metrics whose groups == MetricGroup::NONE are included rather than excluded
	// (NONE & ALL == NONE, which would fail the hasGroup test).
	if (mask == MetricGroup::ALL) {
		std::vector<const QueryMetric *> result;
		result.reserve(all.size());
		for (const auto &f : all) {
			result.push_back(&f);
		}
		return result;
	}
	std::vector<const QueryMetric *> result;
	for (const auto &f : all) {
		if (hasGroup(f.groups, mask)) {
			result.push_back(&f);
		}
	}
	return result;
}

// Case-insensitive glob match against a metric name.
//   '*' — matches zero or more characters (e.g. "power.*" matches "power.gpu.sustained")
//   '?' — matches exactly one character  (e.g. "temperature.?pu" matches "temperature.gpu" but not
//   "temperature.memory")
// Uses the standard O(n*m) iterative backtracking algorithm.
static bool globMatch(std::string_view pattern, std::string_view text) noexcept
{
	const auto toLower = [](char c) noexcept -> char { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; };
	size_t patternPos = 0, textPos = 0;
	size_t lastStarPatternPos = std::string_view::npos, lastStarTextPos = 0;
	while (textPos < text.size()) {
		if (patternPos < pattern.size() &&
			(pattern[patternPos] == '?' || toLower(pattern[patternPos]) == toLower(text[textPos]))) {
			++patternPos;
			++textPos;
		} else if (patternPos < pattern.size() && pattern[patternPos] == '*') {
			lastStarPatternPos = patternPos++;
			lastStarTextPos = textPos;
		} else if (lastStarPatternPos != std::string_view::npos) {
			patternPos = lastStarPatternPos + 1;
			textPos = ++lastStarTextPos;
		} else {
			return false;
		}
	}
	while (patternPos < pattern.size() && pattern[patternPos] == '*') {
		++patternPos;
	}
	return patternPos == pattern.size();
}

std::optional<QueryMetric> findMetric(std::string_view name)
{
	const auto allMetrics = getQueryMetrics();
	auto it = std::ranges::find_if(allMetrics, [&](const QueryMetric &f) noexcept {
		if (iequal(f.name, name)) {
			return true;
		}
		return std::ranges::any_of(f.aliases, [&](std::string_view alias) noexcept { return iequal(alias, name); });
	});
	return it != allMetrics.end() ? std::optional<QueryMetric>{*it} : std::nullopt;
}

std::vector<const QueryMetric *> resolveQuery(std::string_view csv)
{
	const std::span<const QueryMetric> allMetrics = getQueryMetrics();
	std::vector<const QueryMetric *> result;

	const auto addUnique = [&](const QueryMetric &f) {
		if (std::ranges::none_of(result, [&f](const QueryMetric *r) { return r->name == f.name; })) {
			result.push_back(&f);
		}
	};

	const auto findInAll = [&](std::string_view name) -> const QueryMetric * {
		auto it = std::ranges::find_if(allMetrics, [&](const QueryMetric &f) noexcept {
			return iequal(f.name, name) ||
				   std::ranges::any_of(f.aliases, [&](std::string_view a) noexcept { return iequal(a, name); });
		});
		return it != allMetrics.end() ? &*it : nullptr;
	};

	for (const auto tokenRange : csv | std::views::split(',')) {
		const auto token = detail::trim(std::string_view{tokenRange.begin(), tokenRange.end()});
		if (token.empty()) {
			continue;
		}
		if (token.find_first_of("*?") != std::string_view::npos) {
			// Wildcard pattern: match against all metric names and aliases.
			for (const QueryMetric &f : allMetrics) {
				if (globMatch(token, f.name) ||
					std::ranges::any_of(f.aliases, [&](std::string_view a) noexcept { return globMatch(token, a); })) {
					addUnique(f);
				}
			}
		} else if (token.find('.') != std::string_view::npos) {
			// Dotted token → individual metric lookup (canonical name or alias).
			if (const auto *f = findInAll(token)) {
				addUnique(*f);
			}
		} else {
			// Group name / shortcut → expand via parseGroupMask (includes per-char expansion).
			const MetricGroup mask = parseGroupMask(token);
			if (mask != MetricGroup::NONE) {
				const bool matchAll = (mask == MetricGroup::ALL);
				for (const QueryMetric &f : allMetrics) {
					if (matchAll || hasGroup(f.groups, mask)) {
						addUnique(f);
					}
				}
			} else {
				// Fall back to individual lookup for dotless names like "timestamp", "name".
				if (const auto *f = findInAll(token)) {
					addUnique(*f);
				}
			}
		}
	}
	return result;
}

/**
 * @brief Snapshots the names in detail::GROUP_TABLE into a contiguous static array.
 *
 * The array is sized from and filled by GROUP_TABLE at compile time, so it always holds exactly
 * that many names and in that order; adding or renaming a section needs no change here. See
 * metrics_registry.h for what callers may rely on.
 *
 * @return  Span over function-local static storage, valid for the lifetime of the program.
 */
std::span<const std::string_view> sectionNames() noexcept
{
	static constexpr auto NAMES = []() constexpr {
		std::array<std::string_view, detail::GROUP_TABLE.size()> names{};
		std::size_t n = 0;
		for (const auto &entry : detail::GROUP_TABLE) {
			names[n++] = entry.name;
		}
		return names;
	}();
	return NAMES;
}

/**
 * @brief Greedily wraps sectionNames() into lines, packing each line until the next name overflows.
 *
 * The separating comma is appended to a name before the fit is tested, so a comma can never be
 * left dangling at the start of the next line. @p width is a budget rather than a hard limit: a
 * single name longer than it still gets a line to itself instead of being split or dropped.
 *
 * @param width  Preferred maximum line length, in characters.
 * @return       The rendered lines, in GROUP_TABLE order; see metrics_registry.h for guarantees.
 */
std::vector<std::string> formatSectionNames(std::size_t width)
{
	std::vector<std::string> lines;
	std::string current;

	const auto names = sectionNames();
	for (std::size_t i = 0; i < names.size(); ++i) {
		std::string token{names[i]};
		if (i + 1 < names.size()) {
			token += ',';
		}
		if (current.empty()) {
			current = std::move(token);
		} else if (current.size() + 1 + token.size() > width) {
			lines.push_back(std::move(current));
			current = std::move(token);
		} else {
			current += ' ';
			current += token;
		}
	}

	if (!current.empty()) {
		lines.push_back(std::move(current));
	}
	if (lines.empty()) {
		lines.emplace_back();
	}
	return lines;
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
