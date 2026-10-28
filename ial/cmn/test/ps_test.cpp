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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <unistd.h>
#include <unordered_set>
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
		const EuMetricsData eu{};		// zero EU values; scaleFactor defaults to 1000
		const fdinfo::PidUtilMap empty; // no process activity — attribution is a no-op
		applyDeviceEu(std::span<psInfo>{procs.data(), 1}, std::make_optional(eu), empty);

		CHECK(procs[0].euAvailable == true);
		CHECK(procs[1].euAvailable == false); // must not be touched
	}
}

namespace {

namespace fs = std::filesystem;

// RAII wrapper for a mkdtemp-created stand-in for /proc.
struct ProcRoot
{
	fs::path path;
	ProcRoot()
	{
		char tmpl[] = "/tmp/xpusmi_pstest_XXXXXX";
		const char *p = mkdtemp(tmpl);
		REQUIRE(p != nullptr);
		path = p;
		std::ofstream(path / "uptime") << UPTIME_SEC << ".00 0.00\n";
	}
	ProcRoot(const ProcRoot &) = delete;
	ProcRoot &operator=(const ProcRoot &) = delete;
	~ProcRoot() { fs::remove_all(path); }

	// One DRM fd of @p pid on the GPU at @p pdev.
	void addClient(uint32_t pid, int fd, const std::string &pdev) const
	{
		const fs::path base = path / std::to_string(pid);
		fs::create_directories(base / "fd");
		fs::create_directories(base / "fdinfo");
		fs::create_symlink("/dev/dri/renderD128", base / "fd" / std::to_string(fd));
		std::ofstream(base / "fdinfo" / std::to_string(fd)) << "drm-driver:\txe\ndrm-pdev:\t" << pdev << "\n";
		if (!fs::exists(base / "stat")) {
			setAge(pid, DEFAULT_AGE_SEC);
		}
	}

	// /proc/<pid>/stat with a start time @p ageSec before UPTIME_SEC.
	void setAge(uint32_t pid, uint64_t ageSec) const
	{
		const fs::path base = path / std::to_string(pid);
		fs::create_directories(base);
		const auto startTicks = (UPTIME_SEC - ageSec) * static_cast<uint64_t>(sysconf(_SC_CLK_TCK));
		std::ofstream stat(base / "stat");
		stat << pid << " (mem_bench_int) S";
		for (int field = 4; field < 22; ++field) {
			stat << " 0";
		}
		stat << " " << startTicks << " 0 0\n";
	}

	static constexpr uint64_t UPTIME_SEC = 100'000;
	static constexpr uint64_t DEFAULT_AGE_SEC = 1000;
};

fdinfo::ProcessSnapshot ccsSnap(uint32_t pid, uint64_t wallNs, uint64_t ccsCycles)
{
	fdinfo::ProcessSnapshot s;
	s.pid = pid;
	s.wallNs = wallNs;
	s.engines["ccs"].cycles = ccsCycles;
	s.engines["ccs"].hasBusy = true;
	return s;
}

psInfo row(uint32_t pid, uint32_t devId, uint64_t memKiB)
{
	return psInfo{pid, "mem_bench_int", devId, ZES_ENGINE_TYPE_FLAG_COMPUTE, 0, memKiB, {}};
}

constexpr uint64_t IDLE_CTX_KIB = 20908;   // what mem_bench_int holds on each GPU it does not use
constexpr uint64_t XPUSMI_CTX_KIB = 20480; // xpu-smi's own runtime context: the reference

// The GPUs xpu-smi lists in these tests.
const std::unordered_set<std::string> GPUS{"0000:41:00.0", "0000:1a:00.0", "0000:45:00.0", "0000:03:00.0",
										   "0000:04:00.0"};

} // namespace

