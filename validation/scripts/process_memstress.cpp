/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * process_memstress.cpp - Integration test helper for xpu-smi process memory reporting.
 *
 * Allocates real device VRAM via Level Zero in patterns that exercise the
 * drm-client-id deduplication logic in xpu-smi's process listing.
 *
 * Modes:
 *   alloc   One process allocates SIZE MiB and holds it.  Baseline check.
 *   fork    Parent allocates SIZE MiB then forks.  Parent and child share
 *           the same drm-client-id via fd inheritance; the correct total
 *           reported by xpu-smi across both PIDs is SIZE MiB, not 2xSIZE.
 *   dup     One process allocates SIZE MiB then dup()s its DRM fd(s) DUP_COUNT
 *           extra times each (reproducing the Firefox multi-fd pattern).  The
 *           correct total reported by xpu-smi for this PID is SIZE MiB, not
 *           NxSIZE.
 *
 * Output protocol (one "KEY=VALUE\n" per line to stdout):
 *   MODE=<mode>
 *   DEVICE=<index>
 *   ALLOC_BYTES=<n>          exact bytes passed to zeMemAllocDevice
 *   PARENT_PID=<pid>
 *   CHILD_PID=<pid>          fork mode only
 *   CLIENT_IDS=<id>[,<id>]  drm-client-id(s) visible in /proc/self/fdinfo after alloc
 *   DUP_FDS=<n>              dup mode: total DRM fds open after dup (originals + dups)
 *   READY                    last line; block the test harness on this before querying
 *
 * The process runs until SIGINT/SIGTERM, or --duration <seconds> elapses (0 = forever).
 * Exit 0 on clean shutdown; non-zero if L0 initialisation or allocation fails.
 *
 * Build (standalone):
 *   g++ -std=c++20 -O2 process_memstress.cpp $(pkg-config --cflags --libs level-zero) -o
 * process_memstress
 *
 * Example - fork mode, 200 MiB on device 0, 30-second window:
 *   ./process_memstress --mode fork --size 200 --device 0 --duration 30
 */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include "utility/compat/format.h"
#include <fstream>
#include <iostream>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <cerrno>
#include <csignal>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ze_api.h>

namespace fs = std::filesystem;
using namespace std::string_view_literals;

namespace {

// -- constants ---------------------------------------------------------------

constexpr uint64_t MIB_TO_BYTES = 1024ULL * 1024ULL;
constexpr uint64_t DEFAULT_SIZE_MIB = 64;

// -- signal handling ---------------------------------------------------------
volatile sig_atomic_t gStop = 0;
void onSignal([[maybe_unused]] int sig) { gStop = 1; }

void setPdeathSig(int sig)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
	prctl(PR_SET_PDEATHSIG, sig);
}

void installSignalHandlers()
{
	struct sigaction sa
	{};
	sa.sa_handler = onSignal;
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGTERM, &sa, nullptr);
}

/** Owns a file descriptor obtained from dup() and closes it on destruction. */
class ScopedFd
{
	int fd;

public:
	explicit ScopedFd(int fileDescriptor) noexcept : fd(fileDescriptor) {}
	~ScopedFd()
	{
		if (fd >= 0) {
			close(fd);
		}
	}
	ScopedFd(const ScopedFd &) = delete;
	ScopedFd &operator=(const ScopedFd &) = delete;
	ScopedFd(ScopedFd &&other) noexcept : fd(std::exchange(other.fd, -1)) {}
	ScopedFd &operator=(ScopedFd &&other) noexcept
	{
		if (this != &other) {
			if (fd >= 0) {
				close(fd);
			}
			fd = std::exchange(other.fd, -1);
		}
		return *this;
	}
};

// -- fdinfo helpers ----------------------------------------------------------
struct DrmFdInfo
{
	int fd;
	uint64_t clientId;
	uint64_t totalVramKiB; // sum across all drm-total-vram* fields
};

struct FdinfoEntry
{
	int fd{-1};
	uint64_t clientId{UINT64_MAX};
	uint64_t totalVramKiB{0};
};

