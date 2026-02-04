/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "fwupd.h"
#include <fstream>
#include <sys/stat.h>
#include <thread>

/**
 * @brief Reads firmware image content from a file
 *
 * This function reads the entire contents of a firmware image file into
 * a vector buffer. It performs basic file validation to ensure the file
 * exists and is a regular file before reading.
 *
 * @param filePath Path to the firmware image file to read
 * @return std::vector<char> Buffer containing the file contents, empty if error
 */
std::vector<char> fwupd::readImageContent(const char *filePath)
{
	struct stat s;
	if (stat(filePath, &s) != 0 || !(s.st_mode & S_IFREG))
		return std::vector<char>();
	std::ifstream is(std::string(filePath), std::ifstream::binary);
	if (!is) {
		return std::vector<char>();
	}
	// get length of file:
	is.seekg(0, is.end);
	int length = (int)is.tellg();
	is.seekg(0, is.beg);

	std::vector<char> buffer(length);

	is.read(buffer.data(), length);
	is.close();
	return buffer;
}

/**
 * @brief query and prints the firmware update progress percentage
 *
 * This function fetches the firmware progress update from zesFirmwareGetFlashProgress
 * and updates the screen with completed percentage of firmware flash
 *
 * @param flashData the pointer to structure containing firmware handle and mutex for syncronization
 */
static void trackFirmwareFlashProgress(firmwareProgressInfo *flashData)
{
	bool loopContinue = true;
	constexpr uint32_t sleepTimeInMillisec = 250;
	uint32_t progressPercent = 0;
	do {
		progressPercent = 0;
		ze_result_t result = zesFirmwareGetFlashProgress(flashData->firmwareHandle, &progressPercent);
		if (result != ZE_RESULT_SUCCESS) {
			break;
		}
		PRINT("\rFirmware Flash Progress: %u %%", progressPercent);

		std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeInMillisec));

		flashData->firmwareProgressMutex.lock();
		loopContinue = !flashData->flashComplete;
		flashData->firmwareProgressMutex.unlock();

	} while (loopContinue);
	progressPercent = 0;
	ze_result_t result = zesFirmwareGetFlashProgress(flashData->firmwareHandle, &progressPercent);
	if (result != ZE_RESULT_SUCCESS) {
		return;
	}
	PRINT("\rFirmware Flash Progress: %u %%\n", progressPercent);
}

/**
 * @brief Updates firmware using the provided firmware information
 *
 * This function performs the actual firmware update operation by reading
 * the firmware image file and flashing it to the device using the Level Zero
 * Sysman firmware flash API.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return ze_result_t ZE_RESULT_SUCCESS if update successful, error code otherwise
 */
ze_result_t fwupd::updateFW(firmwareInfo *fwInfo)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	// read image file
	fwInfo->buffer = readImageContent(fwInfo->filePath.c_str());

	// start progress thread
	firmwareProgressInfo progressData = {};
	progressData.flashComplete = false;
	progressData.firmwareHandle = fwInfo->firmwareHandle;
	std::thread progressThread(trackFirmwareFlashProgress, &progressData);

	result = zesFirmwareFlash(fwInfo->firmwareHandle, fwInfo->buffer.data(), (uint32_t)fwInfo->buffer.size());
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to flash firmware: 0x%X (%s)\n", result, l0_error_to_string(result));
		// stop progress thread
		progressData.firmwareProgressMutex.lock();
		progressData.flashComplete = true;
		progressData.firmwareProgressMutex.unlock();
		progressThread.join();

		return result;
	}

	// stop progress thread
	progressData.firmwareProgressMutex.lock();
	progressData.flashComplete = true;
	progressData.firmwareProgressMutex.unlock();
	progressThread.join();

	DBG("Firmware flash successful.\n");
	return ZE_RESULT_SUCCESS;
};
