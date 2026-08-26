/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so debug.h (pulled in
// via enginegroup.h) can define INFO(fmt, ...) for log-level gating.
#undef INFO

#include "enginegroup.h"

#include <cstdint>
#include <map>
#include <vector>

// Tests for enginegroup::computeGpuUtilPerTile(), enginegroup::computeGroupUtilPerTile() and
// enginegroup::deviceUtilFromTiles(), which turn two engine busyness snapshots into the
// utilization figures xpu-smi reports.
//
// Level Zero aggregates an engine group by averaging its members, so the busyness the
// driver reports for ZES_ENGINE_GROUP_ALL is the mean over every engine on the device.
// computeGpuUtilPerTile must instead report the busiest engine, preferring the narrowest
// group tier a tile exposes: per-engine (*_SINGLE), then per-class (*_ALL), then ALL.
// computeGroupUtilPerTile reports one named group as the driver aggregates it, for the
// per-engine-class metrics.

namespace {

// The window every fixture below measures over, in driver clock units. The unit is
// deliberately not microseconds: the drivers report their own clock (a BMG device
// advances this counter ~19.2x faster than wall time), so only the ratio is meaningful.
constexpr uint64_t WINDOW = 1000;

/** Sample pair for one engine group that was `busyPercent` busy over WINDOW. */
void addEngine(std::vector<EngineActivitySample> &before, std::vector<EngineActivitySample> &after,
			   zes_engine_group_t type, uint32_t tileId, double busyPercent, uint64_t startActive = 0)
{
	const auto active = static_cast<uint64_t>(busyPercent / 100.0 * static_cast<double>(WINDOW));
	before.push_back({.type = type, .tileId = tileId, .activeTime = startActive, .timestamp = 0});
	after.push_back({.type = type, .tileId = tileId, .activeTime = startActive + active, .timestamp = WINDOW});
}

/** The nine engines a Battlemage GPU exposes as instance-level groups. */
void addBmgEngines(std::vector<EngineActivitySample> &before, std::vector<EngineActivitySample> &after,
				   double computeBusyPercent)
{
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, computeBusyPercent);
	addEngine(before, after, ZES_ENGINE_GROUP_RENDER_SINGLE, 0, 0.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, 0, 0.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, 0, 0.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, 0, 0.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, 0, 0.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COPY_SINGLE, 0, 0.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, 0, 0.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, 0, 0.0);
}

} // namespace

TEST_CASE("computeGpuUtilPerTile: a saturated compute engine reads 100%, not the all-engine mean")
{
	// XPUM-1483: a compute-only workload saturating one of nine engines. The device also
	// exposes ZES_ENGINE_GROUP_ALL, which reports the 11.11% average of those nine.
	std::vector<EngineActivitySample> before, after;
	addBmgEngines(before, after, 100.0);
	addEngine(before, after, ZES_ENGINE_GROUP_ALL, 0, 100.0 / 9.0);

	const auto util = enginegroup::computeGpuUtilPerTile(before, after);

	REQUIRE(util.size() == 1);
	CHECK(util.at(0) == doctest::Approx(100.0));
}

TEST_CASE("computeGpuUtilPerTile: an idle device reads 0%, which is not the same as no data")
{
	std::vector<EngineActivitySample> before, after;
	addBmgEngines(before, after, 0.0);

	const auto util = enginegroup::computeGpuUtilPerTile(before, after);

	REQUIRE(util.size() == 1);
	CHECK(util.at(0) == doctest::Approx(0.0));
}

TEST_CASE("computeGpuUtilPerTile: takes the busiest engine when several are active")
{
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 40.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COPY_SINGLE, 0, 75.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, 0, 10.0);

	const auto util = enginegroup::computeGpuUtilPerTile(before, after);

	CHECK(util.at(0) == doctest::Approx(75.0));
}

TEST_CASE("computeGpuUtilPerTile: instance groups win over the aggregated ones on the same tile")
{
	// COMPUTE_ALL/ALL are averages, so they read lower than the one busy compute engine.
	// Their lower values must not drag the result down, whatever order they arrive in.
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_ALL, 0, 5.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 45.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 90.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COPY_SINGLE, 0, 0.0);

	CHECK(enginegroup::computeGpuUtilPerTile(before, after).at(0) == doctest::Approx(90.0));

	// Same samples, reversed enumeration order: the tier preference must not depend on it.
	const std::vector<EngineActivitySample> reversedBefore(before.rbegin(), before.rend());
	const std::vector<EngineActivitySample> reversedAfter(after.rbegin(), after.rend());
	CHECK(enginegroup::computeGpuUtilPerTile(reversedBefore, reversedAfter).at(0) == doctest::Approx(90.0));
}

TEST_CASE("computeGpuUtilPerTile: falls back to the per-class groups when no instance groups exist")
{
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_ALL, 0, 12.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 60.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_ALL, 0, 30.0);

	CHECK(enginegroup::computeGpuUtilPerTile(before, after).at(0) == doctest::Approx(60.0));
}

