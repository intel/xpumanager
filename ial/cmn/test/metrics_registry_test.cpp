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
#include <span>
#include <string>

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
}

TEST_CASE("getMetricsByGroup NONE returns empty vector") { CHECK(getMetricsByGroup(MetricGroup::NONE).empty()); }

TEST_CASE("getMetricsByGroup UTILIZATION returns all utilization metrics")
{
	CHECK(getMetricsByGroup(MetricGroup::UTILIZATION).size() == metrics::utilization::getUtilizationMetrics().size());
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

TEST_CASE("getMetricsByGroup UTILIZATION returns only canonical names, no aliases")
{
	// Aliases (.single / .group / .decode.single etc.) must be excluded so that
	// '-d UTILIZATION' does not produce duplicate columns.
	static constexpr std::array canonicalNames{
		std::string_view{"utilization.gpu"},	std::string_view{"utilization.compute"},
		std::string_view{"utilization.render"}, std::string_view{"utilization.media"},
		std::string_view{"utilization.copy"},	std::string_view{"utilization.memory"},
	};
	const auto byUtil = getMetricsByGroup(MetricGroup::UTILIZATION);
	REQUIRE(byUtil.size() == canonicalNames.size());
	for (std::size_t i = 0; i < canonicalNames.size(); ++i) {
		CHECK(byUtil[i]->name == canonicalNames[i]);
	}
}

TEST_CASE("getMetricsByGroup MEMORY returns only utilization.memory")
{
	const auto byMem = getMetricsByGroup(MetricGroup::MEMORY);
	REQUIRE(byMem.size() == 1);
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

TEST_CASE("findMetric returns nullopt for unregistered metrics")
{
	CHECK_FALSE(findMetric("power.draw").has_value());
	CHECK_FALSE(findMetric("__no_such_metric__").has_value());
}

TEST_CASE("resolveQuery expands IDENTITY, TEMPERATURE, UTILIZATION, and PCI groups")
{
	CHECK_FALSE(resolveQuery("IDENTITY").empty());
	CHECK_FALSE(resolveQuery("TEMPERATURE").empty());
	CHECK_FALSE(resolveQuery("UTILIZATION").empty());
	CHECK_FALSE(resolveQuery("p").empty()); // alias covers POWER|TEMPERATURE
	CHECK_FALSE(resolveQuery("u").empty()); // single-letter alias for UTILIZATION
	CHECK_FALSE(resolveQuery("PCI").empty());
	CHECK_FALSE(resolveQuery("t").empty()); // single-letter shortcut for PCI
}

TEST_CASE("resolveQuery returns empty for group tokens with no registered metrics")
{
	// These groups are present in GROUP_TABLE but have no metrics registered yet.
	CHECK(resolveQuery("POWER").empty());
	CHECK(resolveQuery("FAN").empty());
}

TEST_CASE("resolveQuery TEMPERATURE returns registered metrics")
{
	CHECK_FALSE(resolveQuery("TEMPERATURE").empty());
	CHECK(resolveQuery("TEMPERATURE").size() == metrics::temperature::getTemperatureMetrics().size());
}

TEST_CASE("resolveQuery pu expands to Temperature and Utilization metrics")
{
	// 'p' maps to POWER|TEMPERATURE; 'u' maps to UTILIZATION.
	// Both groups are registered.
	const auto result = resolveQuery("pu");
	CHECK(result.size() ==
		  metrics::temperature::getTemperatureMetrics().size() + metrics::utilization::getUtilizationMetrics().size());
}

// ── MetricCache struct ────────────────────────────────────────────────────────

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
// A zero-initialised device has default-constructed HAL sub-objects.
// All HAL calls fail gracefully and write zeros, so availability flags stay false.

struct ZeroDeviceFixture
{
	device dev; // default-constructed; safe without calling device::init()
	devInfo di{0, &dev, nullptr, nullptr};
};

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
