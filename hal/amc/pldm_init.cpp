/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm.h"

/**
 * @brief Initialize pldm (Platform Level Data Model) subsystem
 *
 * Allocates memory buffers for pldm read and write operations and initializes
 * the pldm instance ID. Sets up the basic infrastructure needed for pldm
 * communication.
 *
 * @return int Status of initialization
 * @retval PLDM_SUCCESS pldm initialization successful
 * @retval PLDM_ERROR Memory allocation failed or initialization error
 *
 * @note Memory buffers include space for mctp headers, pldm headers, and payload
 * @note Calls cleanup() automatically on failure to prevent memory leaks
 */
int pldm::pldminit()
{
	TRACING();

	mI2cPldmRead = (i2cdataPldmInfo *)malloc(sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr) +
											 MCTL_PLDM_RESPONSE_PAYLOAD_SIZE);
	if (mI2cPldmRead == NULL) {
		cleanup();
		ERR("Memory allocation failed for i2crespinfo\n");
		return PLDM_ERROR;
	}

	mI2cPldmWrite = (i2cdataPldmInfo *)malloc(sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr) +
											  MCTL_PLDM_RESPONSE_PAYLOAD_SIZE);
	if (mI2cPldmWrite == NULL) {
		cleanup();
		ERR("Memory allocation failed for i2cwriteinfo\n");
		return PLDM_ERROR;
	}

	progMutex = new std::mutex();
	if (progMutex == NULL) {
		cleanup();
		ERR("Memory allocation failed for progMutex\n");
		return PLDM_ERROR;
	}

	// Initialize pldm Instance ID to "1"
	instanceID = 1;
	mProgPercent = 0;
	mFruTableInitialized = false;

	DBG("pldm Init Success!!\n");

	return PLDM_SUCCESS;
}

/**
 * @brief Cleanup pldm resources
 *
 * Frees allocated memory buffers for pldm read and write operations.
 * Should be called during shutdown or when initialization fails.
 *
 * @note Sets pointers to NULL after freeing to prevent double-free errors
 * @note Safe to call multiple times or with already-freed pointers
 */
void pldm::cleanup()
{
	TRACING();

	if (mI2cPldmRead != NULL) {
		free(mI2cPldmRead);
		mI2cPldmRead = NULL;
	}

	if (mI2cPldmWrite != NULL) {
		free(mI2cPldmWrite);
		mI2cPldmWrite = NULL;
	}

	if (progMutex != NULL) {
		delete progMutex;
		progMutex = NULL;
	}

	DBG("pldm Cleanup Success!!\n");
}

/**
 * @brief Initialize pldm communication and discovery
 *
 * Performs complete initialization sequence including:
 * 1. Verifying I2C interface is initialized
 * 2. Initializing mctp (Management Component Transport Protocol)
 * 3. Performing pldm device discovery
 *
 * @return int Status of initialization
 * @retval PLDM_SUCCESS All initialization steps completed successfully
 * @retval PLDM_ERROR One or more initialization steps failed
 *
 * @note This function must be called before any pldm operations
 */
int pldm::initialize()
{
	TRACING();
	if (!i2cobj->isInit()) {
		ERR("Failed to initialize I2C interface\n");
		return PLDM_ERROR;
	}

	if (mctp::initialize() != MCTP_SUCCESS) {
		ERR("Failed to initialization mctp init sequence\n");
		return PLDM_ERROR;
	}

	if (pldmDiscInitialize() != PLDM_SUCCESS) {
		ERR("Failed to initialization pldm discovery sequence\n");
		return PLDM_ERROR;
	}

	DBG("mctp - pldm initialization completed successfully for card : {:02}\n", mCardNum);
	return PLDM_SUCCESS;
}

/**
 * @brief Perform firmware update operation
 *
 * Initiates firmware update process using the specified firmware package file.
 * Verifies I2C interface is initialized before proceeding with the update.
 *
 * @param pkgFilePath Path to the firmware package file to flash
 *
 * @return int Status of firmware update operation
 * @retval PLDM_SUCCESS Firmware update completed successfully
 * @retval PLDM_ERROR I2C interface not initialized or update failed
 *
 * @note The device must be properly initialized before calling this function
 */
int pldm::fwupd(const char *pkgFilePath)
{
	TRACING();
	if (!i2cobj->isInit()) {
		ERR("Failed to initialize I2C interface\n");
		return PLDM_ERROR;
	}

	if (fwUpdInitialize(pkgFilePath) != PLDM_SUCCESS) {
		ERR("Firmware update failed\n");
		return PLDM_ERROR;
	}
	return PLDM_SUCCESS;
}