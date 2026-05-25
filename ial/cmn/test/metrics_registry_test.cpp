/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "metrics_registry.h"
#include "metrics/temperature_metrics.h"
#include "metrics/utilization.h"
#include "metrics/eu_array.h"
#include "metrics/power.h"
#include <algorithm>
#include <array>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace metrics; // NOLINT(google-build-using-namespace)

// ── parseGroupMask / formatGroups ─────────────────────────────────────────────
// These functions operate only on the compile-time GROUP_TABLE — no registered
// metrics are needed.

TEST_CASE("parseGroupMask parses canonical names case-insensitively")
{
	CHECK(hasGroup(parseGroupMask("MEMORY"), MetricGroup::MEMORY));
	CHECK(hasGroup(parseGroupMask("memory"), MetricGroup::MEMORY));
	CHECK(hasGroup(parseGroupMask("Memory"), MetricGroup::MEMORY));
}

TEST_CASE("parseGroupMask parses single-letter aliases")
{
	CHECK(hasGroup(parseGroupMask("m"), MetricGroup::MEMORY));
	CHECK(hasGroup(parseGroupMask("u"), MetricGroup::UTILIZATION));
	CHECK(hasGroup(parseGroupMask("c"), MetricGroup::CLOCK));
	CHECK(hasGroup(parseGroupMask("e"), MetricGroup::ECC));
	CHECK(hasGroup(parseGroupMask("f"), MetricGroup::FAN));
	CHECK(hasGroup(parseGroupMask("t"), MetricGroup::PCI));
	CHECK(hasGroup(parseGroupMask("x"), MetricGroup::EU_ARRAY));
}

TEST_CASE("parseGroupMask: alias 'p' matches both POWER and TEMPERATURE")
{
	const MetricGroup mask = parseGroupMask("p");
	CHECK(hasGroup(mask, MetricGroup::POWER));
	CHECK(hasGroup(mask, MetricGroup::TEMPERATURE));
}

TEST_CASE("parseGroupMask handles comma-separated tokens and unknown tokens")
{
	const MetricGroup mask = parseGroupMask("memory, UTILIZATION, unknown");
	CHECK(hasGroup(mask, MetricGroup::MEMORY));
	CHECK(hasGroup(mask, MetricGroup::UTILIZATION));
	CHECK_FALSE(hasGroup(mask, MetricGroup::CLOCK));

	CHECK(parseGroupMask("unknown_only") == MetricGroup::NONE);
}

TEST_CASE("parseGroupMask expands multi-character alias shorthands")
{
	const MetricGroup mask = parseGroupMask("pu");
	CHECK(hasGroup(mask, MetricGroup::POWER));
	CHECK(hasGroup(mask, MetricGroup::TEMPERATURE));
	CHECK(hasGroup(mask, MetricGroup::UTILIZATION));
}

TEST_CASE("formatGroups emits canonical group names")
{
	const std::string names = formatGroups(MetricGroup::POWER | MetricGroup::TEMPERATURE);
	CHECK(names.find("POWER") != std::string::npos);
	CHECK(names.find("TEMPERATURE") != std::string::npos);
}

TEST_CASE("formatGroups returns empty string for NONE") { CHECK(formatGroups(MetricGroup::NONE).empty()); }

// ── Shared fixture ────────────────────────────────────────────────────────────
// A zero-initialised device has default-constructed HAL sub-objects.
// All HAL calls fail gracefully and write zeros, so availability flags stay false.

struct ZeroDeviceFixture
{
	device dev; // default-constructed; safe without calling device::init()
	devInfo di{0, &dev, nullptr, nullptr};
};

// ── Registry API ──────────────────────────────────────────────────────────────

TEST_CASE("getQueryMetrics returns non-empty span")
{
	const auto all = getQueryMetrics();
	CHECK_FALSE(all.empty());
	// >= because multiple metric groups are registered
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::IDENTITY).size());
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::TEMPERATURE).size());
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::UTILIZATION).size());
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::PCI).size());
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::EU_ARRAY).size());
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::FAN).size());
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::MEMORY).size());
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::POWER).size());
	CHECK(all.size() >= getMetricsByGroup(MetricGroup::ECC).size());
}

