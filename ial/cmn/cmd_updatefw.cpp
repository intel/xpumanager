/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_updatefw.h"
#include <fs_lock.h>
#include <debug.h>
#include <assert.h>
#include <CLI/CLI.hpp>
#include <thread>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

static const char *validFwTypes[] = {"GFX", "GFX_DATA", "FDO", "AMC", "OP_CODE", "OP_DATA"};

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
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "--device,--id               The device ID or PCI BDF address. If it is not "
										"specified, all devices will be updated"));
	std::string typeHelp = "-t,--type                   The firmware name. Valid options: " + validFwTypesStr() + ".";
	helpList.push_back(helpCmd(HEADING, "%s", typeHelp.c_str()));
	helpList.push_back(helpCmd(HEADING, "-f,--file                   The firmware image file path on this server"));
	helpList.push_back(helpCmd(
		HEADING, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	helpList.push_back(helpCmd(
		HEADING, "--force                     Force GFX firmware update. This parameter only works for GFX firmware"));

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

	// Acquire global firmware update lock (cross-process)
	FSLock fsLock;
	if (!fsLock.locked()) {
		ERR("Another firmware update operation is already in progress or lock could not be acquired.\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	uint32_t totalThreads = 0;
	std::atomic<uint32_t> curThread{0};
	std::atomic<ze_result_t> firstError{ZE_RESULT_SUCCESS};
	std::vector<std::thread> workers;

	CLI::App sub{"Update GPU firmware", "updatefw"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", fwInfo.jsonOutput, "Print result in JSON format");
	sub.add_option("-d,--device,--id", fwInfo.deviceId,
				   "Device ID or PCI BDF address. If not specified, all devices are updated");
	std::string typeDesc = "Firmware name. Valid options: " + validFwTypesStr();
	sub.add_option("-t,--type", fwInfo.firmwareType, typeDesc);
	sub.add_option("-f,--file", fwInfo.filePath, "Firmware image file path");
	sub.add_flag("-y,--assumeyes", fwInfo.assumeYes, "Assume yes to all questions");
	sub.add_flag("--force", fwInfo.forceUpdate, "Force GFX firmware update");

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

	// Print a newline for every thread that we will be creating. Also count the total number of threads
	// as this will come in handy later.
	for (auto &device : deviceList) {
		if ((STRCASECMP(fwInfo.firmwareType.c_str(), "amc") == 0 && device.dev->getAmcIndex() != -1) ||
			STRCASECMP(fwInfo.firmwareType.c_str(), "amc") != 0) {
			PRINT("\n");
			totalThreads++;
		}
	}

	// Parallelize per-device firmware updates
	workers.reserve(totalThreads);

	for (auto &device : deviceList) {
		if ((STRCASECMP(fwInfo.firmwareType.c_str(), "amc") == 0 && device.dev->getAmcIndex() != -1) ||
			STRCASECMP(fwInfo.firmwareType.c_str(), "amc") != 0) {
			workers.emplace_back([&, fwInfo, totalThreads, devPtr = &device]() {
				// Make a thread‑local copy of firmwareInfo to avoid data races
				firmwareInfo localInfo = fwInfo;
				localInfo.dev = devPtr->dev;
				localInfo.deviceIndex = devPtr->index;
				localInfo.amcIndex = devPtr->dev->getAmcIndex();
				localInfo.totalThreads = totalThreads;
				localInfo.curThread = curThread.fetch_add(1, std::memory_order_relaxed);

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
			});
		}
	}

	for (auto &t : workers) {
		if (t.joinable()) {
			t.join();
		}
	}

	if (firstError != ZE_RESULT_SUCCESS) {
		return firstError.load();
	} else {
		if (totalThreads > 0) {
			PRINT("\n"); // Move the cursor to the next line after the last progress bar
		}
		PRINT("Firmware update operation completed successfully.\n");
	}

	return 0;
}
