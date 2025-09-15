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

#include "pldm.h"

/**
 * @brief Initialize pldm (Platform Level Data Model) subsystem
 *
 * Allocates memory buffers for pldm read and write operations and initializes
 * the pldm instance ID. Sets up the basic infrastructure needed for pldm
 * communication.
 *
 * @return int Status of initialization
 * @retval 0 pldm initialization successful
 * @retval -1 Memory allocation failed or initialization error
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
		return -1;
	}

	mI2cPldmWrite = (i2cdataPldmInfo *)malloc(sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr) +
											  MCTL_PLDM_RESPONSE_PAYLOAD_SIZE);
	if (mI2cPldmWrite == NULL) {
		cleanup();
		ERR("Memory allocation failed for i2cwriteinfo\n");
		return -1;
	}

	progMutex = new std::mutex();
	if (progMutex == NULL) {
		cleanup();
		ERR("Memory allocation failed for progMutex\n");
		return -1;
	}

	// Initialize pldm Instance ID to "1"
	instanceID = 1;
	mProgPercent = 0;

	DBG("pldm Init Success!!\n");

	return 0;
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
 * @retval 0 All initialization steps completed successfully
 * @retval -1 One or more initialization steps failed
 *
 * @note This function must be called before any pldm operations
 */
int pldm::initialize()
{
	TRACING();
	if (!i2cobj->isInit()) {
		ERR("Failed to initialize I2C interface\n");
		return -1;
	}

	if (mctp::initialize() != MCTP_SUCCESS) {
		ERR("Failed to initialization mctp init sequence\n");
		return -1;
	}

	if (pldmDiscInitialize() != PLDM_SUCCESS) {
		ERR("Failed to initialization pldm discovery sequence\n");
		return -1;
	}

	DBG("mctp - pldm initialization completed successfully for card : %02d\n", mCardNum);
	return 0;
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
 * @retval 0 Firmware update completed successfully
 * @retval -1 I2C interface not initialized or update failed
 *
 * @note The device must be properly initialized before calling this function
 */
int pldm::fwupd(const char *pkgFilePath)
{
	TRACING();
	if (!i2cobj->isInit()) {
		ERR("Failed to initialize I2C interface\n");
		return -1;
	}

	if (fwUpdInitialize(pkgFilePath) != PLDM_SUCCESS) {
		ERR("Firmware update failed\n");
		return -1;
	}
	return 0;
}