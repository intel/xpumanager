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
 * @return 0 on success, -1 on failure
 *
 * Sets up mctp instance ID and destination EID, allocates memory buffers for read/write operations,
 * creates and initializes the I2C interface object. Performs cleanup on any initialization failures.
 */
int mctp::mctpinit(const std::string &devpath)
{
	TRACING();

	if (devpath.empty()) {
		ERR("Device path is empty\n");
		return -1;
	}

	// Initialize instanceID to "1"
	instanceID = 1;

	// Initialize Dest EID to "0"
	mDestEid = 0;

	mI2cMctpRead = (i2cdata_mctpinfo *)malloc(sizeof(struct mctpSmbusI2cHdr) + sizeof(mctpControlHdr) +
											  MCTL_PLDM_RESPONSE_PAYLOAD_SIZE);
	if (mI2cMctpRead == NULL) {
		ERR("Memory allocation failed for mctp data structure\n");
		return -1;
	}

	mI2cMctpWrite = (i2cdata_mctpinfo *)malloc(sizeof(struct mctpSmbusI2cHdr) + sizeof(mctpControlHdr) +
											   MCTL_PLDM_RESPONSE_PAYLOAD_SIZE);
	if (mI2cMctpWrite == NULL) {
		ERR("Memory allocation failed for mctp write data structure\n");
		return -1;
	}

	i2cobj = new I2CInterface(devpath);
	if (i2cobj == NULL) {
		ERR("Failed to allocate I2C interface\n");
		return -1;
	}

	if (!i2cobj->isInit()) {
		ERR("Failed to initialize I2C interface\n");
		return -1;
	}

	DBG("mctp Init Success!!\n");
	return 0;
}
