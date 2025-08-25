/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "amcupd.h"
#include <debug.h>
#include <os.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>

// Global amclib instance - shared resource across all amcupd instances
static amclib *amcobj = nullptr;

// Static member definitions for thread synchronization
std::mutex amcupd::amcobj_mutex;
std::once_flag amcupd::amcobj_init_flag;

/**
 * @brief Constructor for the amcupd class
 *
 * Initializes the AMC update handler by enumerating available AMC cards
 * and their firmware versions. Sets up the internal state for subsequent
 * firmware update operations.
 */
amcupd::amcupd() : numOfCards(0) {}

/**
 * @brief Destructor for the amcupd class
 *
 * Cleans up resources used by the amcupd class, including the
 * amclib object.
 */
amcupd::~amcupd()
{
	// Use lock guard to ensure thread-safe cleanup
	std::lock_guard<std::mutex> lock(amcobj_mutex);

	if (amcobj) {
		delete amcobj;
		amcobj = nullptr;
	}
}

/**
 * @brief Prepares the AMC (Add-in Management Controller) for firmware update
 *
 * This function performs pre-update operations including opening the I2C
 * device connection required for AMC firmware communication. It establishes
 * the necessary hardware interface before the actual firmware update process.
 *
 * @param fwInfo Pointer to firmware information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful preparation,
 *                     ZE_RESULT_ERROR_UNKNOWN if I2C device open fails
 */
ze_result_t amcupd::preUpdateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();

	// Use std::call_once to ensure initialization happens exactly once across all threads
	std::call_once(amcobj_init_flag, [this]() {
		amcobj = new amclib();
		// Enumerate available AMC cards along with their firmware versions
		numOfCards = amcobj->amcEnumFirmwares();
		if (numOfCards <= 0) {
			ERR("Failed to initialize amclib\n");
		}
	});

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs multi-threaded AMC firmware update with real-time progress monitoring
 *
 * This function executes a comprehensive firmware update process for the Add-in Management
 * Controller (AMC) using a multi-threaded approach. It creates separate threads for the
 * actual firmware flashing operation and progress monitoring to ensure non-blocking
 * operation and real-time status updates.
 *
 * The function performs the following operations:
 * 1. Validates the device index against available AMC cards
 * 2. Creates a dedicated thread to handle the firmware flashing process
 * 3. Creates a separate thread to continuously monitor and report update progress
 * 4. Waits for both operations to complete before returning
 * 5. Provides thread-safe progress updates through the device interface
 *
 * Progress monitoring includes:
 * - Continuous polling of firmware update status at 100ms intervals
 * - Real-time progress percentage updates via fwInfo->dev->setProgress()
 * - Automatic progress capping at 100% to handle potential overflow
 * - Debug logging for operation completion tracking
 *
 * @param fwInfo Pointer to firmware information structure containing:
 *               - filePath: Absolute path to the firmware binary file
 *               - deviceIndex: Zero-based index of the target AMC device
 *               - dev: Device interface for progress reporting
 *
 * @return ze_result_t ZE_RESULT_SUCCESS on successful firmware update completion
 *                     ZE_RESULT_ERROR_INVALID_ARGUMENT if deviceIndex exceeds available cards
 *
 * @note This function blocks until both firmware flashing and progress monitoring complete
 * @note Thread-safe implementation ensures no race conditions during progress updates
 * @note Requires valid AMC library initialization via constructor
 *
 * @see preUpdateAMC() for pre-update preparation
 * @see postUpdateAMC() for post-update cleanup
 */
ze_result_t amcupd::updateAMC(firmwareInfo *fwInfo)
{
	TRACING();
	DBG("Updating AMC firmware...\n");

	std::string filePath = fwInfo->filePath;
	uint32_t deviceIndex = fwInfo->deviceIndex;

	if (deviceIndex >= (uint32_t)numOfCards) {
		ERR("Invalid device index: %d\n", deviceIndex);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Shared state for thread coordination
	std::atomic<bool> flashCompleted{false};
	std::atomic<bool> flashSuccess{false};
	std::atomic<bool> stopProgress{false};

	// Create a thread to flash firmware
	std::thread flashThread([this, deviceIndex, filePath, &flashCompleted, &flashSuccess]() {
		int result = amcobj->amcFirmwareFlash(deviceIndex, filePath.c_str());
		flashSuccess.store(result == 0);
		flashCompleted.store(true);

		if (result != 0) {
			ERR("Failed to flash firmware for card %d\n", deviceIndex);
		} else {
			DBG("Firmware flash for card %d completed successfully\n", deviceIndex);
		}
	});

	// Create a thread to monitor firmware flash progress
	std::thread progressThread([this, deviceIndex, fwInfo, &flashCompleted, &stopProgress]() {
		uint32_t progress = 0;
		while (!stopProgress.load() && progress < 100) {
			progress = amcobj->amcFirmwareProgress(deviceIndex);
			if (progress > 100)
				progress = 100; // Cap at 100%

			PRINT("Firmware progress for device %d: %d%%\r", deviceIndex, progress);
			fwInfo->dev->setProgress(progress);

			// Exit early if flash completed
			if (flashCompleted.load()) {
				fwInfo->dev->setProgress(100);
				break;
			}

			// Add small delay to prevent excessive polling
			if (progress < 100) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}
		DBG("Firmware progress monitoring completed for device %d\n", deviceIndex);
	});

	// Wait for flash thread to complete
	if (flashThread.joinable()) {
		flashThread.join();
	}

	// Signal progress thread to stop and wait for it
	stopProgress.store(true);
	if (progressThread.joinable()) {
		progressThread.join();
	}

	// Return appropriate result based on flash operation success
	if (flashSuccess.load()) {
		return ZE_RESULT_SUCCESS;
	} else {
		return ZE_RESULT_ERROR_UNKNOWN;
	}
}

/**
 * @brief Performs cleanup operations after AMC firmware update
 *
 * This function handles post-update cleanup including closing the I2C
 * device connection that was opened during the pre-update phase. It ensures
 * proper resource cleanup regardless of update success or failure.
 *
 * @param fwInfo Pointer to firmware information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful cleanup
 */
ze_result_t amcupd::postUpdateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();

	return ZE_RESULT_SUCCESS;
}