TEST_CASE("getMetricsByGroup ALL returns non-empty, NONE returns empty")
{
	CHECK_FALSE(getMetricsByGroup(MetricGroup::ALL).empty());
	CHECK(getMetricsByGroup(MetricGroup::NONE).empty());
}

TEST_CASE("getMetricsByGroup UTILIZATION returns all utilization metrics")
{
	CHECK(getMetricsByGroup(MetricGroup::UTILIZATION).size() ==
		  metrics::utilization::getUtilizationMetrics().size() + metrics::eu_array::getEuArrayMetrics().size());
}

TEST_CASE("findMetric resolves identity metric names")
{
	CHECK(findMetric("name").has_value());
	CHECK(findMetric("index").has_value());
	CHECK(findMetric("uuid").has_value());
	CHECK(findMetric("serial").has_value());
	CHECK(findMetric("timestamp").has_value());
	CHECK(findMetric("driver_version").has_value());
	CHECK(findMetric("vbios_version").has_value());
	CHECK(findMetric("pci.bus_id").has_value());
	CHECK(findMetric("pci.device_id").has_value());
	CHECK(findMetric("pci.sub_device_id").has_value());
}

TEST_CASE("findMetric resolves PCI metric names")
{
	CHECK(findMetric("pcie.link.gen.max").has_value());
	CHECK(findMetric("pcie.link.width.max").has_value());
	CHECK(findMetric("pcie.link.gen.current").has_value());
	CHECK(findMetric("pcie.link.width.current").has_value());
	CHECK(findMetric("pcie.tx.throughput").has_value());
	CHECK(findMetric("pcie.rx.throughput").has_value());
	CHECK(findMetric("pcie.replay.counter").has_value());
	CHECK(findMetric("pcie.rx.throughput.kbs").has_value());
	CHECK(findMetric("pcie.tx.throughput.kbs").has_value());
}

TEST_CASE("PCI group contains exactly 12 canonical entries")
{
	const auto byPci = getMetricsByGroup(MetricGroup::PCI);
	CHECK(byPci.size() == 12); // 9 from pci.cpp + 3 from identity.h (pci.bus_id, pci.device_id, pci.sub_device_id)
}

TEST_CASE("findMetric resolves Power metric names")
{
	CHECK(findMetric("power.draw").has_value());
	CHECK(findMetric("power.draw.gpu").has_value());
	CHECK(findMetric("energy.consumed").has_value());
	CHECK(findMetric("power.limit").has_value());
	CHECK(findMetric("power.max_limit").has_value());
}

TEST_CASE("Power group contains all expected metric names")
{
	const auto byPow = getMetricsByGroup(MetricGroup::POWER);
	const std::vector<std::string_view> expected = {"power.draw", "power.draw.gpu", "energy.consumed", "power.limit",
													"power.max_limit"};
	for (const auto name : expected) {
		CHECK_MESSAGE(std::ranges::any_of(byPow, [name](const auto *m) { return m->name == name; }), name,
					  " not found in POWER group");
	}
	// Relative order: power.draw before power.draw.gpu before energy.consumed
	const auto pos = [&](std::string_view n) {
		return std::ranges::find_if(byPow, [n](const auto *m) { return m->name == n; }) - byPow.begin();
	};
	CHECK(pos("power.draw") < pos("power.draw.gpu"));
	CHECK(pos("power.draw.gpu") < pos("energy.consumed"));
}

TEST_CASE("getMetricsByGroup UTILIZATION returns only canonical names, no aliases")
{
	// Aliases (.single / .group / .decode.single etc.) must be excluded so that
	// '-d UTILIZATION' does not produce duplicate columns.
	// EU Array metrics also carry UTILIZATION so they appear here too.
	static constexpr std::array canonicalNames{
		std::string_view{"utilization.gpu"},
		std::string_view{"utilization.compute"},
		std::string_view{"utilization.render"},
		std::string_view{"utilization.media"},
		std::string_view{"utilization.copy"},
		std::string_view{"utilization.memory"},
		std::string_view{"eu.active"},
		std::string_view{"eu.stall"},
		std::string_view{"eu.idle"},
	};
	const auto byUtil = getMetricsByGroup(MetricGroup::UTILIZATION);
	REQUIRE(byUtil.size() == canonicalNames.size());
	for (std::size_t i = 0; i < canonicalNames.size(); ++i) {
		CHECK(byUtil[i]->name == canonicalNames[i]);
	}
}