TEST_SUITE("cmd_ps idle runtime contexts")
{
	TEST_CASE("a multi-GPU process is hidden on the GPU it only set up, kept where it works")
	{
		const ProcRoot proc;
		proc.addClient(1183, 3, "0000:41:00.0");
		proc.addClient(1183, 4, "0000:1a:00.0");
		proc.addClient(1183, 5, "0000:45:00.0");

		// Idle GPU: setup left the counter at 7859 and it does not move.
		std::vector<psInfo> idleDev{row(1183, 1, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(idleDev, 0, {ccsSnap(1183, 100, 7859)}, {ccsSnap(1183, 200, 7859)},
									   XPUSMI_CTX_KIB, ProcessFacts{GPUS, proc.path.string()});
		CHECK(idleDev.empty());

		// The GPU it works on: the counter rises during the window.
		std::vector<psInfo> busyDev{row(1183, 0, 1632700)};
		cmdPs::dropIdleRuntimeContexts(busyDev, 0, {ccsSnap(1183, 100, 646000000)}, {ccsSnap(1183, 200, 646314451)},
									   XPUSMI_CTX_KIB, ProcessFacts{GPUS, proc.path.string()});
		CHECK(busyDev.size() == 1);
	}

	TEST_CASE("a small idle client of a single GPU stays listed")
	{
		const ProcRoot proc;
		proc.addClient(500, 3, "0000:03:00.0");
		std::vector<psInfo> procs{row(500, 0, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(procs, 0, {ccsSnap(500, 100, 42)}, {ccsSnap(500, 200, 42)}, XPUSMI_CTX_KIB,
									   ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.size() == 1);
	}

	TEST_CASE("clients on GPUs xpu-smi does not list do not make a process multi-GPU")
	{
		// One listed dGPU, plus the iGPU and another vendor's GPU a desktop process also opens.
		const ProcRoot proc;
		proc.addClient(550, 3, "0000:03:00.0");
		proc.addClient(550, 4, "0000:00:02.0");
		proc.addClient(550, 5, "0000:65:00.0");
		std::vector<psInfo> procs{row(550, 0, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(procs, 0, {ccsSnap(550, 100, 42)}, {ccsSnap(550, 200, 42)}, XPUSMI_CTX_KIB,
									   ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.size() == 1);
	}

	TEST_CASE("an idle multi-GPU process holding more than an idle context stays listed")
	{
		// e.g. a loaded model between requests
		const ProcRoot proc;
		proc.addClient(600, 3, "0000:03:00.0");
		proc.addClient(600, 4, "0000:04:00.0");
		std::vector<psInfo> procs{row(600, 0, IDLE_CONTEXT_MAX_FACTOR * XPUSMI_CTX_KIB + 1)};
		cmdPs::dropIdleRuntimeContexts(procs, 0, {ccsSnap(600, 100, 42)}, {ccsSnap(600, 200, 42)}, XPUSMI_CTX_KIB,
									   ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.size() == 1);
	}

	TEST_CASE("a small multi-GPU process that did real work before the window stays listed")
	{
		// A light workload that did not submit during the window: no delta, but its busy
		// time (646 ms) is far more than the setup work of an idle context.
		const ProcRoot proc;
		proc.addClient(750, 3, "0000:03:00.0");
		proc.addClient(750, 4, "0000:04:00.0");
		proc.setAge(750, 10); // 646 ms over 10 s: 6.5% of its life
		std::vector<psInfo> procs{row(750, 0, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(procs, 0, {ccsSnap(750, 100, 646000000)}, {ccsSnap(750, 200, 646000000)},
									   XPUSMI_CTX_KIB, ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.size() == 1);
	}

	TEST_CASE("background busy time on a GPU another process keeps busy is not work")
	{
		// mem_bench_int's client on the GPU vLLM runs on (0x674f, xe): bcs creeps up by
		// ~2400 ticks per 245 ms window while the GPU timestamp advances ~4.7M, ~0.05%.
		auto xeSnap = [](uint64_t wallNs, uint64_t bcs, uint64_t ccs, uint64_t total) {
			fdinfo::ProcessSnapshot s;
			s.pid = 856158;
			s.wallNs = wallNs;
			for (const auto &[eng, cycles] : {std::pair{"bcs", bcs}, std::pair{"ccs", ccs}}) {
				s.engines[eng].cycles = cycles;
				s.engines[eng].totalCycles = total;
				s.engines[eng].hasBusy = true;
			}
			return s;
		};
		const ProcRoot proc;
		proc.addClient(856158, 3, "0000:41:00.0");
		proc.addClient(856158, 4, "0000:45:00.0");
		proc.addClient(856158, 5, "0000:1a:00.0");
		std::vector<psInfo> procs{row(856158, 1, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(procs, 0, {xeSnap(0, 32429, 298, 437966541695)},
									   {xeSnap(245'000'000, 34824, 339, 437971279008)}, XPUSMI_CTX_KIB,
									   ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.empty());
	}

	TEST_CASE("a small multi-GPU process that worked during the window stays listed")
	{
		const ProcRoot proc;
		proc.addClient(700, 3, "0000:03:00.0");
		proc.addClient(700, 4, "0000:04:00.0");
		std::vector<psInfo> procs{row(700, 0, IDLE_CTX_KIB)};
		// 50 ms of compute in a 200 ms window: 25%.
		cmdPs::dropIdleRuntimeContexts(procs, 0, {ccsSnap(700, 0, 42)}, {ccsSnap(700, 200'000'000, 50'000'042)},
									   XPUSMI_CTX_KIB, ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.size() == 1);
	}

	TEST_CASE("a process that started during the window stays listed")
	{
		// Missing from the before snapshot: its setup falls inside the window.
		const ProcRoot proc;
		proc.addClient(850, 3, "0000:03:00.0");
		proc.addClient(850, 4, "0000:04:00.0");
		std::vector<psInfo> procs{row(850, 0, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(procs, 0, {}, {ccsSnap(850, 200, 7859)}, XPUSMI_CTX_KIB,
									   ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.size() == 1);
	}

	TEST_CASE("nothing is hidden without xpu-smi's own context as the reference")
	{
		const ProcRoot proc;
		proc.addClient(1183, 3, "0000:41:00.0");
		proc.addClient(1183, 4, "0000:1a:00.0");
		for (const std::optional<uint64_t> ref : {std::optional<uint64_t>{}, std::optional<uint64_t>{0}}) {
			std::vector<psInfo> procs{row(1183, 1, IDLE_CTX_KIB)};
			cmdPs::dropIdleRuntimeContexts(procs, 0, {ccsSnap(1183, 100, 7859)}, {ccsSnap(1183, 200, 7859)}, ref,
										   ProcessFacts{GPUS, proc.path.string()});
			CHECK(procs.size() == 1);
		}
	}

	TEST_CASE("the memory limit follows the reference, not a fixed size")
	{
		// A GPU whose runtime reserves 200 MiB: a 210 MiB idle context is still hidden.
		const ProcRoot proc;
		proc.addClient(1183, 3, "0000:41:00.0");
		proc.addClient(1183, 4, "0000:1a:00.0");
		std::vector<psInfo> procs{row(1183, 1, 210 * 1024)};
		cmdPs::dropIdleRuntimeContexts(procs, 0, {ccsSnap(1183, 100, 7859)}, {ccsSnap(1183, 200, 7859)}, 200 * 1024,
									   ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.empty());
	}

	TEST_CASE("a process fdinfo cannot see stays listed")
	{
		const ProcRoot proc; // no /proc entry: hidepid, another user, another PID namespace
		std::vector<psInfo> procs{row(800, 0, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(procs, 0, {}, {}, XPUSMI_CTX_KIB, ProcessFacts{GPUS, proc.path.string()});
		CHECK(procs.size() == 1);
	}

	TEST_CASE("one ProcessFacts walks a process's fds once for all its GPUs")
	{
		const ProcRoot proc;
		proc.addClient(1183, 3, "0000:41:00.0");
		proc.addClient(1183, 4, "0000:1a:00.0");
		const ProcessFacts counts{GPUS, proc.path.string()};
		std::vector<psInfo> dev0{row(1183, 0, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(dev0, 0, {ccsSnap(1183, 100, 7859)}, {ccsSnap(1183, 200, 7859)}, XPUSMI_CTX_KIB,
									   counts);
		CHECK(dev0.empty());

		// The second device's call must use the cached count, not walk /proc again.
		fs::remove_all(proc.path / "1183");
		std::vector<psInfo> dev1{row(1183, 1, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(dev1, 0, {ccsSnap(1183, 100, 7859)}, {ccsSnap(1183, 200, 7859)}, XPUSMI_CTX_KIB,
									   counts);
		CHECK(dev1.empty());
	}

	TEST_CASE("entries before the device's first index are left alone")
	{
		const ProcRoot proc;
		proc.addClient(1183, 3, "0000:41:00.0");
		proc.addClient(1183, 4, "0000:1a:00.0");
		// Index 0 is another device's row for the same PID; only index 1 onward is this device.
		std::vector<psInfo> procs{row(1183, 0, IDLE_CTX_KIB), row(1183, 1, IDLE_CTX_KIB)};
		cmdPs::dropIdleRuntimeContexts(procs, 1, {ccsSnap(1183, 100, 7859)}, {ccsSnap(1183, 200, 7859)}, XPUSMI_CTX_KIB,
									   ProcessFacts{GPUS, proc.path.string()});
		REQUIRE(procs.size() == 1);
		CHECK(procs[0].devId == 0);
	}
}

TEST_SUITE("processTypeFromEngines")
{
	constexpr auto COMPUTE = static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE);
	constexpr auto RENDER = static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_RENDER);
	constexpr auto THREE_D = static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_3D);
	constexpr auto MEDIA = static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_MEDIA);
	constexpr auto DMA = static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_DMA);
	constexpr auto OTHER = static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_OTHER);

	TEST_CASE("each engine kind maps to its code")
	{
		CHECK(processTypeFromEngines(COMPUTE, 0, 0) == "C");
		CHECK(processTypeFromEngines(RENDER, 0, 0) == "G");
		CHECK(processTypeFromEngines(THREE_D, 0, 0) == "G");
		CHECK(processTypeFromEngines(MEDIA, 0, 0) == "M");
		CHECK(processTypeFromEngines(DMA, 0, 0) == "DMA");
		CHECK(processTypeFromEngines(OTHER, 0, 0) == "Other");
	}

	TEST_CASE("the first matching engine wins when a process uses several")
	{
		CHECK(processTypeFromEngines(COMPUTE | RENDER | MEDIA, 0, 0) == "C");
		CHECK(processTypeFromEngines(RENDER | MEDIA, 0, 0) == "G");
		CHECK(processTypeFromEngines(MEDIA | DMA, 0, 0) == "M");
		CHECK(processTypeFromEngines(DMA | OTHER, 0, 0) == "DMA");
	}

	TEST_CASE("memory alone classifies a process as compute")
	{
		CHECK(processTypeFromEngines(0, 1, 0) == "C");
		CHECK(processTypeFromEngines(0, 0, 1) == "C");
	}

	TEST_CASE("an engine takes precedence over memory") { CHECK(processTypeFromEngines(MEDIA, 4096, 4096) == "M"); }

	TEST_CASE("no engine and no memory is not available") { CHECK(processTypeFromEngines(0, 0, 0) == "N/A"); }
}
