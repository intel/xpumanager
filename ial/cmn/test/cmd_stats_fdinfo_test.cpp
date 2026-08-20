/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

/**
 * @file cmd_stats_fdinfo_test.cpp
 * @brief Unit tests for the fdinfo utilization fallback helpers in cmd_stats.cpp.
 *
 * These call the pure helpers directly rather than going through collectDeviceStats(), so
 * no GPU or Level Zero handle is needed. The TEST_CASE names state each covered behaviour.
 */

#if __has_include(<doctest/doctest.h>)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include <string>
std::string progName = "test";

#include "cmd_stats.h"

#ifdef INFO
#undef INFO
#endif

#include <algorithm>

namespace {

/// Builds a one-process, one-engine-key fdinfo utilization map: pid -> {engineKey: pct}.
fdinfo::PidUtilMap oneEngine(uint32_t pid, const std::string &engineKey, float pct)
{
	fdinfo::PidUtilMap m;
	m[pid][engineKey] = pct;
	return m;
}

/// Asserts @p trackers is exactly the one engine_0 aggregate carrying @p expected.
void checkSingleAggregate(const std::vector<EngineInstanceTracker> &trackers, double expected)
{
	REQUIRE(trackers.size() == 1);
	CHECK(trackers[0].engineIndex == 0);
	REQUIRE(trackers[0].samples.size() == 1);
	CHECK(trackers[0].samples[0] == doctest::Approx(expected));
}

} // namespace

// --- deriveEngineTrackersFromFdinfo -------------------------------------------

TEST_SUITE("cmdStats::deriveEngineTrackersFromFdinfo")
{
	TEST_CASE("empty trackers + matching fdinfo data produces one aggregated tracker")
	{
		std::vector<EngineInstanceTracker> trackers;
		const auto utilMap = oneEngine(111, "ccs", 42.0F);

		cmdStats::deriveEngineTrackersFromFdinfo(utilMap, trackers, "ccs");

		checkSingleAggregate(trackers, 42.0);
	}

	TEST_CASE("tracker already carrying a real sample is left untouched")
	{
		std::vector<EngineInstanceTracker> trackers(1);
		trackers[0].engineIndex = 3;
		trackers[0].samples.push_back(17.5);
		const auto utilMap = oneEngine(111, "ccs", 99.0F);

		cmdStats::deriveEngineTrackersFromFdinfo(utilMap, trackers, "ccs");

		REQUIRE(trackers.size() == 1);
		CHECK(trackers[0].engineIndex == 3);
		REQUIRE(trackers[0].samples.size() == 1);
		CHECK(trackers[0].samples[0] == doctest::Approx(17.5));
	}

	TEST_CASE("enumerated-but-sample-less trackers (no CAP_PERFMON) are replaced by the aggregate")
	{
		// Level Zero enumerated two engine handles but getEngineActivityByType() never
		// produced a sample for either — the exact shape sampleEngineType() leaves behind
		// without elevated privilege.
		std::vector<EngineInstanceTracker> trackers(2);
		trackers[0].engineIndex = 0;
		trackers[1].engineIndex = 1;
		const auto utilMap = oneEngine(222, "rcs", 55.0F);

		cmdStats::deriveEngineTrackersFromFdinfo(utilMap, trackers, "rcs");

		checkSingleAggregate(trackers, 55.0);
	}

	TEST_CASE("no matching fdinfo data leaves empty trackers empty")
	{
		std::vector<EngineInstanceTracker> trackers;
		const auto utilMap = oneEngine(333, "bcs", 10.0F); // does not match "vecs"

		cmdStats::deriveEngineTrackersFromFdinfo(utilMap, trackers, "vecs");

		CHECK(trackers.empty());
	}

	TEST_CASE("sum across processes is clamped to 100%")
	{
		std::vector<EngineInstanceTracker> trackers;
		fdinfo::PidUtilMap utilMap;
		utilMap[1]["ccs"] = 80.0F;
		utilMap[2]["ccs"] = 60.0F; // 140% combined, must clamp

		cmdStats::deriveEngineTrackersFromFdinfo(utilMap, trackers, "ccs");

		checkSingleAggregate(trackers, 100.0);
	}
}

// --- applyFdinfoUtilFallback ---------------------------------------------------