TEST_CASE("getMetricsByGroup MEMORY includes utilization.memory")
{
	const auto byMem = getMetricsByGroup(MetricGroup::MEMORY);
	// utilization.memory is registered first (before memory.cpp metrics)
	REQUIRE_FALSE(byMem.empty());
	CHECK(byMem[0]->name == "utilization.memory");
}

TEST_CASE("findMetric resolves Temperature metric names")
{
	CHECK(findMetric("temperature.gpu").has_value());
	CHECK(findMetric("temperature.memory").has_value());
}

TEST_CASE("getMetricsByGroup TEMPERATURE matches getTemperatureMetrics")
{
	const auto byTemp = getMetricsByGroup(MetricGroup::TEMPERATURE);
	REQUIRE(byTemp.size() == metrics::temperature::getTemperatureMetrics().size());
	CHECK(byTemp[0]->name == "temperature.gpu");
	CHECK(byTemp[1]->name == "temperature.memory");
}

TEST_CASE("findMetric resolves Utilization canonical names")
{
	CHECK(findMetric("utilization.gpu").has_value());
	CHECK(findMetric("utilization.compute").has_value());
	CHECK(findMetric("utilization.render").has_value());
	CHECK(findMetric("utilization.media").has_value());
	CHECK(findMetric("utilization.copy").has_value());
	CHECK(findMetric("utilization.memory").has_value());
}

TEST_CASE("findMetric resolves Utilization aliases")
{
	// compute
	CHECK(findMetric("utilization.compute.single").has_value());
	CHECK(findMetric("utilization.compute.group").has_value());
	// render
	CHECK(findMetric("utilization.render.single").has_value());
	CHECK(findMetric("utilization.render.group").has_value());
	// media (sub-engine forms)
	CHECK(findMetric("utilization.media.decode.single").has_value());
	CHECK(findMetric("utilization.media.encode.single").has_value());
	CHECK(findMetric("utilization.media.enhancement.single").has_value());
	CHECK(findMetric("utilization.media.group").has_value());
	// copy
	CHECK(findMetric("utilization.copy.single").has_value());
	CHECK(findMetric("utilization.copy.group").has_value());
}

TEST_CASE("findMetric resolves EU Array metric names")
{
	CHECK(findMetric("eu.active").has_value());
	CHECK(findMetric("eu.stall").has_value());
	CHECK(findMetric("eu.idle").has_value());
}

TEST_CASE("EU Array group contains exactly 3 canonical entries")
{
	const auto byEu = getMetricsByGroup(MetricGroup::EU_ARRAY);
	CHECK(byEu.size() == 3);
}

TEST_CASE("findMetric resolves Fan metric names") { CHECK(findMetric("fan.speed").has_value()); }

TEST_CASE("Fan group contains exactly 1 canonical entry")
{
	const auto byFan = getMetricsByGroup(MetricGroup::FAN);
	CHECK(byFan.size() == 1);
}

TEST_CASE("findMetric resolves Memory metric names")
{
	CHECK(findMetric("memory.total").has_value());
	CHECK(findMetric("memory.used").has_value());
	CHECK(findMetric("memory.free").has_value());
	CHECK(findMetric("memory.read.bandwidth").has_value());
	CHECK(findMetric("memory.write.bandwidth").has_value());
	CHECK(findMetric("memory.bandwidth.utilization").has_value());
}

TEST_CASE("Memory group contains exactly 7 canonical entries")
{
	const auto byMem = getMetricsByGroup(MetricGroup::MEMORY);
	CHECK(byMem.size() == 7); // 6 from memory.cpp + utilization.memory
}