/** Parses one /proc/<pid>/fdinfo/<n> file for drm-client-id and drm-total-vram*. */
FdinfoEntry parseFdinfoEntry(int fileDescriptor, const fs::path &path)
{
	constexpr std::string_view clientIdKey = "drm-client-id:";
	constexpr std::string_view vramKey = "drm-total-vram";

	FdinfoEntry entry{.fd = fileDescriptor};
	std::ifstream fin(path);
	if (!fin) {
		return entry;
	}

	std::string line;
	while (std::getline(fin, line)) {
		if (line.starts_with(clientIdKey)) {
			const size_t pos = line.find_first_not_of(" \t", clientIdKey.size());
			if (pos != std::string::npos) {
				try {
					entry.clientId = std::stoull(line.substr(pos));
				} catch (...) {
				}
			}
		} else if (line.starts_with(vramKey)) {
			// "drm-total-vram0:   65072580 KiB" -- skip to after the colon
			const size_t colon = line.find(':');
			if (colon == std::string::npos) {
				continue;
			}
			const size_t pos = line.find_first_not_of(" \t", colon + 1);
			if (pos == std::string::npos) {
				continue;
			}
			try {
				entry.totalVramKiB += std::stoull(line.substr(pos));
			} catch (...) {
			}
		}
	}
	return entry;
}

/** Scans /proc/self/fdinfo and returns one entry per fd that has drm-client-id. */
[[nodiscard]] std::vector<DrmFdInfo> scanSelfFdinfo()
{
	std::vector<DrmFdInfo> out;
	std::error_code ec;
	for (const auto &e : fs::directory_iterator("/proc/self/fdinfo", ec)) {
		if (ec) {
			break;
		}
		int fileDescriptor = -1;
		try {
			fileDescriptor = std::stoi(e.path().filename().string());
		} catch (...) {
			continue;
		}

		const FdinfoEntry entry = parseFdinfoEntry(fileDescriptor, e.path());
		if (entry.clientId != UINT64_MAX) {
			out.push_back({entry.fd, entry.clientId, entry.totalVramKiB});
		}
	}
	return out;
}

void printClientIds(const std::vector<DrmFdInfo> &fds)
{
	std::vector<uint64_t> seen;
	for (const auto &f : fds) {
		if (std::ranges::find(seen, f.clientId) == seen.end()) {
			seen.push_back(f.clientId);
		}
	}
	std::string line = "CLIENT_IDS=";
	for (size_t i = 0; i < seen.size(); ++i) {
		if (i != 0) {
			line += ',';
		}
		line += std::to_string(seen[i]);
	}
	std::cout << line << '\n';
}

// -- Level Zero helpers ---------------------------------------------

/** Throws std::runtime_error if @p r indicates a Level Zero error. */
void l0Check(ze_result_t r, std::source_location loc = std::source_location::current())
{
	if (r != ZE_RESULT_SUCCESS) {
		std::cerr << xpum::compat::format("L0 error 0x{:x} at {}:{}\n", static_cast<unsigned>(r), loc.file_name(), loc.line());
		throw std::runtime_error(xpum::compat::format("L0 error 0x{:x}", static_cast<unsigned>(r)));
	}
}

/** Owns a Level Zero context and device allocation; both freed on destruction. */
struct L0State
{
	ze_driver_handle_t driver = nullptr;
	ze_device_handle_t device = nullptr;
	ze_context_handle_t context = nullptr;
	void *mem = nullptr;
	uint64_t bytes = 0;

