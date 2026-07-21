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
 * @param flashData the pointer to structure containing firmware handle and mutex for synchronization
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

		if (flashData->totalThreads > 0) {
			SETPROGRESSLABEL((int)flashData->deviceIndex, (int)flashData->curThread, (int)flashData->totalThreads,
							 progressPercent, flashData->label);
		} else {
			PRINT("\rFirmware Flash Progress: {} %", progressPercent);
		}

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

	if (flashData->totalThreads > 0) {
		SETPROGRESSLABEL((int)flashData->deviceIndex, (int)flashData->curThread, (int)flashData->totalThreads,
						 progressPercent, flashData->label);
	} else {
		PRINT("\rFirmware Flash Progress: {} %\n", progressPercent);
	}
}

/**
 * @brief Names the image being flashed, for use in progress output and error messages
 *
 * A composite update flashes several images out of one package, so the file path alone does not
 * identify which one a message refers to. Falls back to the file path when no component label was
 * set, which is the case for every single-image update.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return std::string Human-readable description of the image being flashed
 */
static std::string flashTarget(const firmwareInfo *fwInfo)
{
	if (!fwInfo->imageLabel.empty()) {
		return fwInfo->imageLabel + " from '" + fwInfo->filePath + "'";
	}
	return "'" + fwInfo->filePath + "'";
}

/**
 * @brief Updates firmware using the provided firmware information
 *
 * This function performs the actual firmware update operation by reading
 * the firmware image file and flashing it to the device using the Level Zero
 * Sysman firmware flash API.
 *
 * When fwInfo->imagePreloaded is set the buffer already holds the image to flash -- a component
 * extracted from a composite package -- and no file is read.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return ze_result_t ZE_RESULT_SUCCESS if update successful, error code otherwise
 */
ze_result_t fwupd::updateFW(firmwareInfo *fwInfo)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	// read image file, unless the caller already supplied the image to flash
	if (!fwInfo->imagePreloaded) {
		fwInfo->buffer = readImageContent(fwInfo->filePath.c_str());
	}
	if (fwInfo->buffer.empty()) {
		ERR("Firmware image {} is empty or unreadable.\n", flashTarget(fwInfo).c_str());
		return ZE_RESULT_ERROR_INVALID_SIZE;
	}

	zes_firmware_properties_t firmwareProps = {};
	firmwareProps.stype = ZES_STRUCTURE_TYPE_FIRMWARE_PROPERTIES;
	firmwareProps.pNext = nullptr;
	result = zesFirmwareGetProperties(fwInfo->firmwareHandle, &firmwareProps);
	if (result == ZE_RESULT_ERROR_UNINITIALIZED) {
		ERR("Firmware interface is not initialized\n");
		return result;
	} else if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to query firmware interface: 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	// start progress thread
	firmwareProgressInfo progressData = {};
	progressData.flashComplete = false;
	progressData.firmwareHandle = fwInfo->firmwareHandle;
	progressData.deviceIndex = fwInfo->deviceIndex;
	progressData.curThread = fwInfo->curThread;
	progressData.totalThreads = fwInfo->totalThreads;
	progressData.label = fwInfo->imageLabel.empty() ? nullptr : fwInfo->imageLabel.c_str();
	std::thread progressThread(trackFirmwareFlashProgress, &progressData);

	result = zesFirmwareFlash(fwInfo->firmwareHandle, fwInfo->buffer.data(), (uint32_t)fwInfo->buffer.size());
	if (result != ZE_RESULT_SUCCESS) {
		// stop progress thread
		progressData.firmwareProgressMutex.lock();
		progressData.flashComplete = true;
		progressData.firmwareProgressMutex.unlock();
		progressThread.join();

		if (result == ZE_RESULT_ERROR_INVALID_ARGUMENT || result == ZE_RESULT_ERROR_INVALID_SIZE) {
			ERR("Firmware image {} may be invalid/incompatible.\n", flashTarget(fwInfo).c_str());
		} else if (result == ZE_RESULT_ERROR_UNINITIALIZED) {
			ERR("Firmware image {} may be invalid/Firmware interface is not initialized.\n",
				flashTarget(fwInfo).c_str());
		} else {
			ERR("Failed to flash firmware: 0x{:X} ({})\n", result, l0_error_to_string(result));
		}

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