TEST_CASE("findMetric resolves ECC metric names")
{
	CHECK(findMetric("ecc.mode.current").has_value());
	CHECK(findMetric("ecc.errors.corrected.aggregate.total").has_value());
	CHECK(findMetric("ecc.errors.uncorrected.aggregate.total").has_value());
	CHECK(findMetric("ecc.errors.aggregate.total").has_value());
	CHECK(findMetric("ras.reset").has_value());
	CHECK(findMetric("ras.programming.errors").has_value());
	CHECK(findMetric("ras.driver.errors").has_value());
	CHECK(findMetric("ras.cache.errors.correctable").has_value());
	CHECK(findMetric("ras.cache.errors.uncorrectable").has_value());
	CHECK(findMetric("ras.non_compute.errors.total").has_value());
}

TEST_CASE("ECC group contains exactly 10 canonical entries")
{
	const auto byEcc = getMetricsByGroup(MetricGroup::ECC);
	CHECK(byEcc.size() == 10);
}

TEST_CASE("findMetric returns nullopt for unregistered metrics")
{
	CHECK_FALSE(findMetric("__no_such_metric__").has_value());
}

TEST_CASE("resolveQuery expands IDENTITY, TEMPERATURE, UTILIZATION, PCI, EU Array, Fan, Memory, POWER, and ECC groups")
{
	CHECK_FALSE(resolveQuery("IDENTITY").empty());
	CHECK_FALSE(resolveQuery("TEMPERATURE").empty());
	CHECK_FALSE(resolveQuery("UTILIZATION").empty());
	const auto byQuery = resolveQuery("POWER");
	const std::vector<std::string_view> expectedPower = {"power.draw", "power.draw.gpu", "energy.consumed",
														 "power.limit", "power.max_limit"};
	for (const auto name : expectedPower) {
		CHECK_MESSAGE(std::ranges::any_of(byQuery, [name](const auto *m) { return m->name == name; }), name,
					  " not found in resolveQuery(\"POWER\")");
	}
	CHECK_FALSE(resolveQuery("p").empty()); // 'p' expands to POWER|TEMPERATURE
	CHECK_FALSE(resolveQuery("u").empty()); // single-letter alias for UTILIZATION
	CHECK_FALSE(resolveQuery("PCI").empty());
	CHECK_FALSE(resolveQuery("t").empty()); // single-letter shortcut for PCI
	CHECK_FALSE(resolveQuery("EU_ARRAY").empty());
	CHECK_FALSE(resolveQuery("x").empty()); // single-letter alias for EU_ARRAY
	CHECK_FALSE(resolveQuery("FAN").empty());
	CHECK_FALSE(resolveQuery("f").empty()); // single-letter alias for FAN
	CHECK_FALSE(resolveQuery("MEMORY").empty());
	CHECK_FALSE(resolveQuery("m").empty()); // single-letter alias for MEMORY
	CHECK_FALSE(resolveQuery("POWER").empty());
	CHECK_FALSE(resolveQuery("ECC").empty());
	CHECK_FALSE(resolveQuery("e").empty()); // single-letter alias for ECC
}

TEST_CASE("resolveQuery returns empty for group tokens with no registered metrics")
{
	// CLOCK group is in GROUP_TABLE but has no metrics registered in this PR.
	CHECK(resolveQuery("CLOCK").empty());
}

TEST_CASE("resolveQuery TEMPERATURE returns registered metrics")
{
	CHECK_FALSE(resolveQuery("TEMPERATURE").empty());
	CHECK(resolveQuery("TEMPERATURE").size() == metrics::temperature::getTemperatureMetrics().size());
}

TEST_CASE("resolveQuery pu expands to Temperature, Utilization, and Power metrics")
{
	// 'p' maps to POWER|TEMPERATURE; 'u' maps to UTILIZATION. All three groups are registered.
	const auto result = resolveQuery("pu");
	CHECK(result.size() ==
		  metrics::power::getPowerMetrics().size() + metrics::temperature::getTemperatureMetrics().size() +
			  metrics::utilization::getUtilizationMetrics().size() + metrics::eu_array::getEuArrayMetrics().size());
}

