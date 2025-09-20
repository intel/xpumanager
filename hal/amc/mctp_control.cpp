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

#include "common.h"
#include "mctp.h"
#include "os.h"
#include <cstring>

/**
 * @brief Execute mctp control command
 *
 * Sends an mctp (Management Component Transport Protocol) control command
 * to the target device. Manages instance ID, constructs command packet,
 * and handles the communication protocol.
 *
 * @param cmd mctp control command code to execute
 * @param size Size of the command payload in bytes
 *
 * @return uint8_t Status of the command execution
 * @retval MCTP_SUCCESS Command executed successfully
 * @retval MCTP_ERROR_* Various error conditions
 *
 * @note Automatically manages mctp instance ID wrapping at maximum value
 */
uint8_t mctp::command(uint8_t cmd, uint8_t size)
{
	TRACING();
	uint8_t mctpCmdLen = size;
	uint8_t *rptr = (uint8_t *)mI2cMctpRead;
	uint8_t *wptr = (uint8_t *)mI2cMctpWrite;

	if (instanceID == m_mctp_instance_id_MAX) {
		instanceID = 1;
	}

	// UA will calculate this PAK_SEQ for multi packet messages, currently we are not using multi packet messages
	commandConstruction(&mI2cMctpWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (size - 3),
						MCTP_INTEGRITY_CHECK);
	headerConstruction(&mI2cMctpWrite->mctpCtrlHdr, instanceID, cmd);
	fillPayload(cmd, mctpCmdLen);

	DBG("mctp TX  :: ");
	hexdump((uint8_t *)mI2cMctpWrite, mctpCmdLen);

	if (i2cobj->writeAmc(wptr + 1, mctpCmdLen) != true) {
		ERR("mctp Control : I2C Write failure\n");
		return MCTP_FAILURE;
	}

	// Wait for I2C event
	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);

	if (i2cobj->readAmc(rptr + 1, MCTP_MAX_RESPONSE_SIZE) != true) {
		ERR("mctp Control : I2C Read failure\n");
		return MCTP_FAILURE;
	}

	if (controlResponse(cmd, instanceID) != MCTP_SUCCESS) {
		return MCTP_FAILURE;
	}

	instanceID++;
	return MCTP_SUCCESS;
}

/**
 * @brief Fill payload data for mctp control commands
 *
 * Constructs the appropriate payload data based on the specified mctp control
 * command type. Different commands require different payload structures and data.
 *
 * @param cmd mctp control command code that determines payload structure
 * @param mctpCmdLen Expected length of the command payload in bytes
 *
 * @return uint8_t Status of payload construction
 * @retval MCTP_SUCCESS Payload filled successfully
 * @retval MCTP_FAILURE Invalid command or payload construction failure
 *
 * @note Handles endpoint ID management for SET_ENDPOINT_ID command
 * @note Some commands like GET_ENDPOINT_ID require no payload data
 * @note Automatically calculates and appends CRC8 checksum to payload
 */
uint8_t mctp::fillPayload(uint8_t cmd, uint8_t mctpCmdLen)
{
	TRACING();
	uint8_t ret = MCTP_SUCCESS;

	// Initialize payload buffer to prevent reading uninitialized memory during CRC calculation
	memset(mI2cMctpWrite->respPayload, 0, mctpCmdLen);

	switch (cmd) {
	case MCTP_SET_ENDPOINT_ID:
		mI2cMctpWrite->respPayload[BYTE_0] = 0x00; // SET EID Operation

		if (mDestEid == 0) {
			mI2cMctpWrite->respPayload[BYTE_1] = mDestNewEid; // Set new EID
		} else {
			mI2cMctpWrite->respPayload[BYTE_1] = mDestEid; // Use existing EID
		}
		mI2cMctpWrite->respPayload[BYTE_2] = crc8Smbus(mI2cMctpWrite->respPayload, mctpCmdLen - 1);
		break;

	case MCTP_GET_ENDPOINT_ID:
	case MCTP_GET_ENDPOINT_UUID:
	case MCTP_GET_MESSAGE_TYPE:
		// No Payload data
		mI2cMctpWrite->respPayload[BYTE_0] = crc8Smbus(mI2cMctpWrite->respPayload, mctpCmdLen - 1);
		break;
	case MCTP_GET_VERSION:
		mI2cMctpWrite->respPayload[BYTE_0] = 0x00; // Return mctp control protocol message version info
		mI2cMctpWrite->respPayload[BYTE_1] = crc8Smbus(mI2cMctpWrite->respPayload, mctpCmdLen - 1);
		break;
	default:
		ERR("mctp Invalid Control Command\n");
		ret = MCTP_FAILURE;
		break;
	}

	return ret;
}

