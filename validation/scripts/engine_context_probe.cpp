/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * engine_context_probe.cpp - Integration test helper for xpu-smi's
 * engine-context process filter (validation/tests/engine_context_filter.yaml).
 *
 * What is under test
 * ------------------
 * zeInit() opens a DRM fd to every GPU on the system and the driver makes small
 * internal allocations there, so holding VRAM on a device does not make a
 * process a client of it.  xpu-smi therefore lists a process only where it has
 * actually executed work, which fdinfo reports as a non-zero drm-cycles-<eng>
 * (xe) or drm-engine-<eng> (i915) counter.
 *
 * Presence of those keys proves nothing: both drivers emit one line per engine
 * class the *hardware* exposes, for every DRM client.  A bare open() of the
 * render node already lists rcs/ccs/bcs/vcs/vecs at zero.  Only the value
 * separates a real client from a zeInit artifact, and because the counters are
 * cumulative for the client's lifetime it stays non-zero for a real workload
 * that is idle at the moment xpu-smi reads it.
 *
 * How this isolates that
 * ----------------------
 * Two independent processes allocate the same amount of VRAM on the *same*
 * device and differ in one respect only:
 *
 *   submitter  allocates, then submits a 4-byte fill  -> drm-cycles-ccs > 0
 *   idler      allocates and submits nothing          -> every counter stays 0
 *
 * xpu-smi must list the submitter and omit the idler.  Needing just one GPU,
 * this is reproducible anywhere, unlike the cross-GPU phantom that motivated
 * the filter (see cross_gpu_alloc.cpp, which needs two).
 *
 * Level Zero is not fork-safe, so the second process is exec'd rather than
 * forked, giving each a clean driver state.
 *
 * Linux only
 * ----------
 * The filter under test reads /proc/<pid>/fdinfo, and this helper reads the
 * same files and uses fork/exec/prctl to run the idler, so it is built only on
 * Linux (the top-level meson.build skips validation/ elsewhere) and
 * engine_context_filter.yaml gates its test on os: "Linux".  Windows has
 * nothing to exercise: Level Zero returns ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 * from zesDeviceProcessesGetState() there, so there is no process list for the
 * filter to act on.
 *
 * Why RAW_SYSMAN_* matters
 * ------------------------
 * "idler absent from xpu-smi" is only evidence of filtering if the driver
 * reported it in the first place.  The parent therefore also calls
 * zesDeviceProcessesGetState() directly and reports whether each PID appears in
 * the unfiltered list.  RAW_SYSMAN_IDLER=yes means the driver offered the entry
 * and xpu-smi dropped it, which is the result the test is looking for.
 * RAW_SYSMAN_IDLER=no means the driver never offered it, so an absence in
 * xpu-smi proves nothing and the check must not claim a pass.
 *
 * Output protocol (one "KEY=VALUE\n" per line to stdout, parent only):
 *   MODE=<mode>
 *   DEVICE=<L0 device index>
 *   BDF=<pci address>
 *   ALLOC_BYTES=<n>
 *   SUBMITTER_PID=<pid>            modes both, submit
 *   SUBMITTER_VRAM_KIB=<n>
 *   SUBMITTER_CYCLES=<n>           summed over every engine; expected > 0
 *   IDLER_PID=<pid>                modes both, idle
 *   IDLER_VRAM_KIB=<n>
 *   IDLER_CYCLES=<n>               expected 0
 *   RAW_SYSMAN_SUBMITTER=yes|no|unknown
 *   RAW_SYSMAN_IDLER=yes|no|unknown
 *   READY                          last line; harness blocks here before querying
 *
 * Modes:
 *   both    (default) exec an idler, become the submitter, report on both
 *   submit  this process only: allocate and submit work
 *   idle    this process only: allocate and submit nothing
 *
 * Runs until SIGINT/SIGTERM or --duration <secs> elapses (0 = forever).
 *
 * Example - 128 MiB each on device 0, 45-second window:
 *   ./engine_context_probe --size 128 --device 0 --duration 45
 *
 * Expected xpu-smi behaviour:
 *   xpu-smi ps  lists SUBMITTER_PID and does not list IDLER_PID
 */

