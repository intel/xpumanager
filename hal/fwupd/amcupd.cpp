/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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
std::once_flag amcupd::enumFlag;
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
	std::call_once(enumFlag, []() {
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

/*
 * @brief Initialize AMC devices
 *
 * This function initializes the AMC devices by calling the amcInitialize()
 * method of the amclib instance. It ensures that initialization is performed
 * only once across all threads using std::call_once.
 *
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization,
 *                     ZE_RESULT_ERROR_UNKNOWN if initialization fails
 */
ze_result_t amcupd::init()
{
	TRACING();

	amclib *amc = getAmcObj();
	if (!amc) {
		ERR("Failed to get AMC library instance\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Flag to track initialization success
	static std::atomic<bool> initFailed{false};

	std::call_once(initFlag, [amc]() {
		if (amc->amcInitialize() != AMC_SUCCESS) {
			ERR("Failed to initialize AMC devices\n");
			initFailed.store(true);
		}
	});

	// Check if initialization failed
	if (initFailed.load()) {
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	return ZE_RESULT_SUCCESS;
}

/*
 * @brief Find the index of an AMC card associated with the specified GPU BDF
 *
 * Searches the discovered AMC device list to find the index of the card
 * associated with the given GPU Bus-Device-Function (BDF) identifier.
 *
 * @param gpuBDF The BDF string of the GPU to check (e.g., "0000:00:02.0")
 * @return Index of the AMC card in the device list if found, -1 if not found
 */
int amcupd::amcGetIndex(const std::string &gpuBDF)
{
	int ret = -1;
	amclib *amc = getAmcObj();
	ret = amc->amcGetIndex(gpuBDF);
	return ret;
}

/*
 * @brief Retrieve AMC card information (serial number and version) by GPU BDF
 *
 * This function retrieves the serial number and version of the AMC card
 * associated with the specified GPU Bus-Device-Function (BDF) identifier.
 * It ensures that the AMC devices are initialized before querying for information.
 *
 * @param gpuBDF The BDF string of the GPU to check (e.g., "0000:00:02.0")
 * @param serialNum Reference to string to receive the serial number
 * @param version Reference to string to receive the version
 * @return int 0 on success, -1 on failure
 */
int amcupd::amcGetCardInfo(std::string gpuBDF, std::string &serialNum, std::string &version)
{
	int ret = -1;

	// Call init here and make sure that it returns success. The reason is that we will need to send some i2c commands
	// to collect FRU data like version and name of the AMC
	ze_result_t initResult = init();
	if (initResult != ZE_RESULT_SUCCESS) {
		ERR("Failed to initialize AMC devices before finding BDF\n");
		return -1;
	}

	amclib *amc = getAmcObj();
	ret = amc->amcGetCardInfo(gpuBDF, serialNum, version);
	return ret;
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
	return init();
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
	uint32_t amcIndex = fwInfo->amcIndex;

	if (amcIndex >= (uint32_t)getNumOfCards()) {
		ERR("Invalid AMC device index: %d\n", amcIndex);
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
	std::thread flashThread([amc, amcIndex, filePath, &flashCompleted, &flashSuccess]() {
		int result = amc->amcFirmwareFlash(amcIndex, filePath.c_str());
		flashSuccess.store(result == AMC_SUCCESS);
		flashCompleted.store(true);

		if (result != AMC_SUCCESS) {
			ERR("Failed to flash firmware for card %d\n", amcIndex);
		} else {
			DBG("Firmware flash for card %d completed successfully\n", amcIndex);
		}
	});

	// Create a thread to monitor firmware flash progress
	std::thread progressThread([amc, amcIndex, fwInfo, &flashCompleted, &stopProgress]() {
		int progress = 0;
		while (!stopProgress.load() && progress < 100) {
			progress = amc->amcFirmwareProgress(amcIndex);

			if (progress < 0) {
				ERR("Failed to get firmware progress for card %d\n", amcIndex);
				break;
			}

			if (progress > 100)
				progress = 100; // Cap at 100%

			SETPROGRESS(amcIndex, fwInfo->curThread, fwInfo->totalThreads, progress);

			// Exit early if flash completed
			if (flashCompleted.load()) {
				SETPROGRESS(amcIndex, fwInfo->curThread, fwInfo->totalThreads, 100);
				break;
			}

			// Add small delay to prevent excessive polling
			if (progress < 100) {
				MSLEEP(100);
			}
		}
		DBG("Firmware progress monitoring completed for device %d\n", amcIndex);
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
