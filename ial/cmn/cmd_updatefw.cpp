/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_updatefw.h"
#include <fs_lock.h>
#include <debug.h>
#include <os.h>
#include <terminal.h>
#include "utility/logger/ostream_sink.h"
#include "utility/logger/sink_base.h"
#include <assert.h>
#include <CLI/CLI.hpp>
#include <thread>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <sys/stat.h>
#include "utility/watchdog/watchdog.h"

/**
 * @brief Sink that writes console records above the firmware update progress bars
 *
 * The bars redraw relative to the cursor, so a line printed below them would be overwritten by the
 * next bar update. Each record is handed to the wrapped console sink, which keeps its own stream,
 * through progressPrintAbove().
 */
class ProgressAwareSink final : public Sink
{
	std::shared_ptr<Sink> mBackend;

public:
	/**
	 * @brief Wrap a console sink
	 * @param backend Sink that writes the records, to its own stream
	 */
	explicit ProgressAwareSink(std::shared_ptr<Sink> backend) : mBackend(std::move(backend)) {}

	/**
	 * @brief Write one record above the progress bars
	 * @param level Level of the record
	 * @param loc Source location of the record
	 * @param prefix Level prefix, such as "[Error] "
	 * @param msg Formatted message
	 */
	void log(LogLevel level, std::source_location loc, std::string_view prefix, std::string_view msg) noexcept override
	{
		// OStreamSink ends a record with a line break unless it has one, or contains '\r' (an in-place update)
		const bool endsLine = msg.empty() || msg.back() == '\n' || msg.find('\r') == std::string_view::npos;
		try {
			progressPrintAbove([&] { mBackend->log(level, loc, prefix, msg); }, endsLine);
		} catch (...) {
		}
	}

	/**
	 * @brief Unused: log() is overridden and hands records to the backend directly
	 */
	void emit(const LogRecord &) override {}

	/**
	 * @brief Unused: the backend flushes its own stream
	 */
	void sync() override {}
};

/**
 * @brief Keeps console output from being drawn over by the progress bars while it is alive
 *
 * The diagnostic sink (stderr) and the PRINT sink (stdout) are wrapped and restored separately, so
 * each keeps writing to its own stream. Only takes effect when the bars are on a terminal, and only
 * for console sinks; a log file sink is left alone.
 */
class ProgressConsoleGuard
{
	std::shared_ptr<Sink> mPreviousLog;
	std::shared_ptr<Sink> mPreviousPrint;

public:
	/**
	 * @brief Wrap the console sinks, when stdout is a terminal
	 *
	 * The previous sinks are kept for the destructor to restore.
	 */
	ProgressConsoleGuard()
	{
		if (!stdoutIsTerminal()) {
			return;
		}
		Logger &logger = Logger::instance();
		std::shared_ptr<Sink> log = logger.getSink();
		if (std::dynamic_pointer_cast<OStreamSink>(log)) {
			mPreviousLog = log;
			logger.setLogSink(std::make_shared<ProgressAwareSink>(log));
		}
		std::shared_ptr<Sink> print = logger.getPrintSink();
		if (std::dynamic_pointer_cast<OStreamSink>(print)) {
			mPreviousPrint = print;
			logger.setPrintSink(std::make_shared<ProgressAwareSink>(print));
		}
	}
	/**
	 * @brief End the progress bar block and put the previous sinks back
	 */
	~ProgressConsoleGuard()
	{
		progressEnd();
		if (mPreviousLog) {
			Logger::instance().setLogSink(mPreviousLog);
		}
		if (mPreviousPrint) {
			Logger::instance().setPrintSink(mPreviousPrint);
		}
	}
	ProgressConsoleGuard(const ProgressConsoleGuard &) = delete;
	ProgressConsoleGuard &operator=(const ProgressConsoleGuard &) = delete;
	ProgressConsoleGuard(ProgressConsoleGuard &&) = delete;
	ProgressConsoleGuard &operator=(ProgressConsoleGuard &&) = delete;
};

// Outcome of one update pass on one device, listed in the summary printed at the end of run()
struct deviceUpdateResult
{
	uint32_t deviceIndex;
	std::string bdf;
	int amcIndex;	  // -1 when the device has no AMC
	const char *what; // what the pass flashed, as shown in the summary
	bool showAmcCard; // the pass went through the AMC, so its card number is worth showing
	bool success;
};

/**
 * @brief Print which devices were updated and which were not
 *
 * Several devices are updated in parallel and their errors interleave on the console, so the
 * outcome is listed per device once every update has finished. The AMC card number matches the
 * "card N" used by the AMC error messages.
 *
 * @param results One entry per device and update pass
 */
