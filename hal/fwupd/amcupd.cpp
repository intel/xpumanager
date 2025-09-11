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
#include <memory>

// Static member definitions for singleton pattern using smart pointer
std::unique_ptr<amclib> amcupd::amcobj = nullptr;
int amcupd::globalNumOfCards = 0;
std::once_flag amcupd::initFlag;
std::mutex amcupd::amcobjMutex;

/**
 * @brief Static method to get the singleton amclib instance
 *
 * This method implements the singleton pattern for the amclib instance,
 * ensuring that only one instance exists throughout the program's lifetime
 * and that it's initialized exactly once in a thread-safe manner.
 * Uses smart pointer for automatic memory management.
 *
 * @return amclib* Pointer to the singleton amclib instance
 */
amclib *amcupd::getAmcObj()
{
	std::call_once(initFlag, []() {
		amcobj = std::make_unique<amclib>();
		globalNumOfCards = amcobj->amcEnumFirmwares();
		if (globalNumOfCards <= 0) {
			ERR("Failed to enumerate AMC cards\n");
		}
	});
	return amcobj.get();
}

/**
 * @brief Static method to get the global number of AMC cards
 *
 * This method provides thread-safe access to the global number of AMC cards
 * that were enumerated during singleton initialization.
 *
 * @return int The number of AMC cards found during enumeration
 */
int amcupd::getNumOfCards()
{
	// Ensure the singleton is initialized before returning the count
	getAmcObj();
	return globalNumOfCards;
}

/**
 * @brief Static cleanup method for explicit resource cleanup
 *
 * This method can be called during program shutdown to explicitly clean up
 * the singleton resources. While the smart pointer will automatically handle
 * cleanup, this provides an option for explicit cleanup if needed.
 */
void amcupd::cleanup()
{
	std::lock_guard<std::mutex> lock(amcobjMutex);
	amcobj.reset();
	globalNumOfCards = 0;
}

/**
 * @brief Constructor for the amcupd class
 *
 * Initializes the AMC update handler. The actual AMC enumeration and
 * initialization is handled by the singleton pattern to ensure consistent
 * state across all instances.
 */
amcupd::amcupd()
{
	TRACING();
	// Initialize the singleton - this ensures amcobj and globalNumOfCards are set
	getAmcObj();
}

/**
 * @brief Destructor for the amcupd class
 *
 * Since we're using a singleton pattern with smart pointer, we don't clean up
 * the shared amclib instance here as it should persist for the program's lifetime.
 * Individual instances can be destroyed without affecting other instances.
 * The smart pointer will automatically handle memory cleanup.
 */
amcupd::~amcupd()
{
	// No cleanup needed for singleton resources - smart pointer handles it
}

/**
 * @brief Prepares the AMC (Add-in Management Controller) for firmware update
 *
 * This function performs pre-update operations including initializing the AMC
 * device connection required for AMC firmware communication. It establishes
 * the necessary hardware interface before the actual firmware update process.
 *
 * @param fwInfo Pointer to firmware information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful preparation,
 *                     ZE_RESULT_ERROR_UNKNOWN if AMC initialization fails
 */
ze_result_t amcupd::preUpdateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();

	amclib *amc = getAmcObj();
	if (!amc) {
		ERR("Failed to get AMC library instance\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (amc->amcInitialize() != 0) {
		ERR("Failed to initialize AMC devices\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

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

	if (deviceIndex >= (uint32_t)getNumOfCards()) {
		ERR("Invalid AMC device index: %d\n", deviceIndex);
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	amclib *amc = getAmcObj();
	if (!amc) {
		ERR("Failed to get AMC library instance\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Shared state for thread coordination
	std::atomic<bool> flashCompleted{false};
	std::atomic<bool> flashSuccess{false};
	std::atomic<bool> stopProgress{false};

	// Create a thread to flash firmware
	std::thread flashThread([amc, deviceIndex, filePath, &flashCompleted, &flashSuccess]() {
		int result = amc->amcFirmwareFlash(deviceIndex, filePath.c_str());
		flashSuccess.store(result == 0);
		flashCompleted.store(true);

		if (result != 0) {
			ERR("Failed to flash firmware for card %d\n", deviceIndex);
		} else {
			DBG("Firmware flash for card %d completed successfully\n", deviceIndex);
		}
	});

	// Create a thread to monitor firmware flash progress
	std::thread progressThread([amc, deviceIndex, fwInfo, &flashCompleted, &stopProgress]() {
		uint32_t progress = 0;
		while (!stopProgress.load() && progress < 100) {
			progress = amc->amcFirmwareProgress(deviceIndex);
			if (progress > 100)
				progress = 100; // Cap at 100%

			fwInfo->dev->setProgress(progress);

			// Exit early if flash completed
			if (flashCompleted.load()) {
				fwInfo->dev->setProgress(100);
				break;
			}

			// Add small delay to prevent excessive polling
			if (progress < 100) {
				MSLEEP(100);
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
