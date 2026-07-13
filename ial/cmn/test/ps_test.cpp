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
