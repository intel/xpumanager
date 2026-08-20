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

#include <string>
std::string progName = "test";

#include "cmd_ps.h"
#include "os.h"
#include <nlohmann/json.hpp>
#include <span>
#include <unistd.h>
#include <vector>

#ifdef INFO
#undef INFO
#endif

TEST_SUITE("cmd_ps self-exclusion")
{
	TEST_CASE("getCurrentProcessId matches the OS process id")
	{
		CHECK(getCurrentProcessId() == static_cast<uint32_t>(getpid()));
	}

	TEST_CASE("buildPsInfoList drops the calling process and keeps the rest")
	{
		const uint32_t self = getCurrentProcessId();

		std::vector<zes_process_state_t> procs(3);
		procs[0].processId = self; // xpu-smi itself -> must be excluded
		procs[1].processId = 424242;
		procs[1].memSize = 2048;	// bytes -> 2 KiB
		procs[1].sharedSize = 1024; // bytes -> 1 KiB
		procs[2].processId = 525252;

		std::vector<psInfo> out;
		cmdPs::buildPsInfoList(out, procs, /*devIndex=*/2, self);

		REQUIRE(out.size() == 2);
		for (const auto &e : out) {
			CHECK(e.processId != self);
			CHECK(e.devId == 2);
		}
		CHECK(out[0].processId == 424242);
		CHECK(out[0].memSize == 2);
		CHECK(out[0].sharedSize == 1);
		CHECK(out[1].processId == 525252);
	}

	TEST_CASE("buildPsInfoList keeps every process when none is the caller")
	{
		std::vector<zes_process_state_t> procs(2);
		procs[0].processId = 111111;
		procs[1].processId = 222222;

		std::vector<psInfo> out;
		cmdPs::buildPsInfoList(out, procs, /*devIndex=*/0, /*selfPid=*/999999999);

		CHECK(out.size() == 2);
	}

	TEST_CASE("buildPsInfoList excludes the caller on every device")
	{
		const uint32_t self = getCurrentProcessId();

		std::vector<psInfo> out;
		for (uint32_t dev = 0; dev < 4; ++dev) {
			std::vector<zes_process_state_t> procs(1);
			procs[0].processId = self;
			cmdPs::buildPsInfoList(out, procs, dev, self);
		}

		CHECK(out.empty());
	}
}

// ── Regression: multi-device accumulation ────────────────────────────────────
//
// When iterating over multiple devices, a device that does not support process
// enumeration (e.g. a sysman-only device on the DEPENDENCY_UNAVAILABLE path)
// must not suppress processes already found on other devices.
//
// Previously, the ps run() loop overwrote a single `result` variable on every
// iteration.  If the last device returned ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
// the command exited before printing the psInfoList that had already been
// populated from earlier devices.

TEST_SUITE("cmd_ps multi-device accumulation")
{
	TEST_CASE("processes from device 0 survive when device 1 contributes nothing")
	{
		// Device 0: one real process.
		std::vector<zes_process_state_t> dev0procs(1);
		dev0procs[0].processId = 42000;
		dev0procs[0].memSize = 4096;

		// Device 1: empty — simulates UNSUPPORTED_FEATURE on the sysman-only
		// path where zesDeviceProcessesGetState returns that error.
		std::vector<zes_process_state_t> dev1procs;

		std::vector<psInfo> out;
		cmdPs::buildPsInfoList(out, dev0procs, /*devIndex=*/0, /*selfPid=*/999);
		cmdPs::buildPsInfoList(out, dev1procs, /*devIndex=*/1, /*selfPid=*/999);

		REQUIRE(out.size() == 1);
		CHECK(out[0].processId == 42000);
		CHECK(out[0].devId == 0);
	}

	TEST_CASE("processes from multiple devices are accumulated in order")
	{
		std::vector<zes_process_state_t> dev0procs(1);
		dev0procs[0].processId = 10001;
		std::vector<zes_process_state_t> dev1procs(1);
		dev1procs[0].processId = 10002;

		std::vector<psInfo> out;
		cmdPs::buildPsInfoList(out, dev0procs, 0, 999);
		cmdPs::buildPsInfoList(out, dev1procs, 1, 999);

		REQUIRE(out.size() == 2);
		CHECK(out[0].devId == 0);
		CHECK(out[1].devId == 1);
	}

	// Verify the error-classification predicate used in run():
	// ZE_RESULT_ERROR_UNSUPPORTED_FEATURE must not be recorded as a fatal error;
	// any other non-SUCCESS code must be.
	TEST_CASE("UNSUPPORTED_FEATURE is not treated as a fatal enumeration error")
	{
		CHECK_FALSE(isFatalPsError(ZE_RESULT_SUCCESS));
		CHECK_FALSE(isFatalPsError(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE));
		CHECK(isFatalPsError(ZE_RESULT_ERROR_DEVICE_LOST));
		CHECK(isFatalPsError(ZE_RESULT_ERROR_UNKNOWN));
	}
}

// ── eu_available JSON field ───────────────────────────────────────────────────
//
// EU columns (EUAct%, EUStl%) should appear in the table whenever EU metrics
// were successfully collected for any device, even when no process received
// non-zero attribution (e.g. during an idle sample).  The eu_available field
// in the JSON drives this decision; it is set independently per device so that
// a device without CAP_PERFMON does not suppress EU columns for a device that
// does have it.

TEST_SUITE("cmd_ps eu_available field")
{
	TEST_CASE("psInfo with euAvailable=true emits eu_available:true in JSON")
	{
		psInfo p{};
		p.processId = 42;
		p.commandName = "workload";
		p.euAvailable = true;
		// EU attribution values are null — idle sample, but collection succeeded.

		nlohmann::ordered_json j;
		to_json(j, p);

		REQUIRE(j.contains("eu_available"));
		CHECK(j["eu_available"].get<bool>() == true);
		CHECK(j["eu_active_pct"].is_null()); // null attribution is fine
	}

	TEST_CASE("psInfo with euAvailable=false emits eu_available:false in JSON")
	{
		psInfo p{};
		p.processId = 43;
		p.commandName = "copy_proc";
		p.euAvailable = false;

		nlohmann::ordered_json j;
		to_json(j, p);

		CHECK(j["eu_available"].get<bool>() == false);
	}

	// Regression: eu_available must be set per device's span so that a process
	// appearing on both device 0 (EU collected) and device 1 (no EU) does not
	// have euAvailable incorrectly set on device 1's row.
	TEST_CASE("eu_available is set only within the bounded device span")
	{
		std::vector<psInfo> procs(2);
		procs[0].devId = 0;
		procs[1].devId = 1;

		// Call the production helper with EU data for device 0 only.
		// An unbounded subspan regression would set procs[1].euAvailable.
		const EuMetricsData eu{};       // zero EU values; scaleFactor defaults to 1000
		const fdinfo::PidUtilMap empty; // no process activity — attribution is a no-op
		applyDeviceEu(std::span<psInfo>{procs.data(), 1}, std::make_optional(eu), empty);

		CHECK(procs[0].euAvailable == true);
		CHECK(procs[1].euAvailable == false); // must not be touched
	}
}
