/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * cross_gpu_alloc — integration test helper for xpu-smi cross-GPU phantom
 * process detection (validation/tests/cross_gpu_phantom_process.yaml).
 *
 * Root cause under test: zeInit() causes the Level Zero loader to open DRM
 * fds to ALL enumerated GPUs.  zesDeviceProcessesGetState() therefore reports
 * every zeInit'd process on every GPU — even those it never uses.  The fix in
 * oal/lin/process_platform.cpp reads /proc/<pid>/fdinfo per PID and zeros
 * memSize when no real allocation is present on a given GPU.
 *
 * This helper spawns two independent processes (parent + exec'd child) so each
 * gets a clean Level Zero state.  Parent allocates --size0 MiB on device
 * --device0; child allocates --size1 MiB on device --device1.  Both hold a
 * live immediate command list so the xe driver registers them with sysman.
 *
 * Output protocol (one "KEY=VALUE\n" per line to stdout):
 *   PARENT_PID=<pid>
 *   CHILD_PID=<pid>
 *   PARENT_DEVICE=<L0 device index>
 *   CHILD_DEVICE=<L0 device index>
 *   PARENT_ALLOC_BYTES=<n>
 *   CHILD_ALLOC_BYTES=<n>
 *   READY                         last line; harness blocks here before querying
 *
 * Build: see validation/scripts/meson.build
 *
 * Example:
 *   ./cross_gpu_alloc --device0 0 --device1 1 --size0 140 --size1 512 \
 *                     --duration 45
 */

#include "utility/compat/format.h"
#include <array>
#include <chrono>
#include <csignal>
#include <iostream>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ze_api.h>

namespace {

constexpr uint64_t MIB_TO_BYTES       = 1024ULL * 1024ULL;
constexpr uint32_t DEFAULT_DEVICE0    = 0;
constexpr uint32_t DEFAULT_DEVICE1    = 1;
constexpr uint64_t DEFAULT_SIZE0_MIB  = 140;
constexpr uint64_t DEFAULT_SIZE1_MIB  = 512;
constexpr int      DEFAULT_DURATION_S = 45;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile sig_atomic_t gStop = 0;
void onSignal([[maybe_unused]] int sig) { gStop = 1; }

void installSignalHandlers()
{
	struct sigaction sa{};
	sa.sa_handler = onSignal;
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
}

void l0Check(ze_result_t r, std::source_location loc = std::source_location::current())
{
	if (r != ZE_RESULT_SUCCESS) {
		std::cerr << xpum::compat::format("L0 error 0x{:x} at {}:{}\n",
		                                  static_cast<unsigned>(r), loc.file_name(), loc.line());
		throw std::runtime_error(xpum::compat::format("L0 error 0x{:x}", static_cast<unsigned>(r)));
	}
}

struct L0State
{
	ze_driver_handle_t       driver  = nullptr;
	ze_device_handle_t       device  = nullptr;
	ze_context_handle_t      context = nullptr;
	void                    *mem     = nullptr;
	ze_command_list_handle_t immList = nullptr;

	L0State() = default;
	~L0State()
	{
		// Destruction order matters: list → mem → context.
		if (immList  != nullptr) { zeCommandListDestroy(immList); }
		if (mem      != nullptr) { zeMemFree(context, mem); }
		if (context  != nullptr) { zeContextDestroy(context); }
	}
	L0State(const L0State &) = delete;
	L0State &operator=(const L0State &) = delete;
	L0State(L0State &&) = delete;
	L0State &operator=(L0State &&) = delete;
};

struct AllocConfig
{
	uint32_t deviceIndex;
	uint64_t bytes;
};

struct ChildConfig
{
	AllocConfig alloc;
	int syncFd;
	int duration;
};

void l0Init(L0State &s, AllocConfig cfg)
{
	l0Check(zeInit(ZE_INIT_FLAG_GPU_ONLY));

	uint32_t drvCount = 0;
	l0Check(zeDriverGet(&drvCount, nullptr));
	if (drvCount == 0) {
		throw std::runtime_error("no L0 drivers found");
	}

	std::vector<ze_driver_handle_t> drivers(drvCount);
	l0Check(zeDriverGet(&drvCount, drivers.data()));
	s.driver = drivers[0];

	uint32_t devCount = 0;
	l0Check(zeDeviceGet(s.driver, &devCount, nullptr));
	if (cfg.deviceIndex >= devCount) {
		throw std::runtime_error(
			xpum::compat::format("device index {} out of range (have {})", cfg.deviceIndex, devCount));
	}

	std::vector<ze_device_handle_t> devices(devCount);
	l0Check(zeDeviceGet(s.driver, &devCount, devices.data()));
	s.device = devices[cfg.deviceIndex];

	ze_context_desc_t ctxDesc{
		.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC,
		.pNext = nullptr,
		.flags = 0,
	};
	l0Check(zeContextCreate(s.driver, &ctxDesc, &s.context));

	ze_device_mem_alloc_desc_t memDesc{
		.stype   = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC,
		.pNext   = nullptr,
		.flags   = 0,
		.ordinal = 0,
	};
	l0Check(zeMemAllocDevice(s.context, &memDesc, cfg.bytes, 0, s.device, &s.mem));

	// Immediate command list held open for the process lifetime so the xe
	// driver keeps a live exec queue registered with sysman.
	ze_command_queue_desc_t qDesc{
		.stype    = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
		.pNext    = nullptr,
		.ordinal  = 0,
		.index    = 0,
		.flags    = 0,
		.mode     = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS,
		.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL,
	};
	l0Check(zeCommandListCreateImmediate(s.context, s.device, &qDesc, &s.immList));

	// Fill to commit physical VRAM pages; fdinfo drm-total-vram* reflects this.
	const uint8_t zero = 0;
	l0Check(zeCommandListAppendMemoryFill(s.immList, s.mem, &zero, 1, cfg.bytes, nullptr, 0, nullptr));
	l0Check(zeCommandListHostSynchronize(s.immList, UINT64_MAX));
}

void waitUntilStop(int durationSecs)
{
	for (int elapsed = 0; gStop == 0 && (durationSecs <= 0 || elapsed < durationSecs); ++elapsed) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

uint32_t probeGpuCount()
{
	if (zeInit(ZE_INIT_FLAG_GPU_ONLY) != ZE_RESULT_SUCCESS) { return 0; }
	uint32_t drvCount = 0;
	if (zeDriverGet(&drvCount, nullptr) != ZE_RESULT_SUCCESS || drvCount == 0) { return 0; }
	std::vector<ze_driver_handle_t> drivers(drvCount);
	zeDriverGet(&drvCount, drivers.data());
	uint32_t devCount = 0;
	zeDeviceGet(drivers[0], &devCount, nullptr);
	return devCount;
}

[[nodiscard]] int runChild(ChildConfig cfg)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
	prctl(PR_SET_PDEATHSIG, SIGTERM);
	try {
		L0State s;
		l0Init(s, cfg.alloc);
		if (cfg.syncFd >= 0) {
			const char ready = 'R';
			(void)write(cfg.syncFd, &ready, 1);
			close(cfg.syncFd);
		}
		waitUntilStop(cfg.duration);
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "child error: " << e.what() << "\n";
		if (cfg.syncFd >= 0) { close(cfg.syncFd); }
		return 1;
	}
}

[[nodiscard]] int runParent(const char *self, AllocConfig cfg0, AllocConfig cfg1, int duration)
{
	if (probeGpuCount() < 2) {
		std::cout << "SKIP: fewer than 2 GPUs available\n";
		std::cout.flush();
		return 0;
	}

	std::array<int, 2> pipefd{};
	if (pipe(pipefd.data()) < 0) { perror("pipe"); return 1; }

	const pid_t childPid = fork();
	if (childPid < 0) { perror("fork"); return 1; }
	if (childPid == 0) {
		close(pipefd[0]);
		// exec self with --child so the child gets a fresh Level Zero state.
		// Level Zero is not fork-safe; exec is required.
		const std::string syncFdStr = std::to_string(pipefd[1]);
		const std::string durStr    = std::to_string(duration);
		const std::string dev1Str   = std::to_string(cfg1.deviceIndex);
		const std::string sz1Str    = std::to_string(cfg1.bytes / MIB_TO_BYTES);
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
		execl(self, self,
		      "--child",
		      "--sync-fd",  syncFdStr.c_str(),
		      "--device1",  dev1Str.c_str(),
		      "--size1",    sz1Str.c_str(),
		      "--duration", durStr.c_str(),
		      nullptr);
		perror("execl");
		_exit(1);
	}
	close(pipefd[1]);

	try {
		L0State s;
		l0Init(s, cfg0);

		char buf = 0;
		const ssize_t nr = read(pipefd[0], &buf, 1);
		close(pipefd[0]);
		if (nr != 1 || buf != 'R') {
			std::cerr << xpum::compat::format(
				"child failed to send readiness byte (nr={}, buf=0x{:x})\n",
				static_cast<long>(nr), static_cast<unsigned char>(buf));
			kill(childPid, SIGTERM);
			waitpid(childPid, nullptr, 0);
			return 1;
		}

		std::cout << xpum::compat::format(
			"PARENT_PID={}\nCHILD_PID={}\n"
			"PARENT_DEVICE={}\nCHILD_DEVICE={}\n"
			"PARENT_ALLOC_BYTES={}\nCHILD_ALLOC_BYTES={}\n"
			"READY\n",
			static_cast<int>(getpid()), static_cast<int>(childPid),
			cfg0.deviceIndex, cfg1.deviceIndex, cfg0.bytes, cfg1.bytes);
		std::cout.flush();

		waitUntilStop(duration);

		kill(childPid, SIGTERM);
		waitpid(childPid, nullptr, 0);
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "parent error: " << e.what() << "\n";
		close(pipefd[0]);
		kill(childPid, SIGTERM);
		waitpid(childPid, nullptr, 0);
		return 1;
	}
}

} // namespace

int main(int argc, char **argv)
{
	installSignalHandlers();

	AllocConfig cfg0{DEFAULT_DEVICE0, DEFAULT_SIZE0_MIB * MIB_TO_BYTES};
	AllocConfig cfg1{DEFAULT_DEVICE1, DEFAULT_SIZE1_MIB * MIB_TO_BYTES};
	int duration = DEFAULT_DURATION_S;
	int syncFd   = -1;
	bool isChild = false;

	const std::span<char *> args(argv, static_cast<size_t>(argc));
	try {
		for (size_t i = 1; i < args.size(); ++i) {
			const std::string_view arg = args[i];
			auto next = [&]() -> const char * {
				if (i + 1 >= args.size()) {
					throw std::invalid_argument(xpum::compat::format("'{}' requires an argument", arg));
				}
				return args[++i];
			};

			if      (arg == "--child")    { isChild      = true; }
			else if (arg == "--sync-fd")  { syncFd       = std::stoi(next()); }
			else if (arg == "--device0")  { cfg0.deviceIndex = static_cast<uint32_t>(std::stoul(next())); }
			else if (arg == "--device1")  { cfg1.deviceIndex = static_cast<uint32_t>(std::stoul(next())); }
			else if (arg == "--size0")    { cfg0.bytes   = std::stoull(next()) * MIB_TO_BYTES; }
			else if (arg == "--size1")    { cfg1.bytes   = std::stoull(next()) * MIB_TO_BYTES; }
			else if (arg == "--duration") { duration     = std::stoi(next()); }
		}
	} catch (const std::exception &e) {
		std::cerr << "error: " << e.what() << "\n";
		return 1;
	}

	return isChild ? runChild({cfg1, syncFd, duration})
	               : runParent(args.front(), cfg0, cfg1, duration);
}