TEST_SUITE("cmdStats::applyFdinfoUtilFallback")
{
	TEST_CASE("fills only the per-tile series that are still empty")
	{
		DeviceMetrics metrics;
		// Level Zero already produced two real per-interval compute samples this run.
		metrics.computeUtilPerTile[0] = {12.0, 14.0};

		fdinfo::PidUtilMap utilMap;
		utilMap[1]["ccs"] = 30.0F; // compute — series already populated, must stay untouched
		utilMap[1]["rcs"] = 20.0F; // render — series empty, must be filled

		cmdStats::applyFdinfoUtilFallback(utilMap, metrics);

		CHECK(metrics.computeUtilPerTile[0] == std::vector<double>{12.0, 14.0});
		REQUIRE(metrics.renderUtilPerTile.count(0) == 1);
		REQUIRE(metrics.renderUtilPerTile[0].size() == 1);
		CHECK(metrics.renderUtilPerTile[0][0] == doctest::Approx(20.0));
	}

	TEST_CASE("gpuUtilPerTile fallback is the max across categories and independently gated")
	{
		DeviceMetrics metrics;
		fdinfo::PidUtilMap utilMap;
		utilMap[1]["ccs"] = 30.0F;
		utilMap[1]["bcs"] = 70.0F;

		cmdStats::applyFdinfoUtilFallback(utilMap, metrics);

		REQUIRE(metrics.gpuUtilPerTile.count(0) == 1);
		REQUIRE(metrics.gpuUtilPerTile[0].size() == 1);
		CHECK(metrics.gpuUtilPerTile[0][0] == doctest::Approx(70.0));
	}

	TEST_CASE("empty utilMap leaves every series untouched")
	{
		DeviceMetrics metrics;
		const fdinfo::PidUtilMap empty;

		cmdStats::applyFdinfoUtilFallback(empty, metrics);

		CHECK(metrics.computeUtilPerTile.empty());
		CHECK(metrics.renderUtilPerTile.empty());
		CHECK(metrics.copyUtilPerTile.empty());
		CHECK(metrics.mediaUtilPerTile.empty());
		CHECK(metrics.gpuUtilPerTile.empty());
	}

	TEST_CASE("per-category sum across processes is clamped to 100%")
	{
		DeviceMetrics metrics;
		fdinfo::PidUtilMap utilMap;
		utilMap[1]["ccs"] = 80.0F;
		utilMap[2]["ccs"] = 60.0F;

		cmdStats::applyFdinfoUtilFallback(utilMap, metrics);

		REQUIRE(metrics.computeUtilPerTile[0].size() == 1);
		CHECK(metrics.computeUtilPerTile[0][0] == doctest::Approx(100.0));
	}
}

// --- buildProcessUtilEntries ---------------------------------------------------

TEST_SUITE("cmdStats::buildProcessUtilEntries")
{
	TEST_CASE("builds entries from fdinfo alone when the process list is empty")
	{
		// Mirrors ZE_RESULT_ERROR_UNSUPPORTED_FEATURE from process::getState() on the
		// sysman-only path: the caller passes an empty processList rather than failing
		// the whole collection.
		const auto utilMap = oneEngine(555, "ccs", 25.0F);
		const std::vector<zes_process_state_t> emptyProcessList;

		const auto entries = cmdStats::buildProcessUtilEntries(utilMap, emptyProcessList, /*selfPid=*/999999999);

		REQUIRE(entries.size() == 1);
		CHECK(entries[0].pid == 555);
		REQUIRE(static_cast<bool>(entries[0].util.compute));
		CHECK(*entries[0].util.compute == doctest::Approx(25.0));
	}

	TEST_CASE("Level-Zero-only PIDs are merged in with disengaged (N/A) util fields")
	{
		const auto utilMap = oneEngine(111, "ccs", 25.0F);
		std::vector<zes_process_state_t> processList(1);
		processList[0].processId = 222; // not present in utilMap

		const auto entries = cmdStats::buildProcessUtilEntries(utilMap, processList, /*selfPid=*/999999999);

		REQUIRE(entries.size() == 2);
		const auto it = std::find_if(entries.begin(), entries.end(), [](const auto &e) { return e.pid == 222; });
		REQUIRE(it != entries.end());
		CHECK_FALSE(static_cast<bool>(it->util.compute));
		CHECK_FALSE(static_cast<bool>(it->util.render));
		CHECK_FALSE(static_cast<bool>(it->util.copy));
		CHECK_FALSE(static_cast<bool>(it->util.media));
	}

	TEST_CASE("a PID present in both sources is not duplicated")
	{
		const auto utilMap = oneEngine(333, "ccs", 40.0F);
		std::vector<zes_process_state_t> processList(1);
		processList[0].processId = 333;

		const auto entries = cmdStats::buildProcessUtilEntries(utilMap, processList, /*selfPid=*/999999999);

		REQUIRE(entries.size() == 1);
		CHECK(entries[0].pid == 333);
		REQUIRE(static_cast<bool>(entries[0].util.compute));
		CHECK(*entries[0].util.compute == doctest::Approx(40.0));
	}

	TEST_CASE("the caller's own PID is excluded from the Level-Zero-only merge")
	{
		const uint32_t self = 42;
		const fdinfo::PidUtilMap emptyUtilMap; // caller is expected to have erased self already
		std::vector<zes_process_state_t> processList(2);
		processList[0].processId = self;
		processList[1].processId = 777;

		const auto entries = cmdStats::buildProcessUtilEntries(emptyUtilMap, processList, self);

		REQUIRE(entries.size() == 1);
		CHECK(entries[0].pid == 777);
	}
}

#else

int main() { return 0; }

#endif
