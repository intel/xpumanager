/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "mctp.h"

/**
 * @brief Constructor for mctp class
 * @param devpath The AMC's I2C device, such as /dev/i2c-3
 *
 * Opens the AMC's I2C device (makeI2cAmcBus()) and initializes mctp on it; see mctp(std::unique_ptr<AmcBus>).
 */
mctp::mctp(const std::string &devpath) : mctp(makeI2cAmcBus(devpath)) {}

/**
 * @brief Constructor for mctp class on a given bus
 * @param bus Bus to the AMC; a test passes a simulated AMC here
 *
 * Initializes an mctp (Management Component Transport Protocol) instance by calling mctpinit().
 * Sets the init flag based on the success of the initialization process.
 */
mctp::mctp(std::unique_ptr<AmcBus> bus) : mBus(std::move(bus))
{
	TRACING();
	mI2cMctpRead = NULL;
	mI2cMctpWrite = NULL;
	mDestEid = 0;
	mDestNewEid = 0;
	init = (mctpinit() == MCTP_SUCCESS);
}

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

	mBus.reset();
	DBG("mctp Cleanup Success!!\n");
}

/**
 * @brief Initializes mctp communication interface on mBus
 * @return int Status of initialization
 * @retval MCTP_SUCCESS Initialization successful
 * @retval MCTP_FAILURE Memory allocation failed or initialization error
 *
 * Sets up mctp instance ID and destination EID, allocates memory buffers for read/write operations,
 * and checks that the bus to the AMC is open.
 */
int mctp::mctpinit()
{
	TRACING();

	if (mBus == nullptr) {
		ERR("No bus to the AMC\n");
		return MCTP_FAILURE;
	}

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

	if (!mBus->isOpen()) {
		ERR("Failed to initialize I2C interface\n");
		return MCTP_FAILURE;
	}

	DBG("mctp Init Success!!\n");
	return MCTP_SUCCESS;
}