	L0State() = default;
	~L0State()
	{
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

/** Collects the parameters shared by all allocation modes. */
struct AllocParams
{
	uint32_t deviceIndex{0};
	uint64_t bytes{0};
	int duration{0};
	int dupCount{0};
};

void l0Init(L0State &s, const AllocParams &p)
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
	if (p.deviceIndex >= deviceCount) {
		throw std::runtime_error(xpum::compat::format("device index {} out of range (have {})", p.deviceIndex, deviceCount));
	}

	std::vector<ze_device_handle_t> devices(deviceCount);
	l0Check(zeDeviceGet(s.driver, &deviceCount, devices.data()));
	s.device = devices[p.deviceIndex];

	ze_context_desc_t ctxDesc{};
	ctxDesc.stype = ZE_STRUCTURE_TYPE_CONTEXT_DESC;
	l0Check(zeContextCreate(s.driver, &ctxDesc, &s.context));

	ze_device_mem_alloc_desc_t memDesc{};
	memDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
	l0Check(zeMemAllocDevice(s.context, &memDesc, p.bytes, 1, s.device, &s.mem));
	s.bytes = p.bytes;
}

// Submit a minimal GPU command so that drm-cycles-* in fdinfo becomes non-zero.
// Without this, zesDeviceProcessesGetState reports engines==0 and the process
// is filtered out of xpu-smi's process list before memory is ever examined.
void l0SubmitWork(L0State &s)
{
	ze_command_queue_desc_t qDesc{};
	qDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
	qDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

	ze_command_list_handle_t list = nullptr;
	const ze_result_t r = zeCommandListCreateImmediate(s.context, s.device, &qDesc, &list);
	if (r != ZE_RESULT_SUCCESS) {
		std::cerr << xpum::compat::format("warning: zeCommandListCreateImmediate 0x{:x} -- "
								 "process may not appear in xpu-smi ps\n",
								 static_cast<unsigned>(r));
		return;
	}

	// 4-byte fill: smallest valid operation; enough to register engine cycles.
	uint32_t pattern = 0;
	zeCommandListAppendMemoryFill(list, s.mem, &pattern, sizeof(pattern), sizeof(pattern), nullptr, 0, nullptr);
	zeCommandListDestroy(list);
}

// -- wait helper -------------------------------------------------------------

void waitUntilStop(int durationSecs)
{
	if (durationSecs > 0) {
		for (int i = 0; i < durationSecs && gStop == 0; ++i) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	} else {
		while (gStop == 0) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
}

// -- mode: alloc -------------------------------------------------------------

[[nodiscard]] int modeAlloc(const AllocParams &p)
{
	try {
		L0State s;
		l0Init(s, p);
		l0SubmitWork(s);

		const auto fds = scanSelfFdinfo();
		std::cout << xpum::compat::format("MODE=alloc\nDEVICE={}\nALLOC_BYTES={}\nPARENT_PID={}\n", p.deviceIndex, p.bytes,
								 static_cast<int>(getpid()));
		printClientIds(fds);
		std::cout << "READY\n";
		std::cout.flush();

		waitUntilStop(p.duration);
		return 0; // ~L0State() frees mem and context
	} catch (const std::exception &e) {
		std::cerr << xpum::compat::format("error: {}\n", e.what());
		return 1;
	}
}

// -- mode: fork --------------------------------------------------------------
//
// Parent allocates VRAM then forks.  The child inherits all open file
// descriptors, so both parent and child show the same drm-client-id(s)
// in /proc/<pid>/fdinfo.  xpu-smi (after the fix) must report ALLOC_BYTES
// exactly once across both PIDs, not twice.

[[nodiscard]] int modeFork(const AllocParams &p)
{
	try {
		L0State s;
		l0Init(s, p);
		l0SubmitWork(s);

		const auto fds = scanSelfFdinfo();

		const pid_t child = fork();
		if (child < 0) {
			throw std::system_error(errno, std::system_category(), "fork");
		}

		if (child == 0) {
			// Use _exit: bypasses C++ destructors so ~L0State() does not run.
			// The L0 context and allocation belong to the parent; freeing them
			// from the child would corrupt the parent's GPU state.
			setPdeathSig(SIGTERM);
			installSignalHandlers();
			while (gStop == 0) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			_exit(0);
		}

		// Parent: report both PIDs so the test harness knows what to query.
		std::cout << xpum::compat::format("MODE=fork\nDEVICE={}\nALLOC_BYTES={}\n"
								 "PARENT_PID={}\nCHILD_PID={}\n",
								 p.deviceIndex, p.bytes, static_cast<int>(getpid()), static_cast<int>(child));
		printClientIds(fds);
		std::cout << "READY\n";
		std::cout.flush();

		waitUntilStop(p.duration);
		kill(child, SIGTERM);
		waitpid(child, nullptr, 0);
		return 0; // ~L0State() frees mem and context
	} catch (const std::exception &e) {
		std::cerr << xpum::compat::format("error: {}\n", e.what());
		return 1;
	}
}

// -- mode: dup ---------------------------------------------------------------
//
// Single process allocates VRAM then dup()s every DRM fd DUP_COUNT extra
// times.  Each dup shares the same drm-client-id in /proc/<pid>/fdinfo.
// xpu-smi (after the fix) must report ALLOC_BYTES once, not DUP_FDS times.

[[nodiscard]] int modeDup(const AllocParams &p)
{
	try {
		L0State s;
		l0Init(s, p);
		l0SubmitWork(s);

		const auto fds = scanSelfFdinfo();

		// Dup every fd that has a drm-client-id (covers all contexts L0 may have
		// opened, including the one tracking our allocation).
		std::vector<ScopedFd> dupFds;
		for (const auto &f : fds) {
			for (int i = 0; i < p.dupCount; ++i) {
				const int d = dup(f.fd);
				if (d >= 0) {
					dupFds.emplace_back(d);
				}
			}
		}
		const int totalDrmFds = static_cast<int>(fds.size()) + static_cast<int>(dupFds.size());

		std::cout << xpum::compat::format("MODE=dup\nDEVICE={}\nALLOC_BYTES={}\nPARENT_PID={}\n", p.deviceIndex, p.bytes,
								 static_cast<int>(getpid()));
		printClientIds(fds);
		std::cout << xpum::compat::format("DUP_FDS={}\n", totalDrmFds);
		std::cout << "READY\n";
		std::cout.flush();

		waitUntilStop(p.duration);
		return 0; // ~ScopedFd() closes dups, ~L0State() frees mem and context
	} catch (const std::exception &e) {
		std::cerr << xpum::compat::format("error: {}\n", e.what());
		return 1;
	}
}

void usage(const char *prog)
{
	std::cerr << xpum::compat::format("Usage: {} [options]\n"
							 "  --mode     alloc|fork|dup   allocation pattern (default: alloc)\n"
							 "  --size     <MiB>            device VRAM to allocate (default: 64)\n"
							 "  --device   <index>          L0 device index (default: 0)\n"
							 "  --dups     <n>              extra dup()s per DRM fd in dup mode (default: 3)\n"
							 "  --duration <secs>           hold time; 0 = until SIGINT/SIGTERM (default: 0)\n"
							 "\n"
							 "Expected xpu-smi behaviour after fix:\n"
							 "  alloc: ALLOC_BYTES reported for PARENT_PID\n"
							 "  fork:  ALLOC_BYTES total across PARENT_PID + CHILD_PID (not 2x)\n"
							 "  dup:   ALLOC_BYTES reported for PARENT_PID (not DUP_FDS x ALLOC_BYTES)\n",
							 prog);
}

} // namespace

int main(int argc, char **argv)
{
	installSignalHandlers();

	enum class Mode
	{
		ALLOC,
		FORK,
		DUP
	} mode = Mode::ALLOC;
	AllocParams p;
	p.bytes = DEFAULT_SIZE_MIB * MIB_TO_BYTES;
	p.dupCount = 3;

	const std::span<char *> args(argv, static_cast<size_t>(argc));

	try {
		for (size_t i = 1; i < args.size(); ++i) {
			const std::string_view arg = args[i];
			auto nextArg = [&]() -> const char * {
				if (i + 1 >= args.size()) {
					throw std::invalid_argument(xpum::compat::format("'{}' requires an argument", arg));
				}
				++i;
				return args[i];
			};

			if (arg == "--mode") {
				const std::string_view m = nextArg();
				if (m == "alloc") {
					mode = Mode::ALLOC;
				} else if (m == "fork") {
					mode = Mode::FORK;
				} else if (m == "dup") {
					mode = Mode::DUP;
				} else {
					throw std::invalid_argument(xpum::compat::format("unknown mode '{}'", m));
				}
			} else if (arg == "--size") {
				const uint64_t sizeMiB = std::stoull(nextArg());
				p.bytes = sizeMiB * MIB_TO_BYTES;
			} else if (arg == "--device") {
				p.deviceIndex = static_cast<uint32_t>(std::stoul(nextArg()));
			} else if (arg == "--dups") {
				p.dupCount = std::stoi(nextArg());
			} else if (arg == "--duration") {
				p.duration = std::stoi(nextArg());
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

	switch (mode) {
	case Mode::ALLOC:
		return modeAlloc(p);
	case Mode::FORK:
		return modeFork(p);
	case Mode::DUP:
		return modeDup(p);
	}
	return 0;
}