static void printUpdateSummary(const std::vector<deviceUpdateResult> &results)
{
	size_t failed = 0;
	for (const auto &r : results) {
		if (!r.success) {
			failed++;
		}
	}

	PRINT("Firmware update summary: {} of {} succeeded\n", results.size() - failed, results.size());
	for (const auto &r : results) {
		std::string amcCard;
		if (r.showAmcCard && r.amcIndex >= 0) {
			amcCard = ", AMC card " + std::to_string(r.amcIndex);
		}
		PRINT("  Device {} ({}{}) {}: {}\n", r.deviceIndex, r.bdf, amcCard, r.what, r.success ? "Success" : "Failed");
	}
}

static const char *validFwTypes[] = {"GFX", "GFX_DATA", "FDO", "AMC", "OP_CODE", "OP_DATA", "COMPOSITE"};

/**
 * @brief Returns a comma-separated string of valid firmware type names
 *
 * @return std::string Formatted list of supported firmware types
 */
static std::string validFwTypesStr()
{
	std::string s;
	for (const auto &t : validFwTypes) {
		if (!s.empty())
			s += ", ";
		s += t;
	}
	return s;
}

static ze_result_t validateFirmwareImageFile(const std::string &filePath)
{
	struct stat fileStat;
	if (stat(filePath.c_str(), &fileStat) != 0) {
		ERR("Error: Invalid firmware file path '{}': {}.\n", filePath.c_str(), std::strerror(errno));
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (!(fileStat.st_mode & S_IFREG)) {
		ERR("Error: Firmware file '{}' is not a regular file.\n", filePath.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (fileStat.st_size <= 0) {
		ERR("Error: Firmware file '{}' is empty.\n", filePath.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::ifstream file(filePath, std::ios::binary);
	if (!file.good()) {
		ERR("Error: Firmware file '{}' is not readable.\n", filePath.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdUpdateFW::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Update GPU firmware"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s updatefw [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s updatefw -d [deviceId] -t GFX -f [imageFilePath]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s updatefw -d [pciBdfAddress] -t GFX -f [imageFilePath]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s updatefw -d [deviceId] -t FDO -f [imageFilePath]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s updatefw -t AMC -f [imageFilePath]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s updatefw -d [deviceId] -t COMPOSITE -f [packageFilePath]", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "--device,--id               The device ID or PCI BDF address. If it is not "
										"specified, all devices will be updated"));
	std::string typeHelp = "-t,--type                   The firmware name. Valid options: " + validFwTypesStr() + ".";
	helpList.push_back(helpCmd(HEADING, "%s", typeHelp.c_str()));
	helpList.push_back(helpCmd(HEADING, "                            COMPOSITE takes a PLDM firmware update package "
										"and applies every component it carries that this device accepts"));
	helpList.push_back(helpCmd(HEADING, "-f,--file                   The firmware image file path on this server"));
	helpList.push_back(helpCmd(
		HEADING, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	helpList.push_back(helpCmd(
		HEADING, "--force                     Force firmware update. For GFX firmware, forces the update. For AMC "
				 "firmware, allows downgrade when the .img advertises the ForceUpdate capability"));
	helpList.push_back(helpCmd(HEADING, "--fdo                       Only with -t COMPOSITE. Flash the IFWI recovery "
										"component of the package to the flash override device, and nothing else"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Executes the updatefw run.
 *
 * @return int Returns 0 on success.
 */
int cmdUpdateFW::run(arg_struct *args)
{
	TRACING();
	firmwareInfo fwInfo = {};
	std::vector<devInfo> deviceList;
	ze_result_t result;
	uint32_t totalThreads = 0;
	std::atomic<uint32_t> curThread{0};
	std::atomic<ze_result_t> firstError{ZE_RESULT_SUCCESS};
	// Components of a composite package flashed across both update passes and all devices
	std::atomic<uint32_t> componentsApplied{0};
	std::vector<std::thread> workers;
	// Filled by the workers, one slot each, so they need no lock
	std::vector<deviceUpdateResult> results;

	CLI::App sub{"Update GPU firmware", "updatefw"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", fwInfo.jsonOutput, "Print result in JSON format");
	sub.add_option("-d,--device,--id", fwInfo.deviceId,
				   "Device index or BDF address, comma-separated for multiple (e.g. 0,1). If not specified, all "
				   "devices are updated");
	std::string typeDesc = "Firmware name. Valid options: " + validFwTypesStr();
	sub.add_option("-t,--type", fwInfo.firmwareType, typeDesc);
	sub.add_option("-f,--file", fwInfo.filePath, "Firmware image file path");
	sub.add_flag("-y,--assumeyes", fwInfo.assumeYes, "Assume yes to all questions");
	sub.add_flag("--force", fwInfo.forceUpdate,
				 "Force firmware update (GFX: force flash; AMC: allow downgrade when supported by the image)");
	sub.add_flag("--fdo", fwInfo.fdoOnly,
				 "COMPOSITE only: flash the IFWI recovery component to the flash override device and nothing else");

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}", e.what());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate firmware type
	if (fwInfo.firmwareType.empty()) {
		ERR("Error: Missing required argument --type.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	bool validType = false;
	for (const auto &t : validFwTypes) {
		if (STRCASECMP(fwInfo.firmwareType.c_str(), t) == 0) {
			validType = true;
			break;
		}
	}
	if (!validType) {
		ERR("Error: Invalid firmware type '{}'. Valid options: {}.\n", fwInfo.firmwareType.c_str(),
			validFwTypesStr().c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	const bool isComposite = STRCASECMP(fwInfo.firmwareType.c_str(), "composite") == 0;

	if (fwInfo.fdoOnly && !isComposite) {
		ERR("Error: --fdo is only valid with --type COMPOSITE.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Validate file path
	if (fwInfo.filePath.empty()) {
		ERR("Error: Missing required argument --file.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = validateFirmwareImageFile(fwInfo.filePath);
	if (result != ZE_RESULT_SUCCESS) {
		return result;
	}

	result = args->sm.findDevice(fwInfo.deviceId.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'.\n", fwInfo.deviceId.c_str());
		return result;
	}

	// Acquire each device's firmware update lock (cross-process) only once the request
	// is known to be valid, so --help or bad input never touches the locks. They are
	// held until run() returns, i.e. across every flash below. Updates of other devices,
	// in this process or another, are not blocked.
	std::vector<PciAddress> addresses;
	addresses.reserve(deviceList.size());
	for (auto &device : deviceList) {
		const std::string bdf = device.dev->getPCI()->getBDFStr();
		const std::optional<PciAddress> address = PciAddress::parse(bdf);
		if (!address) {
			ERR("Unrecognized PCI address '{}' of a device to update.\n", bdf);
			return ZE_RESULT_ERROR_UNKNOWN;
		}
		addresses.push_back(*address);
	}
	const FSLockSet fsLocks{addresses};
	if (!fsLocks.locked()) {
		if (fsLocks.failedBecauseBusy()) {
			ERR("A firmware update of device {} is already in progress.\n", *fsLocks.failedDevice());
		} else {
			ERR("Could not acquire the firmware update lock of device {} (firmware update requires "
				"root/administrator privileges).\n",
				*fsLocks.failedDevice());
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Count the total number of threads as this will come in handy later, and lay out the result of every
	// update before any worker starts, so the vector is never resized while the workers write to it.
	const bool isAmcType = STRCASECMP(fwInfo.firmwareType.c_str(), "amc") == 0;
	const std::string passName = isComposite ? "GPU components" : fwInfo.firmwareType;
	std::vector<devInfo *> workerDevices;
	for (auto &device : deviceList) {
		if ((STRCASECMP(fwInfo.firmwareType.c_str(), "amc") == 0 && device.dev->hasAmc()) ||
			STRCASECMP(fwInfo.firmwareType.c_str(), "amc") != 0) {
			totalThreads++;
			workerDevices.push_back(&device);
			results.push_back({device.index, device.dev->getPCI()->getBDFStr(), device.dev->getAmcIndex(),
							   passName.c_str(), isAmcType, false});
		}
	}

	// Killing a flash mid-write can leave the card unusable, and a flash can legitimately take
	// minutes, so the watchdog is suspended. Held to the end of run() so it also covers the AMC
	// pass below.
	const auto flashing = xpum::watchdog::suspend("firmware update");
	// Log output goes above the progress bars from here until run() returns
	ProgressConsoleGuard progressConsole;
	progressBegin(static_cast<int>(totalThreads));

	// Parallelize per-device firmware updates
	workers.reserve(totalThreads);

	for (size_t w = 0; w < workerDevices.size(); w++) {
		deviceUpdateResult *outcome = &results[w];
		workers.emplace_back([&, fwInfo, totalThreads, devPtr = workerDevices[w], outcome]() {
			// Make a thread‑local copy of firmwareInfo to avoid data races
			firmwareInfo localInfo = fwInfo;
			localInfo.dev = devPtr->dev;
			localInfo.deviceIndex = devPtr->index;
			localInfo.amcIndex = devPtr->dev->getAmcIndex();
			localInfo.totalThreads = totalThreads;
			localInfo.curThread = curThread.fetch_add(1, std::memory_order_relaxed);
			// This pass only covers what is reachable per device; the AMC of a composite package
			// is shared between the GPUs of a card and is flashed by the pass below.
			localInfo.scope = isComposite ? COMPOSITE_SCOPE_GPU : COMPOSITE_SCOPE_NONE;

			firmware *fw = devPtr->dev->getFirmware();
			if (fw == nullptr) {
				ERR("Error: Firmware pointer not found (device {}).\n", devPtr->index);
				ze_result_t expected = ZE_RESULT_SUCCESS;
				firstError.compare_exchange_strong(expected, ZE_RESULT_ERROR_UNKNOWN);
				return;
			}
			if (fw->updateFW(&localInfo) != ZE_RESULT_SUCCESS) {
				ERR("Error: Failed to update firmware for device {}.\n", devPtr->index);
				ze_result_t expected = ZE_RESULT_SUCCESS;
				firstError.compare_exchange_strong(expected, ZE_RESULT_ERROR_UNKNOWN);
				return;
			}
			outcome->success = true;
			componentsApplied.fetch_add(localInfo.componentsApplied, std::memory_order_relaxed);
		});
	}

	for (auto &t : workers) {
		if (t.joinable()) {
			t.join();
		}
	}

	// A composite package also carries the images the AMC flashes over PLDM (AMC and VR_CONFIG). One
	// AMC can serve several GPUs, and it is reached over a shared bus, so the cards are updated one at
	// a time after the per-device pass instead of once per attached GPU. --fdo asks for the recovery
	// image alone, which is not flashed through the AMC, so that pass is skipped entirely.
	size_t amcLines = 0;
	if (isComposite && !fwInfo.fdoOnly) {
		std::vector<devInfo *> amcDevices;

		for (auto &device : deviceList) {
			int amcIndex = device.dev->getAmcIndex();
			if (amcIndex == -1) {
				continue;
			}
			bool alreadyQueued = false;
			for (const auto *queued : amcDevices) {
				if (queued->dev->getAmcIndex() == amcIndex) {
					alreadyQueued = true;
					break;
				}
			}
			if (!alreadyQueued) {
				amcDevices.push_back(&device);
			}
		}

		amcLines = amcDevices.size();
		progressBegin(static_cast<int>(amcLines));

		for (size_t i = 0; i < amcDevices.size(); i++) {
			firmwareInfo localInfo = fwInfo;
			localInfo.dev = amcDevices[i]->dev;
			localInfo.deviceIndex = amcDevices[i]->index;
			localInfo.amcIndex = amcDevices[i]->dev->getAmcIndex();
			localInfo.totalThreads = (uint32_t)amcLines;
			localInfo.curThread = (uint32_t)i;
			localInfo.scope = COMPOSITE_SCOPE_AMC;

			firmware *fw = amcDevices[i]->dev->getFirmware();
			if (fw == nullptr) {
				results.push_back({amcDevices[i]->index, amcDevices[i]->dev->getPCI()->getBDFStr(),
								   amcDevices[i]->dev->getAmcIndex(), "AMC components", true, false});
				ERR("Error: Firmware pointer not found (device {}).\n", amcDevices[i]->index);
				ze_result_t expected = ZE_RESULT_SUCCESS;
				firstError.compare_exchange_strong(expected, ZE_RESULT_ERROR_UNKNOWN);
				continue;
			}
			const bool success = fw->updateFW(&localInfo) == ZE_RESULT_SUCCESS;
			results.push_back({amcDevices[i]->index, amcDevices[i]->dev->getPCI()->getBDFStr(),
							   amcDevices[i]->dev->getAmcIndex(), "AMC components", true, success});
			if (!success) {
				ERR("Error: Failed to update AMC firmware for device {}.\n", amcDevices[i]->index);
				ze_result_t expected = ZE_RESULT_SUCCESS;
				firstError.compare_exchange_strong(expected, ZE_RESULT_ERROR_UNKNOWN);
				continue;
			}
			componentsApplied.fetch_add(localInfo.componentsApplied, std::memory_order_relaxed);
		}
	}

	progressEnd();
	if (totalThreads > 0 || amcLines > 0) {
		PRINT("\n"); // Move the cursor to the next line after the last progress bar
	}

	// With more than one update their errors interleave, so say per device how each one ended.
	if (results.size() > 1) {
		printUpdateSummary(results);
	}

	if (firstError != ZE_RESULT_SUCCESS) {
		return firstError.load();
	}

	// Each pass skips the components it does not own, so a pass that flashed nothing is not an
	// error on its own; only a composite update in which no pass flashed anything is.
	if (isComposite && componentsApplied.load() == 0) {
		ERR("Error: Package '{}' carries no component that applies to the selected device(s){}. Nothing was "
			"updated. Run with XPU_SMI_LOG_LEVEL=DBG to see why each component was skipped.\n",
			fwInfo.filePath.c_str(), fwInfo.fdoOnly ? " with --fdo" : "");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	PRINT("Firmware update operation completed successfully.\n");

	return 0;
}