#include "utility/compat/format.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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
#include <zes_api.h>

namespace fs = std::filesystem;
using namespace std::string_view_literals;

namespace {

// -- constants ---------------------------------------------------------------

constexpr uint64_t MIB_TO_BYTES = 1024ULL * 1024ULL;
constexpr uint64_t DEFAULT_SIZE_MIB = 128;
constexpr int DEFAULT_DURATION_S = 45;
constexpr int READY_WAIT_DECISECONDS = 150; // 15 s

// -- signal handling ---------------------------------------------------------

volatile sig_atomic_t gStop = 0;
void onSignal([[maybe_unused]] int sig) { gStop = 1; }

void installSignalHandlers()
{
	struct sigaction sa
	{};
	sa.sa_handler = onSignal;
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
}

void waitUntilStop(int durationSecs)
{
	for (int elapsed = 0; gStop == 0 && (durationSecs <= 0 || elapsed < durationSecs); ++elapsed) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

// -- Level Zero helpers ------------------------------------------------------

void l0Check(ze_result_t r, std::source_location loc = std::source_location::current())
{
	if (r != ZE_RESULT_SUCCESS) {
		std::cerr << xpum::compat::format("L0 error 0x{:x} at {}:{}\n", static_cast<unsigned>(r), loc.file_name(),
										  loc.line());
		throw std::runtime_error(xpum::compat::format("L0 error 0x{:x}", static_cast<unsigned>(r)));
	}
}

/** Owns a Level Zero context, allocation and optional command list. */
struct L0State
{
	ze_driver_handle_t driver = nullptr;
	ze_device_handle_t device = nullptr;
	ze_context_handle_t context = nullptr;
	void *mem = nullptr;
	ze_command_list_handle_t immList = nullptr;

	L0State() = default;
	~L0State()
	{
		// Destruction order matters: list -> mem -> context.
		if (immList != nullptr) {
			zeCommandListDestroy(immList);
		}
		if (mem != nullptr) {
			zeMemFree(context, mem);
		}
		if (context != nullptr) {
			zeContextDestroy(context);
		}
	}
	L0State(const L0State &) = delete;
	L0State &operator=(const L0State &) = delete;
	L0State(L0State &&) = delete;
	L0State &operator=(L0State &&) = delete;
};

/** Brings up Level Zero and allocates @p bytes of device VRAM. Submits nothing. */
void l0InitAndAlloc(L0State &s, uint32_t deviceIndex, uint64_t bytes)
{
	l0Check(zeInit(ZE_INIT_FLAG_GPU_ONLY));

	uint32_t driverCount = 0;
	l0Check(zeDriverGet(&driverCount, nullptr));
	if (driverCount == 0) {
		throw std::runtime_error("no L0 drivers found");
	}
	std::vector<ze_driver_handle_t> drivers(driverCount);
	l0Check(zeDriverGet(&driverCount, drivers.data()));
	s.driver = drivers[0];

	uint32_t deviceCount = 0;
	l0Check(zeDeviceGet(s.driver, &deviceCount, nullptr));
	if (deviceIndex >= deviceCount) {
		throw std::runtime_error(
			xpum::compat::format("device index {} out of range (have {})", deviceIndex, deviceCount));
	}
	std::vector<ze_device_handle_t> devices(deviceCount);
	l0Check(zeDeviceGet(s.driver, &deviceCount, devices.data()));
	s.device = devices[deviceIndex];

	ze_context_desc_t ctxDesc{};
	ctxDesc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
	l0Check(zeContextCreate(s.driver, &ctxDesc, &s.context));

	ze_device_mem_alloc_desc_t memDesc{};
	memDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
	l0Check(zeMemAllocDevice(s.context, &memDesc, bytes, 1, s.device, &s.mem));
}

/**
 * Submits a 4-byte fill, which is the smallest operation that moves
 * drm-cycles-<eng> off zero.  The immediate list is kept open for the process
 * lifetime so an exec queue stays registered, matching a real workload.
 */
void l0SubmitWork(L0State &s, uint64_t bytes)
{
	ze_command_queue_desc_t qDesc{};
	qDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
	qDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
	l0Check(zeCommandListCreateImmediate(s.context, s.device, &qDesc, &s.immList));

	uint32_t pattern = 0;
	const size_t fillBytes = std::min<uint64_t>(sizeof(pattern), bytes);
	l0Check(zeCommandListAppendMemoryFill(s.immList, s.mem, &pattern, sizeof(pattern), fillBytes, nullptr, 0, nullptr));
	l0Check(zeCommandListHostSynchronize(s.immList, UINT64_MAX));
}

/** PCI address of a ze device, formatted to match fdinfo's drm-pdev. */
[[nodiscard]] std::string deviceBdf(ze_device_handle_t device)
{
	ze_pci_ext_properties_t pci{};
	pci.stype = ZE_STRUCTURE_TYPE_PCI_EXT_PROPERTIES;
	l0Check(zeDevicePciGetPropertiesExt(device, &pci));
	return xpum::compat::format("{:04x}:{:02x}:{:02x}.{:x}", pci.address.domain, pci.address.bus, pci.address.device,
								pci.address.function);
}

// -- fdinfo scanning ---------------------------------------------------------

struct FdinfoTotals
{
	uint64_t vramKiB{0};
	uint64_t cycles{0}; // summed across every engine class
	bool found{false};	// at least one DRM fd for the target device
};

/** Accumulates one "key:\tvalue" fdinfo line into @p totals. */
void accumulateLine(std::string_view line, FdinfoTotals &totals)
{
	static constexpr auto vramPfx = "drm-total-vram"sv;
	static constexpr auto cycPfx = "drm-cycles-"sv;
	static constexpr auto engPfx = "drm-engine-"sv;
	static constexpr auto capPfx = "drm-engine-capacity-"sv;

	const size_t colon = line.find(':');
	if (colon == std::string_view::npos) {
		return;
	}
	const size_t pos = line.find_first_not_of(" \t", colon + 1);
	if (pos == std::string_view::npos) {
		return;
	}
	uint64_t value = 0;
	const std::string_view digits = line.substr(pos);
	// from_chars stops at the unit suffix ("65536 KiB", "12345 ns").
	if (std::from_chars(digits.data(), digits.data() + digits.size(), value).ec != std::errc{}) {
		return;
	}

	if (line.starts_with(vramPfx)) {
		totals.vramKiB += value;
	} else if (line.starts_with(cycPfx) || (line.starts_with(engPfx) && !line.starts_with(capPfx))) {
		totals.cycles += value;
	}
}

/**
 * Sums VRAM and engine counters across every /proc/<pid>/fdinfo entry whose
 * drm-pdev matches @p bdf.  A pid with no such fd yields found == false.
 */
[[nodiscard]] FdinfoTotals scanFdinfo(uint32_t pid, const std::string &bdf)
{
	FdinfoTotals totals;
	std::error_code ec;
	const fs::path fdDir = fs::path{"/proc"} / std::to_string(pid) / "fd";
	const fs::path fdinfoDir = fs::path{"/proc"} / std::to_string(pid) / "fdinfo";

	fs::directory_iterator it{fdDir, ec};
	if (ec) {
		return totals;
	}
	for (; it != fs::directory_iterator{}; it.increment(ec)) {
		if (ec) {
			break;
		}
		const fs::path target = fs::read_symlink(it->path(), ec);
		if (ec) {
			ec.clear();
			continue;
		}
		if (target.parent_path() != "/dev/dri") {
			continue;
		}

		std::ifstream fin(fdinfoDir / it->path().filename());
		if (!fin) {
			continue;
		}
		// Buffer the file so drm-pdev can gate the counters regardless of line order.
		std::vector<std::string> lines;
		std::string line;
		bool matchesDevice = false;
		while (std::getline(fin, line)) {
			if (line.starts_with("drm-pdev"sv) && line.find(bdf) != std::string::npos) {
				matchesDevice = true;
			}
			lines.push_back(line);
		}
		if (!matchesDevice) {
			continue;
		}
		totals.found = true;
		for (const auto &l : lines) {
			accumulateLine(l, totals);
		}
	}
	return totals;
}

// -- raw sysman cross-check --------------------------------------------------

/**
 * PIDs that zesDeviceProcessesGetState() reports for @p bdf, with no filtering.
 * Returns nullopt when sysman is unavailable, so the caller can report "unknown"
 * instead of claiming the driver omitted a process.
 */
[[nodiscard]] std::optional<std::vector<uint32_t>> rawSysmanPids(const std::string &bdf)
{
	if (zesInit(ZE_INIT_FLAG_GPU_ONLY) != ZE_RESULT_SUCCESS) {
		return std::nullopt;
	}
	uint32_t driverCount = 0;
	if (zesDriverGet(&driverCount, nullptr) != ZE_RESULT_SUCCESS || driverCount == 0) {
		return std::nullopt;
	}
	std::vector<zes_driver_handle_t> drivers(driverCount);
	if (zesDriverGet(&driverCount, drivers.data()) != ZE_RESULT_SUCCESS) {
		return std::nullopt;
	}

	for (auto drv : drivers) {
		uint32_t deviceCount = 0;
		if (zesDeviceGet(drv, &deviceCount, nullptr) != ZE_RESULT_SUCCESS || deviceCount == 0) {
			continue;
		}
		std::vector<zes_device_handle_t> devices(deviceCount);
		if (zesDeviceGet(drv, &deviceCount, devices.data()) != ZE_RESULT_SUCCESS) {
			continue;
		}
		for (auto dev : devices) {
			zes_pci_properties_t pci{};
			pci.stype = ZES_STRUCTURE_TYPE_PCI_PROPERTIES;
			if (zesDevicePciGetProperties(dev, &pci) != ZE_RESULT_SUCCESS) {
				continue;
			}
			const std::string devBdf = xpum::compat::format("{:04x}:{:02x}:{:02x}.{:x}", pci.address.domain,
															pci.address.bus, pci.address.device, pci.address.function);
			if (devBdf != bdf) {
				continue;
			}

			uint32_t count = 0;
			if (zesDeviceProcessesGetState(dev, &count, nullptr) != ZE_RESULT_SUCCESS) {
				return std::nullopt;
			}
			std::vector<zes_process_state_t> procs(count);
			for (auto &p : procs) {
				p.stype = ZES_STRUCTURE_TYPE_PROCESS_STATE;
			}
			if (count > 0 && zesDeviceProcessesGetState(dev, &count, procs.data()) != ZE_RESULT_SUCCESS) {
				return std::nullopt;
			}
			std::vector<uint32_t> pids;
			pids.reserve(count);
			for (uint32_t i = 0; i < count; ++i) {
				pids.push_back(procs[i].processId);
			}
			return pids;
		}
	}
	return std::nullopt;
}

[[nodiscard]] std::string_view sysmanVerdict(const std::optional<std::vector<uint32_t>> &pids, uint32_t pid)
{
	if (!pids) {
		return "unknown"sv;
	}
	return std::ranges::find(*pids, pid) != pids->end() ? "yes"sv : "no"sv;
}

// -- reporting ---------------------------------------------------------------

void reportRole(std::string_view label, uint32_t pid, const FdinfoTotals &totals)
{
	std::cout << xpum::compat::format("{}_PID={}\n{}_VRAM_KIB={}\n{}_CYCLES={}\n", label, pid, label, totals.vramKiB,
									  label, totals.cycles);
	if (!totals.found) {
		std::cerr << xpum::compat::format("warning: no DRM fd for the target device found in /proc/{}/fdinfo\n", pid);
	}
}

// -- roles -------------------------------------------------------------------

enum class Mode : uint8_t
{
	Both,
	Submit,
	Idle,
};

struct Config
{
	Mode mode{Mode::Both};
	uint32_t deviceIndex{0};
	uint64_t bytes{DEFAULT_SIZE_MIB * MIB_TO_BYTES};
	int duration{DEFAULT_DURATION_S};
	int syncFd{-1};
};

/** The exec'd child: allocate, signal readiness, submit nothing, hold. */
[[nodiscard]] int runIdlerChild(const Config &cfg)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
	prctl(PR_SET_PDEATHSIG, SIGTERM);
	try {
		L0State s;
		l0InitAndAlloc(s, cfg.deviceIndex, cfg.bytes);
		if (cfg.syncFd >= 0) {
			const char ready = 'R';
			(void)write(cfg.syncFd, &ready, 1);
			close(cfg.syncFd);
		}
		waitUntilStop(cfg.duration);
		return 0;
	} catch (const std::exception &e) {
		std::cerr << xpum::compat::format("idler error: {}\n", e.what());
		if (cfg.syncFd >= 0) {
			close(cfg.syncFd);
		}
		return 1;
	}
}

/** Single-role run, for manual use via --mode submit / --mode idle. */
[[nodiscard]] int runSingle(const Config &cfg)
{
	const bool submitting = cfg.mode == Mode::Submit;
	try {
		L0State s;
		l0InitAndAlloc(s, cfg.deviceIndex, cfg.bytes);
		if (submitting) {
			l0SubmitWork(s, cfg.bytes);
		}
		const std::string bdf = deviceBdf(s.device);
		const auto self = static_cast<uint32_t>(getpid());

		std::cout << xpum::compat::format("MODE={}\nDEVICE={}\nBDF={}\nALLOC_BYTES={}\n",
										  submitting ? "submit" : "idle", cfg.deviceIndex, bdf, cfg.bytes);
		reportRole(submitting ? "SUBMITTER" : "IDLER", self, scanFdinfo(self, bdf));
		const auto raw = rawSysmanPids(bdf);
		std::cout << xpum::compat::format("RAW_SYSMAN_{}={}\n", submitting ? "SUBMITTER" : "IDLER",
										  sysmanVerdict(raw, self));
		std::cout << "READY\n";
		std::cout.flush();

		waitUntilStop(cfg.duration);
		return 0;
	} catch (const std::exception &e) {
		std::cerr << xpum::compat::format("error: {}\n", e.what());
		return 1;
	}
}

/** Default run: exec an idler sibling, become the submitter, report on both. */
[[nodiscard]] int runBoth(const char *self, const Config &cfg)
{
	std::array<int, 2> pipefd{};
	if (pipe(pipefd.data()) < 0) {
		perror("pipe");
		return 1;
	}

	const pid_t childPid = fork();
	if (childPid < 0) {
		perror("fork");
		return 1;
	}
	if (childPid == 0) {
		close(pipefd[0]);
		// exec rather than fork alone: Level Zero is not fork-safe, and the idler
		// needs a driver state of its own with no inherited submission history.
		const std::string syncFdStr = std::to_string(pipefd[1]);
		const std::string devStr = std::to_string(cfg.deviceIndex);
		const std::string sizeStr = std::to_string(cfg.bytes / MIB_TO_BYTES);
		const std::string durStr = std::to_string(cfg.duration);
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
		execl(self, self, "--role-idler-child", "--sync-fd", syncFdStr.c_str(), "--device", devStr.c_str(), "--size",
			  sizeStr.c_str(), "--duration", durStr.c_str(), nullptr);
		perror("execl");
		_exit(1);
	}
	close(pipefd[1]);

	const auto reap = [childPid] {
		kill(childPid, SIGTERM);
		waitpid(childPid, nullptr, 0);
	};

	try {
		L0State s;
		l0InitAndAlloc(s, cfg.deviceIndex, cfg.bytes);
		l0SubmitWork(s, cfg.bytes);

		char buf = 0;
		const ssize_t nr = read(pipefd[0], &buf, 1);
		close(pipefd[0]);
		if (nr != 1 || buf != 'R') {
			std::cerr << xpum::compat::format("idler failed to signal readiness (nr={}, buf=0x{:x})\n",
											  static_cast<long>(nr), static_cast<unsigned char>(buf));
			reap();
			return 1;
		}

		const std::string bdf = deviceBdf(s.device);
		const auto submitterPid = static_cast<uint32_t>(getpid());
		const auto idlerPid = static_cast<uint32_t>(childPid);

		std::cout << xpum::compat::format("MODE=both\nDEVICE={}\nBDF={}\nALLOC_BYTES={}\n", cfg.deviceIndex, bdf,
										  cfg.bytes);
		reportRole("SUBMITTER", submitterPid, scanFdinfo(submitterPid, bdf));
		reportRole("IDLER", idlerPid, scanFdinfo(idlerPid, bdf));

		// One query, both verdicts: the list must be sampled while both processes
		// are alive or the comparison is meaningless.
		const auto raw = rawSysmanPids(bdf);
		std::cout << xpum::compat::format("RAW_SYSMAN_SUBMITTER={}\nRAW_SYSMAN_IDLER={}\n",
										  sysmanVerdict(raw, submitterPid), sysmanVerdict(raw, idlerPid));
		std::cout << "READY\n";
		std::cout.flush();

		waitUntilStop(cfg.duration);
		reap();
		return 0;
	} catch (const std::exception &e) {
		std::cerr << xpum::compat::format("submitter error: {}\n", e.what());
		close(pipefd[0]);
		reap();
		return 1;
	}
}

void usage(const char *prog)
{
	std::cerr << xpum::compat::format(
		"Usage: {} [options]\n"
		"  --mode     both|submit|idle  which roles to run (default: both)\n"
		"  --size     <MiB>             device VRAM each role allocates (default: {})\n"
		"  --device   <index>           L0 device index (default: 0)\n"
		"  --duration <secs>            hold time; 0 = until SIGINT/SIGTERM (default: {})\n"
		"\n"
		"Both roles allocate the same VRAM on the same device.  Only the submitter\n"
		"runs a GPU command, so only its drm-cycles-<eng> counters leave zero.\n"
		"\n"
		"Expected xpu-smi behaviour:\n"
		"  xpu-smi ps  lists SUBMITTER_PID\n"
		"  xpu-smi ps  does NOT list IDLER_PID\n"
		"\n"
		"RAW_SYSMAN_IDLER=yes confirms the driver did offer the idler, so its\n"
		"absence from xpu-smi is the filter working rather than the driver\n"
		"hiding it.  RAW_SYSMAN_IDLER=no makes the comparison inconclusive.\n",
		prog, DEFAULT_SIZE_MIB, DEFAULT_DURATION_S);
}

} // namespace