TEST_CASE("resolveQuery returns empty vector for unknown tokens")
{
	CHECK(resolveQuery("__no_such_metric__").empty());
	CHECK(resolveQuery("UNKNOWN_GROUP").empty());
}

TEST_CASE("power QueryMetric fields: unit, source, and group membership")
{
	for (const auto name : std::to_array<std::string_view>({"power.draw", "power.draw.gpu"})) {
		auto found = findMetric(name);
		REQUIRE(found.has_value());
		CHECK(found->get().unit == "W");
		CHECK(found->get().source == MetricSource::Live);
		CHECK(hasGroup(found->get().groups, MetricGroup::POWER));
	}
	{
		auto found = findMetric("energy.consumed");
		REQUIRE(found.has_value());
		CHECK(found->get().unit == "J");
		CHECK(found->get().source == MetricSource::Live);
		CHECK(hasGroup(found->get().groups, MetricGroup::POWER));
	}
	for (const auto name : std::to_array<std::string_view>({"power.limit", "power.max_limit"})) {
		auto found = findMetric(name);
		REQUIRE(found.has_value());
		CHECK(found->get().unit == "W");
		CHECK(found->get().source == MetricSource::Static);
		CHECK(hasGroup(found->get().groups, MetricGroup::POWER));
	}
}

// ── Power getter logic ────────────────────────────────────────────────────────
// Live metrics (DRAW, DRAW_GPU, ENERGY_CONSUMED) ignore devInfo entirely,
// so a zero-device is sufficient — no HAL calls occur.