TEST_CASE("computeGpuUtilPerTile: a class with only an aggregated group still counts")
{
	// The tier is chosen per engine class, not per tile: a driver that exposes per-engine
	// counters for compute but only the aggregate for media must still report the media
	// work, otherwise a transcode workload would read as idle.
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_ALL, 0, 80.0);

	CHECK(enginegroup::computeGpuUtilPerTile(before, after).at(0) == doctest::Approx(80.0));
}

TEST_CASE("computeGpuUtilPerTile: the device-wide average never wins over a measured class")
{
	// A pathological driver reporting ALL above every class must not raise the result:
	// ALL is a fallback for "no class was measurable", not a candidate for the maximum.
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 20.0);
	addEngine(before, after, ZES_ENGINE_GROUP_ALL, 0, 95.0);

	CHECK(enginegroup::computeGpuUtilPerTile(before, after).at(0) == doctest::Approx(20.0));
}

TEST_CASE("computeGpuUtilPerTile: falls back to ALL when it is the only group exposed")
{
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_ALL, 0, 33.0);

	CHECK(enginegroup::computeGpuUtilPerTile(before, after).at(0) == doctest::Approx(33.0));
}

TEST_CASE("computeGpuUtilPerTile: reports each tile separately")
{
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 80.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COPY_SINGLE, 0, 10.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 1, 20.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COPY_SINGLE, 1, 40.0);

	const auto util = enginegroup::computeGpuUtilPerTile(before, after);

	REQUIRE(util.size() == 2);
	CHECK(util.at(0) == doctest::Approx(80.0));
	CHECK(util.at(1) == doctest::Approx(40.0));
	// A tile that only exposes ALL still gets its own tier decision, independent of tile 0.
	CHECK(enginegroup::deviceUtilFromTiles(util) == doctest::Approx(60.0));
}

TEST_CASE("computeGpuUtilPerTile: per-tile tier choice is independent")
{
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 70.0);
	addEngine(before, after, ZES_ENGINE_GROUP_ALL, 1, 25.0);

	const auto util = enginegroup::computeGpuUtilPerTile(before, after);

	REQUIRE(util.size() == 2);
	CHECK(util.at(0) == doctest::Approx(70.0));
	CHECK(util.at(1) == doctest::Approx(25.0));
}

TEST_CASE("computeGpuUtilPerTile: empty snapshots produce no data rather than 0%")
{
	const std::vector<EngineActivitySample> empty;
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 50.0);

	CHECK(enginegroup::computeGpuUtilPerTile(empty, empty).empty());
	CHECK(enginegroup::computeGpuUtilPerTile(empty, after).empty());
	CHECK(enginegroup::computeGpuUtilPerTile(before, empty).empty());
}

TEST_CASE("computeGpuUtilPerTile: a non-advancing timestamp yields no sample for that engine")
{
	std::vector<EngineActivitySample> before{
		{.type = ZES_ENGINE_GROUP_COMPUTE_SINGLE, .tileId = 0, .activeTime = 100, .timestamp = WINDOW}};
	std::vector<EngineActivitySample> after{
		{.type = ZES_ENGINE_GROUP_COMPUTE_SINGLE, .tileId = 0, .activeTime = 200, .timestamp = WINDOW}};

	CHECK(enginegroup::computeGpuUtilPerTile(before, after).empty());

	// A stalled instance counter must not stop a usable aggregated group from being used.
	addEngine(before, after, ZES_ENGINE_GROUP_ALL, 0, 20.0);
	CHECK(enginegroup::computeGpuUtilPerTile(before, after).at(0) == doctest::Approx(20.0));
}

TEST_CASE("computeGpuUtilPerTile: a counter that went backwards reports idle, not a wrapped value")
{
	// Driver reload or GPU reset: activeTime restarts below the baseline.
	std::vector<EngineActivitySample> before{
		{.type = ZES_ENGINE_GROUP_COMPUTE_SINGLE, .tileId = 0, .activeTime = 5000, .timestamp = 0}};
	std::vector<EngineActivitySample> after{
		{.type = ZES_ENGINE_GROUP_COMPUTE_SINGLE, .tileId = 0, .activeTime = 10, .timestamp = WINDOW}};

	CHECK(enginegroup::computeGpuUtilPerTile(before, after).at(0) == doctest::Approx(0.0));
}

TEST_CASE("computeGpuUtilPerTile: busyness above the window length is clamped to 100%")
{
	// Some drivers report a group as the sum of its members, which can exceed the window.
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 400.0);

	CHECK(enginegroup::computeGpuUtilPerTile(before, after).at(0) == doctest::Approx(100.0));
}

