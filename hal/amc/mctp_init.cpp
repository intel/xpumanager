/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "mctp.h"

/**
 * @brief Constructor for mctp class
 * @param devpath Wide character string path to the device to initialize mctp communication with
 *
 * Initializes an mctp (Management Component Transport Protocol) instance by calling mctpinit().
 * Sets the init flag based on the success of the initialization process.
 */
mctp::mctp(const std::string &devpath)
{
	TRACING();
	mI2cMctpRead = NULL;
	mI2cMctpWrite = NULL;
	i2cobj = NULL;
	instanceID = 0;
	mDestEid = 0;
	mDestNewEid = 0;

	if (devpath.empty()) {
		ERR("Device path is empty\n");
		init = false;
	} else {
		if (!mctpinit(devpath)) {
			init = true;
		} else {
			init = false;
		}
	}
}

/**
 * @brief Destructor for mctp class
 *
 * Cleans up all allocated resources by calling cleanup() to ensure proper cleanup
 * of memory and I2C interface objects.
 */
mctp::~mctp()
{
	TRACING();
	cleanup();
}

/**
 * @brief Cleans up and deallocates all mctp resources
 *
 * Frees dynamically allocated memory for mctp read buffer and deletes the I2C interface object.
 * Sets all pointers to NULL after cleanup to prevent dangling pointer issues.
 */
void mctp::cleanup()
{
	TRACING();

	if (mI2cMctpRead) {
		free(mI2cMctpRead);
		mI2cMctpRead = NULL;
	}

	if (mI2cMctpWrite) {
		free(mI2cMctpWrite);
		mI2cMctpWrite = NULL;
	}

	if (i2cobj) {
		delete i2cobj;
		i2cobj = NULL;
	}
	DBG("mctp Cleanup Success!!\n");
}

/**
 * @brief Initializes mctp communication interface
 * @param devpath Wide character string path to the I2C device
 * @return int Status of initialization
 * @retval MCTP_SUCCESS Initialization successful
 * @retval MCTP_FAILURE Memory allocation failed or initialization error
 *
 * Sets up mctp instance ID and destination EID, allocates memory buffers for read/write operations,
 * creates and initializes the I2C interface object. Performs cleanup on any initialization failures.
 */
int mctp::mctpinit(const std::string &devpath)
{
	TRACING();

	if (devpath.empty()) {
		ERR("Device path is empty\n");
		return MCTP_FAILURE;
	}

	// Initialize instanceID to "1"
	instanceID = 1;

	// Initialize Dest EID to "0"
	mDestEid = 0;

	mI2cMctpRead = (i2cdata_mctpinfo *)malloc(sizeof(struct mctpSmbusI2cHdr) + sizeof(mctpControlHdr) +
											  MCTL_PLDM_RESPONSE_PAYLOAD_SIZE);
	if (mI2cMctpRead == NULL) {
		ERR("Memory allocation failed for mctp data structure\n");
		return MCTP_FAILURE;
	}

	mI2cMctpWrite = (i2cdata_mctpinfo *)malloc(sizeof(struct mctpSmbusI2cHdr) + sizeof(mctpControlHdr) +
											   MCTL_PLDM_RESPONSE_PAYLOAD_SIZE);
	if (mI2cMctpWrite == NULL) {
		ERR("Memory allocation failed for mctp write data structure\n");
		return MCTP_FAILURE;
	}

	i2cobj = new I2CInterface(devpath);
	if (i2cobj == NULL) {
		ERR("Failed to allocate I2C interface\n");
		return MCTP_FAILURE;
	}

	if (!i2cobj->isInit()) {
		ERR("Failed to initialize I2C interface\n");
		return MCTP_FAILURE;
	}

	DBG("mctp Init Success!!\n");
	return MCTP_SUCCESS;
}