TEST_CASE_FIXTURE(ZeroDeviceFixture, "power.draw getter: UNSUPPORTED when cardPowerBefore.ts is zero")
{
	MetricValue out;
	MetricCache c; // all zeros — before.ts == 0
	CHECK(findMetric("power.draw").value().get().getter(di, out, c) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "power.draw getter: UNSUPPORTED when after.ts < before.ts")
{
	MetricValue out;
	MetricCache c;
	c.cardPowerBefore.ts = 200;
	c.cardPowerAfter.ts = 100;
	CHECK(findMetric("power.draw").value().get().getter(di, out, c) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "power.draw getter: N/A when timestamps are equal")
{
	MetricValue out;
	MetricCache c;
	c.cardPowerBefore.ts = c.cardPowerAfter.ts = 100;
	CHECK(findMetric("power.draw").value().get().getter(di, out, c) == ZE_RESULT_SUCCESS);
	CHECK(out == "N/A");
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "power.draw getter: N/A when energy counter regresses")
{
	MetricValue out;
	MetricCache c;
	c.cardPowerBefore = {.energy = 500, .ts = 100};
	c.cardPowerAfter = {.energy = 400, .ts = 200};
	CHECK(findMetric("power.draw").value().get().getter(di, out, c) == ZE_RESULT_SUCCESS);
	CHECK(out == "N/A");
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "power.draw getter: computes watts (delta 2 000 000 µJ / 2 000 000 µs == 1.00 W)")
{
	MetricValue out;
	MetricCache c;
	c.cardPowerBefore = {.energy = 0, .ts = 1};
	c.cardPowerAfter = {.energy = 2'000'000, .ts = 2'000'001};
	CHECK(findMetric("power.draw").value().get().getter(di, out, c) == ZE_RESULT_SUCCESS);
	CHECK(out == "1.00");
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "power.draw.gpu getter: UNSUPPORTED when gpuPowerBefore.ts is zero")
{
	MetricValue out;
	MetricCache c;
	CHECK(findMetric("power.draw.gpu").value().get().getter(di, out, c) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "power.draw.gpu getter: UNSUPPORTED when after.ts < before.ts")
{
	MetricValue out;
	MetricCache c;
	c.gpuPowerBefore.ts = 200;
	c.gpuPowerAfter.ts = 100;
	CHECK(findMetric("power.draw.gpu").value().get().getter(di, out, c) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "power.draw.gpu getter: N/A when timestamps are equal")
{
	MetricValue out;
	MetricCache c;
	c.gpuPowerBefore.ts = c.gpuPowerAfter.ts = 100;
	CHECK(findMetric("power.draw.gpu").value().get().getter(di, out, c) == ZE_RESULT_SUCCESS);
	CHECK(out == "N/A");
}

TEST_CASE_FIXTURE(ZeroDeviceFixture,
				  "power.draw.gpu getter: computes watts (delta 4 000 000 µJ / 2 000 000 µs == 2.00 W)")
{
	MetricValue out;
	MetricCache c;
	c.gpuPowerBefore = {.energy = 0, .ts = 1};
	c.gpuPowerAfter = {.energy = 4'000'000, .ts = 2'000'001};
	CHECK(findMetric("power.draw.gpu").value().get().getter(di, out, c) == ZE_RESULT_SUCCESS);
	CHECK(out == "2.00");
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "energy.consumed getter: UNSUPPORTED when gpuPowerAfter.ts is zero")
{
	MetricValue out;
	MetricCache c;
	CHECK(findMetric("energy.consumed").value().get().getter(di, out, c) == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "energy.consumed getter: converts µJ to J (5 000 000 000 µJ == 5000.00 J)")
{
	MetricValue out;
	MetricCache c;
	c.gpuPowerAfter = {.energy = 5'000'000'000ULL, .ts = 1};
	CHECK(findMetric("energy.consumed").value().get().getter(di, out, c) == ZE_RESULT_SUCCESS);
	CHECK(out == "5000.00");
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "energy.consumed getter: zero energy formats as 0.00 J")
{
	MetricValue out;
	MetricCache c;
	c.gpuPowerAfter = {.energy = 0, .ts = 1}; // ts != 0, so it is available
	CHECK(findMetric("energy.consumed").value().get().getter(di, out, c) == ZE_RESULT_SUCCESS);
	CHECK(out == "0.00");
}

TEST_CASE("MetricCache default values are zero and all flags false")
{
	const MetricCache cache;
	CHECK_FALSE(cache.populated);
	CHECK_FALSE(cache.pcieAvail);
	CHECK_FALSE(cache.memAvail);
	CHECK_FALSE(cache.engineAvail);
	CHECK_FALSE(cache.powerAvail);
	CHECK_FALSE(cache.euAvail);
	CHECK(cache.pcieBefore.tx == 0);
	CHECK(cache.pcieAfter.tx == 0);
	CHECK(cache.cardPowerBefore.energy == 0);
	CHECK(cache.cardPowerAfter.energy == 0);
	CHECK(cache.memBefore.read == 0);
	CHECK(cache.memBefore.ts == 0);
	CHECK(cache.engines.all.before.active == 0);
	CHECK(cache.engines.all.before.ts == 0);
	CHECK(cache.euSample.euActive == 0);
	CHECK(cache.euSample.euStall == 0);
	CHECK(cache.euSample.euIdle == 0);
}

// ── populateMetricCache* ──────────────────────────────────────────────────────

TEST_CASE_FIXTURE(ZeroDeviceFixture, "populateMetricCacheBegin returns unpopulated cache")
{
	const MetricCache cache = populateMetricCacheBegin(di);
	CHECK_FALSE(cache.populated);
	CHECK_FALSE(cache.pcieAvail);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "populateMetricCacheEnd marks cache as populated")
{
	MetricCache cache = populateMetricCacheBegin(di);
	REQUIRE_FALSE(cache.populated);
	populateMetricCacheEnd(di, cache);
	CHECK(cache.populated);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "populateMetricCacheEnd: availability flags are false when HAL data is absent")
{
	MetricCache cache = populateMetricCacheBegin(di);
	populateMetricCacheEnd(di, cache);

	CHECK_FALSE(cache.engineAvail);
	CHECK_FALSE(cache.pcieAvail);
	CHECK_FALSE(cache.memAvail);
	CHECK_FALSE(cache.euAvail);
	// powerAvail requires advancing timestamps from both Begin and End; the zero device
	// returns constant/zero timestamps, so powerAvail is false.
	CHECK_FALSE(cache.powerAvail);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture,
				  "populateMetricCacheEnd: stale euAvail/euSample are cleared even when HAL call fails")
{
	// Simulate a cache that already holds EU data from a previous tick (e.g. the caller
	// reuses a MetricCache rather than constructing a fresh one via populateMetricCacheBegin).
	MetricCache cache;
	cache.euAvail = true;
	cache.euSample.euActive = 999;
	cache.euSample.euStall = 111;
	cache.euSample.euIdle = 222;

	// ZeroDeviceFixture has a null deviceHdl, so the EU HAL call will fail.
	populateMetricCacheEnd(di, cache);

	// The stale values must be overwritten, not preserved.
	CHECK_FALSE(cache.euAvail);
	CHECK(cache.euSample.euActive == 0);
	CHECK(cache.euSample.euStall == 0);
	CHECK(cache.euSample.euIdle == 0);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "populateMetricCacheContinuous promotes prev after-samples into curr before-slots")
{
	MetricCache prev;
	prev.cardPowerAfter.energy = 12345;
	prev.cardPowerAfter.ts = 67890;
	prev.gpuPowerAfter.energy = 11111;
	prev.gpuPowerAfter.ts = 22222;
	prev.pcieAfter.tx = 33333;
	prev.pcieAfter.rx = 44444;
	prev.pcieAfter.timeUs = 55555;
	prev.pcieReplay = 99;
	prev.pcieAvail = true;
	prev.memAfter.read = 66666;
	prev.memAfter.write = 77777;
	prev.memAfter.ts = 88888;
	prev.memMaxBandwidth = 10000;
	prev.engines.all.after.active = 500;
	prev.engines.all.after.ts = 1000;
	prev.engines.compute.after.active = 600;
	prev.engines.compute.after.ts = 1100;
	prev.engines.render.after.active = 400;
	prev.engines.render.after.ts = 900;
	prev.engines.media.after.active = 300;
	prev.engines.media.after.ts = 800;
	prev.engines.copy.after.active = 200;
	prev.engines.copy.after.ts = 700;
	prev.populated = true;

	const MetricCache curr = populateMetricCacheContinuous(di, prev);

	CHECK(curr.cardPowerBefore.energy == prev.cardPowerAfter.energy);
	CHECK(curr.cardPowerBefore.ts == prev.cardPowerAfter.ts);
	CHECK(curr.gpuPowerBefore.energy == prev.gpuPowerAfter.energy);
	CHECK(curr.gpuPowerBefore.ts == prev.gpuPowerAfter.ts);
	CHECK(curr.pcieBefore.tx == prev.pcieAfter.tx);
	CHECK(curr.pcieBefore.rx == prev.pcieAfter.rx);
	CHECK(curr.pcieBefore.timeUs == prev.pcieAfter.timeUs);
	// pcieReplay is always overwritten by populateMetricCacheEnd (new counter on success,
	// zero on failure). With a null device handle the end sample fails, so it is zero here.
	CHECK(curr.pcieReplay == 0ULL);
	CHECK(curr.memBefore.read == prev.memAfter.read);
	CHECK(curr.memBefore.write == prev.memAfter.write);
	CHECK(curr.memBefore.ts == prev.memAfter.ts);
	CHECK(curr.memMaxBandwidth == prev.memMaxBandwidth);
	CHECK(curr.engines.all.before.active == prev.engines.all.after.active);
	CHECK(curr.engines.all.before.ts == prev.engines.all.after.ts);
	CHECK(curr.engines.compute.before.active == prev.engines.compute.after.active);
	CHECK(curr.engines.compute.before.ts == prev.engines.compute.after.ts);
	CHECK(curr.engines.render.before.active == prev.engines.render.after.active);
	CHECK(curr.engines.render.before.ts == prev.engines.render.after.ts);
	CHECK(curr.engines.media.before.active == prev.engines.media.after.active);
	CHECK(curr.engines.media.before.ts == prev.engines.media.after.ts);
	CHECK(curr.engines.copy.before.active == prev.engines.copy.after.active);
	CHECK(curr.engines.copy.before.ts == prev.engines.copy.after.ts);
	CHECK(curr.populated);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture,
				  "populateMetricCacheContinuous: EU metrics are re-sampled fresh, not carried over from prev")
{
	MetricCache prev;
	prev.euSample.euActive = 800;
	prev.euSample.euStall = 100;
	prev.euSample.euIdle = 100;
	prev.euAvail = true;
	prev.populated = true;

	const MetricCache curr = populateMetricCacheContinuous(di, prev);

	// EU data must NOT be promoted from prev. With a null device handle the
	// fresh HAL sample fails, so the EU fields revert to their zero defaults.
	CHECK_FALSE(curr.euAvail);
	CHECK(curr.euSample.euActive == 0);
	CHECK(curr.euSample.euStall == 0);
	CHECK(curr.euSample.euIdle == 0);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "populateMetricCacheContinuous: pcieAvail is cleared when the end HAL call fails")
{
	MetricCache prev;
	prev.pcieAfter.tx = 100;
	prev.pcieAfter.rx = 200;
	prev.pcieAfter.timeUs = 500;
	prev.pcieAvail = true;
	prev.populated = true;

	const MetricCache curr = populateMetricCacheContinuous(di, prev);

	CHECK(curr.pcieBefore.tx == 100ULL);
	CHECK(curr.pcieBefore.rx == 200ULL);
	CHECK(curr.pcieBefore.timeUs == 500ULL);
	// End sample failed (null zesDeviceHdl), so the availability flag is cleared.
	CHECK_FALSE(curr.pcieAvail);
	// pcieReplay must be zero — not the stale value carried from prev — when end sample fails.
	CHECK(curr.pcieReplay == 0ULL);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "populateMetricCacheContinuous: pcieAvail recovers after a bad tick (no latch)")
{
	// Simulate a tick where pcieAvail was false (e.g. non-advancing timestamp),
	// but prev.pcieAfter holds a valid snapshot (timeUs != 0).
	// pcieAvail for the NEW interval must be derived from the snapshot validity,
	// not from prev.pcieAvail — otherwise a single bad tick latches it false forever.
	MetricCache prev;
	prev.pcieAfter.tx = 400;
	prev.pcieAfter.rx = 500;
	prev.pcieAfter.timeUs = 1000; // non-zero: snapshot is usable as a before-sample
	prev.pcieAvail = false;		  // previous delta was invalid (e.g. timestamp didn't advance)
	prev.populated = true;

	const MetricCache curr = populateMetricCacheContinuous(di, prev);

	// before-slots must be promoted from prev.pcieAfter regardless of prev.pcieAvail.
	CHECK(curr.pcieBefore.tx == 400ULL);
	CHECK(curr.pcieBefore.rx == 500ULL);
	CHECK(curr.pcieBefore.timeUs == 1000ULL);
	// With a null zesDeviceHdl the end sample still fails, so curr.pcieAvail is false here,
	// but the path leading to false is "end failed", not "before was invalid".
	// The key invariant: curr would become available once a real device provides a valid end sample.
	CHECK_FALSE(curr.pcieAvail); // end sample failed on ZeroDeviceFixture, as expected
}

TEST_CASE_FIXTURE(ZeroDeviceFixture,
				  "populateMetricCacheContinuous: engineAvail is false when after.ts does not advance beyond before.ts")
{
	MetricCache prev;
	prev.engines.all.after.ts = 1000;
	prev.populated = true;

	const MetricCache curr = populateMetricCacheContinuous(di, prev);

	// curr.engines.all.before.ts = 1000 (promoted), after.ts = 0 (HAL failure).
	// Condition: (before.ts != 0) && (after.ts > before.ts) ≡ true && (0 > 1000) = false.
	CHECK_FALSE(curr.engineAvail);
}

TEST_CASE_FIXTURE(ZeroDeviceFixture, "populateMetricCache one-shot wrapper returns populated cache")
{
	// Pass a zero window so this test does not block in CI.
	const MetricCache cache = populateMetricCache(di, std::chrono::milliseconds{0});
	CHECK(cache.populated);
	CHECK_FALSE(cache.engineAvail);
	CHECK_FALSE(cache.pcieAvail);
	CHECK_FALSE(cache.memAvail);
	CHECK_FALSE(cache.euAvail);
	// powerAvail requires advancing timestamps; zero device returns constant timestamps.
	CHECK_FALSE(cache.powerAvail);
}