TEST_CASE("computeGpuUtilPerTile: counters are paired by tile and type, not by position")
{
	// One engine group failed to read in the `after` snapshot, shifting every later entry.
	// Pairing by position would subtract one engine's counter from another's.
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 10.0, /*startActive=*/100'000);
	addEngine(before, after, ZES_ENGINE_GROUP_COPY_SINGLE, 0, 50.0, /*startActive=*/0);
	before.erase(before.begin()); // compute is missing from the earlier snapshot only

	const auto util = enginegroup::computeGpuUtilPerTile(before, after);

	REQUIRE(util.size() == 1);
	CHECK(util.at(0) == doctest::Approx(50.0)); // copy engine, correctly paired
}

TEST_CASE("computeGpuUtilPerTile: repeated groups of one type are paired in enumeration order")
{
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, 0, 15.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, 0, 65.0);
	// A third handle appears only in the later snapshot: it has no baseline, so it is skipped.
	after.push_back(
		{.type = ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, .tileId = 0, .activeTime = WINDOW, .timestamp = WINDOW});

	const auto util = enginegroup::computeGpuUtilPerTile(before, after);

	REQUIRE(util.size() == 1);
	CHECK(util.at(0) == doctest::Approx(65.0));
}

TEST_CASE("computeGroupUtilPerTile: reports every tile that exposes the group, not just the first")
{
	// A four-tile part enumerates one COMPUTE_ALL handle per tile. Reading only the first
	// would report tile 0's busyness as if it were the whole device.
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 80.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 1, 40.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 2, 20.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 3, 0.0);

	const auto util = enginegroup::computeGroupUtilPerTile(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL);

	REQUIRE(util.size() == 4);
	CHECK(util.at(0) == doctest::Approx(80.0));
	CHECK(util.at(1) == doctest::Approx(40.0));
	CHECK(util.at(2) == doctest::Approx(20.0));
	CHECK(util.at(3) == doctest::Approx(0.0));
	CHECK(enginegroup::deviceUtilFromTiles(util) == doctest::Approx(35.0));
}

TEST_CASE("computeGroupUtilPerTile: reports only the group asked for")
{
	// Every class shares the snapshot, so a busy compute engine must not leak into the media
	// figure, and a class the device does not expose must stay absent rather than read 0%.
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL, 0, 90.0);
	addEngine(before, after, ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 90.0);
	addEngine(before, after, ZES_ENGINE_GROUP_ALL, 0, 10.0);

	CHECK(enginegroup::computeGroupUtilPerTile(before, after, ZES_ENGINE_GROUP_COMPUTE_ALL).at(0) ==
		  doctest::Approx(90.0));
	CHECK(enginegroup::computeGroupUtilPerTile(before, after, ZES_ENGINE_GROUP_MEDIA_ALL).empty());
	CHECK(enginegroup::computeGroupUtilPerTile(before, after, ZES_ENGINE_GROUP_COPY_ALL).empty());
}

TEST_CASE("computeGroupUtilPerTile: the busiest handle wins when a tile exposes several")
{
	std::vector<EngineActivitySample> before, after;
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_ALL, 0, 30.0);
	addEngine(before, after, ZES_ENGINE_GROUP_MEDIA_ALL, 0, 65.0);

	const auto util = enginegroup::computeGroupUtilPerTile(before, after, ZES_ENGINE_GROUP_MEDIA_ALL);

	REQUIRE(util.size() == 1);
	CHECK(util.at(0) == doctest::Approx(65.0));
}

TEST_CASE("computeGroupUtilPerTile: one tile without a usable window does not hide the others")
{
	std::vector<EngineActivitySample> before{
		{.type = ZES_ENGINE_GROUP_RENDER_ALL, .tileId = 0, .activeTime = 100, .timestamp = WINDOW}};
	std::vector<EngineActivitySample> after{
		{.type = ZES_ENGINE_GROUP_RENDER_ALL, .tileId = 0, .activeTime = 900, .timestamp = WINDOW}};
	addEngine(before, after, ZES_ENGINE_GROUP_RENDER_ALL, 1, 50.0);

	const auto util = enginegroup::computeGroupUtilPerTile(before, after, ZES_ENGINE_GROUP_RENDER_ALL);

	REQUIRE(util.size() == 1);
	CHECK(util.at(1) == doctest::Approx(50.0));
}

TEST_CASE("deviceUtilFromTiles: averages the tiles and reports nullopt for no data")
{
	CHECK_FALSE(enginegroup::deviceUtilFromTiles({}).has_value());
	CHECK(enginegroup::deviceUtilFromTiles({{0, 100.0}}) == doctest::Approx(100.0));
	CHECK(enginegroup::deviceUtilFromTiles({{0, 100.0}, {1, 0.0}}) == doctest::Approx(50.0));
	CHECK(enginegroup::deviceUtilFromTiles({{0, 30.0}, {1, 60.0}, {2, 90.0}}) == doctest::Approx(60.0));
	// 0% is a value, not an absence of one.
	CHECK(enginegroup::deviceUtilFromTiles({{0, 0.0}}).has_value());
}