/**
 * @brief Initialize mctp control protocol
 *
 * Performs the complete mctp initialization sequence by executing a series
 * of control commands in the proper order:
 * 1. GetVersion - Query mctp protocol version
 * 2. GetEID - Retrieve current Endpoint ID
 * 3. GetEUUID - Get Endpoint UUID
 * 4. SetEID - Set destination Endpoint ID (with retry logic)
 * 5. GetMessageType - Query supported message types
 *
 * @return int Status of the mctp initialization
 * @retval MCTP_SUCCESS All mctp commands completed successfully
 * @retval MCTP_FAILURE One or more mctp commands failed
 *
 * @note This function must be called during device initialization
 * @note SetEID command includes retry logic with different EID values
 * @note Skips SetEID if destination EID is already configured
 * @note Provides detailed console output for debugging and monitoring
 */
int mctp::initialize()
{
	TRACING();

	DBG("\n=====================mctp CTRL INIT===============================\n");
	// GET VERSION
	DBG(">>> Send GetVersion\n");
	if (command(MCTP_GET_VERSION, MCTP_GET_VERSION_SIZE) != MCTP_SUCCESS) {
		ERR("MCTP_GET_VERSION command failed\n");
		return MCTP_FAILURE;
	}
	DBG("<<< Mctp GetVersion Success...\n");

	// GET EID
	DBG(">>> Send GetEID\n");
	if (command(MCTP_GET_ENDPOINT_ID, MCTP_GET_EID_CMD_SIZE) != MCTP_SUCCESS) {
		ERR("MCTP_GET_ENDPOINT_ID command failed\n");
		return MCTP_FAILURE;
	}
	DBG("<<< Mctp GetEID Success...\n");

	// GET EUUID
	DBG(">>> Send GetEUUID\n");
	if (command(MCTP_GET_ENDPOINT_UUID, MCTP_GET_EID_UUID_CMD_SIZE) != MCTP_SUCCESS) {
		ERR("MCTP_GET_ENDPOINT_UUID command failed\n");
		return MCTP_FAILURE;
	}
	DBG("<<< Mctp GetEUUID Success...\n");

	// SET Destination EID
	if (mDestEid == 0) {
		for (uint8_t i = 0; i < RETRY_COUNT; i++) {
			DBG(">>> Send SetEID\n");
			mDestNewEid = MCTP_DEST_SET_EID + i;
			DBG("Mctp SetEID:: 0x%02x\n", mDestNewEid);
			if (command(MCTP_SET_ENDPOINT_ID, MCTP_SET_EID_CMD_SIZE) != MCTP_SUCCESS) {
				if (i == (RETRY_COUNT - 1)) {
					ERR("MCTP_SET_ENDPOINT_ID command failed after %d retries\n", RETRY_COUNT);
					return MCTP_FAILURE;
				}
				DBG("Retry setting MCTP_SET_ENDPOINT_ID again...\n");
			} else {
				DBG("<<< Mctp SetEID Success...\n");
				break;
			}
		}
	} else {
		DBG("Mctp EID already set to 0x%02x, Skipping SET EID CMD \n", mDestEid);
	}

	// GET MESSAGE TYPE
	DBG(">>> Send GetMessageType\n");
	if (command(MCTP_GET_MESSAGE_TYPE, MCTP_GET_MSG_TYPE_SIZE) != MCTP_SUCCESS) {
		ERR("MCTP_GET_MESSAGE_TYPE command failed\n");
		return MCTP_FAILURE;
	}
	DBG("<<< Mctp GetMessageType Success...\n");

	return MCTP_SUCCESS;
}