int main(int argc, char **argv)
{
	installSignalHandlers();

	Config cfg;
	bool idlerChild = false;
	const std::span<char *> args(argv, static_cast<size_t>(argc));

	try {
		for (size_t i = 1; i < args.size(); ++i) {
			const std::string_view arg = args[i];
			auto nextArg = [&]() -> const char * {
				if (i + 1 >= args.size()) {
					throw std::invalid_argument(xpum::compat::format("'{}' requires an argument", arg));
				}
				return args[++i];
			};

			if (arg == "--mode") {
				const std::string_view m = nextArg();
				if (m == "both") {
					cfg.mode = Mode::Both;
				} else if (m == "submit") {
					cfg.mode = Mode::Submit;
				} else if (m == "idle") {
					cfg.mode = Mode::Idle;
				} else {
					throw std::invalid_argument(xpum::compat::format("unknown mode '{}'", m));
				}
			} else if (arg == "--size") {
				cfg.bytes = std::stoull(nextArg()) * MIB_TO_BYTES;
			} else if (arg == "--device") {
				cfg.deviceIndex = static_cast<uint32_t>(std::stoul(nextArg()));
			} else if (arg == "--duration") {
				cfg.duration = std::stoi(nextArg());
			} else if (arg == "--role-idler-child") {
				// Internal: set by runBoth's execl, not part of the documented CLI.
				idlerChild = true;
			} else if (arg == "--sync-fd") {
				cfg.syncFd = std::stoi(nextArg());
			} else if (arg == "--help" || arg == "-h") {
				usage(args[0]);
				return 0;
			} else {
				throw std::invalid_argument(xpum::compat::format("unknown option '{}'", arg));
			}
		}
	} catch (const std::exception &e) {
		std::cerr << xpum::compat::format("error: {}\n", e.what());
		usage(args[0]);
		return 1;
	}

	if (idlerChild) {
		return runIdlerChild(cfg);
	}
	switch (cfg.mode) {
	case Mode::Submit:
	case Mode::Idle:
		return runSingle(cfg);
	case Mode::Both:
		return runBoth(args.front(), cfg);
	}
	return 0;
}
