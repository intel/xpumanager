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
#include "pldm.h"

/**
 * @brief Execute pldm discovery command
 *
 * Sends a pldm discovery command to query device capabilities and supported
 * pldm types. This is part of the device discovery and initialization process.
 *
 * @param cmd pldm discovery command code to execute
 * @param size Size of the command payload in bytes
 * @param pldmVersionType pldm version type parameter for version queries
 *
 * @return uint8_t Status of the discovery command execution
 * @retval PLDM_SUCCESS Command executed successfully
 * @retval PLDM_ERROR Communication failure or invalid response
 *
 * @note Used during initialization to discover device pldm capabilities
 * @note Manages pldm instance ID automatically with wraparound
 */
uint8_t pldm::discoveryCmd(uint8_t cmd, uint8_t size, uint8_t pldmVersionType)
{
	TRACING();
	uint8_t pldmCmdLen = size;
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;
	uint8_t *rptr = (uint8_t *)mI2cPldmRead;

	if (instanceID == PLDM_INSTANCE_ID_MAX) {
		instanceID = 1;
	}

	// UA will calculate this PAK_SEQ for multi packet messages, currently we are not using multi packet messages
	commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (pldmCmdLen - 3),
						MCTP_INTEGRITY_CHECK);
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;
	pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, instanceID, PLDM_MESSAGE_DISCOVERY, cmd, PLDM_ASYNC_REQUEST_NOTIFY,
						PLDM_REQUEST);

	if (cmd == PLDM_GETVERSION) {
		pldmDiscoveryGetVersionPayload(pldmVersionType, pldmCmdLen);
	} else if (cmd == PLDM_GETCOMMANDS) {
		pldmDiscoveryGetCmdPayload(pldmVersionType, pldmCmdLen);
	} else {
		if (pldmDiscoveryFillPayload(cmd, pldmCmdLen) != PLDM_SUCCESS) {
			ERR("pldm Discovery : Fill payload failed for command 0x%02x\n", cmd);
			return PLDM_ERROR;
		}
	}

	DBG("pldm TX  :: ");
	hexdump((uint8_t *)mI2cPldmWrite, pldmCmdLen);

	if (i2cobj->writeAmc(wptr + 1, pldmCmdLen) != true) {
		ERR("pldm Discovery : I2C Write failure\n");
		return PLDM_ERROR;
	}

	// Wait for I2C event
	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);

	if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) != true) {
		ERR("pldm Discovery : I2C Read failure\n");
		return PLDM_ERROR;
	}

	if (discoveryResponse(cmd, instanceID, pldmVersionType) != PLDM_SUCCESS) {
		return PLDM_ERROR;
	}

	instanceID++;
	return PLDM_SUCCESS;
}

/**
 * @brief Fill payload data for pldm discovery commands
 *
 * Constructs the appropriate payload data based on the specified discovery
 * command type. Handles different discovery operations like TID setting,
 * version queries, and command queries.
 *
 * @param cmd pldm discovery command code that determines payload structure
 * @param pldmCmdLen Expected length of the command payload in bytes
 *
 * @return uint8_t Status of payload construction
 * @retval PLDM_SUCCESS Payload filled successfully
 * @retval PLDM_ERROR Invalid command or payload construction failure
 *
 * @note Automatically calculates and appends CRC8 checksum to payload
 */
uint8_t pldm::pldmDiscoveryFillPayload(uint8_t cmd, uint8_t pldmCmdLen)
{
	TRACING();
	uint8_t ret = PLDM_SUCCESS;

	switch (cmd) {
	case PLDM_SETTID:
		if (mPldmRespInfo.tid == 0x00) {
			mI2cPldmWrite->respPayload[BYTE_0] = PLDM_DISCOVERY_TID;
			mI2cPldmWrite->respPayload[BYTE_1] = crc8Smbus(mI2cPldmWrite->respPayload, pldmCmdLen);
		} else {
			mI2cPldmWrite->respPayload[BYTE_0] = mPldmRespInfo.tid; // Set TID
			mI2cPldmWrite->respPayload[BYTE_1] = crc8Smbus(mI2cPldmWrite->respPayload, pldmCmdLen);
		}
		break;

	case PLDM_GETTID:
	case PLDM_GETTYPES:
		// No Payload data
		mI2cPldmWrite->respPayload[BYTE_0] = crc8Smbus(mI2cPldmWrite->respPayload, pldmCmdLen);
		break;

	default:
		ERR("pldm Invalid Discovery Command\n");
		ret = PLDM_ERROR;
		break;
	}

	return ret;
}

/**
 * @brief Construct payload for pldm GetVersion command
 *
 * Builds the payload structure for a pldm GetVersion command according to
 * the pldm specification v240. Sets the GetFirstPart flag and message type.
 *
 * @param msg_type pldm message type to query version information for
 * @param pldmCmdLen Total command length for CRC calculation
 *
 * @note Payload bytes 0-3 are set to 0 when GetFirstPart = 1 per spec
 * @note GetFirstPart is set to 1 to request the first part of version info
 * @note Automatically calculates and appends CRC8 checksum
 */
void pldm::pldmDiscoveryGetVersionPayload(uint8_t msg_type, uint8_t pldmCmdLen)
{
	TRACING();

	// Payload 0-3 bytes is set to 0 if GetFirstPart = 1, as per specv240
	mI2cPldmWrite->respPayload[BYTE_0] = 0;
	mI2cPldmWrite->respPayload[BYTE_1] = 0;
	mI2cPldmWrite->respPayload[BYTE_2] = 0;
	mI2cPldmWrite->respPayload[BYTE_3] = 0;
	mI2cPldmWrite->respPayload[BYTE_4] = 1; // GetFirstPart = 1
	mI2cPldmWrite->respPayload[BYTE_5] = msg_type;
	mI2cPldmWrite->respPayload[BYTE_6] = crc8Smbus(mI2cPldmWrite->respPayload, pldmCmdLen);
}

/**
 * @brief Construct payload for pldm GetCommands command
 *
 * Builds the payload structure for a pldm GetCommands command by looking up
 * the version information for the specified pldm type and populating the
 * version fields (alpha, update, minor, major).
 *
 * @param type pldm type to query supported commands for
 * @param pldmCmdLen Total command length for CRC calculation
 *
 * @note Searches through versionInfo array to find matching pldm type
 * @note Populates version fields from stored version information
 * @note Automatically calculates and appends CRC8 checksum
 */
void pldm::pldmDiscoveryGetCmdPayload(uint8_t type, uint8_t pldmCmdLen)
{
	TRACING();
	mI2cPldmWrite->respPayload[BYTE_0] = type;

	for (int i = 0; i < mPldmRespInfo.totalSupportedTypes; i++) {
		if (type == versionInfo[i].type) {
			mI2cPldmWrite->respPayload[BYTE_1] = versionInfo[i].alpha;
			mI2cPldmWrite->respPayload[BYTE_2] = versionInfo[i].update;
			mI2cPldmWrite->respPayload[BYTE_3] = versionInfo[i].minor;
			mI2cPldmWrite->respPayload[BYTE_4] = versionInfo[i].major;
			break;
		}
	}
	mI2cPldmWrite->respPayload[BYTE_5] = crc8Smbus(mI2cPldmWrite->respPayload, pldmCmdLen);
}

/**
 * @brief Initialize pldm discovery sequence
 *
 * Performs the complete pldm discovery initialization sequence by executing
 * a series of discovery commands in the proper order:
 * 1. GetTypes - Discover supported pldm types
 * 2. GetVersion - Query version for each supported type
 * 3. GetCommands - Query supported commands for each type
 * 4. GetTid - Retrieve current Terminal ID
 * 5. SetTid - Set Terminal ID for communication
 *
 * @return uint8_t Status of the discovery initialization
 * @retval PLDM_SUCCESS All discovery commands completed successfully
 * @retval PLDM_ERROR One or more discovery commands failed
 *
 * @note This function must be called during device initialization
 * @note Failure of any command in the sequence results in overall failure
 * @note Provides detailed console output for debugging and monitoring
 */
uint8_t pldm::pldmDiscInitialize()
{
	TRACING();

	DBG("\n=====================pldm DISCOVERY INIT===============================\n");
	// GET TYPES
	DBG(">>> Send GetTypes\n");
	if (discoveryCmd(PLDM_GETTYPES, PLDM_GETTYPES_SIZE, 0) != PLDM_SUCCESS) {
		ERR("pldm Discovery : GETTYPES command failed\n");
		return PLDM_ERROR;
	}
	DBG("<<< Pldm GetTypes Success...\n");

	// GET VERSION
	DBG(">>> Send GetVersion\n");
	for (int i = 0; i < mPldmRespInfo.totalSupportedTypes; i++) {
		DBG("PLDM_GETVERSION for 0x%02x\n", versionInfo[i].type);
		if (discoveryCmd(PLDM_GETVERSION, PLDM_GETVERSION_SIZE, versionInfo[i].type) != PLDM_SUCCESS) {
			ERR("pldm Discovery : GETVERSION command failed for type 0x%02x\n", versionInfo[i].type);
			return PLDM_ERROR;
		}
	}
	DBG("<<< Pldm GetVersion Success...\n");

	// GET COMMAND
	DBG(">>> Send GetCommands\n");
	for (int i = 0; i < mPldmRespInfo.totalSupportedTypes; i++) {
		DBG("PLDM_GETCOMMANDS for 0x%02x\n", versionInfo[i].type);
		if (discoveryCmd(PLDM_GETCOMMANDS, PLDM_GETCOMMANDS_SIZE, versionInfo[i].type) != PLDM_SUCCESS) {
			ERR("pldm Discovery : GETCOMMANDS command failed for type 0x%02x\n", versionInfo[i].type);
			return PLDM_ERROR;
		}
	}
	DBG("<<< Pldm GetCommands Success...\n");

	// GET TID
	DBG(">>> Send GetTid\n");
	if (discoveryCmd(PLDM_GETTID, PLDM_GETTID_SIZE, 0) != PLDM_SUCCESS) {
		ERR("pldm Discovery : GETTID command failed\n");
		return PLDM_ERROR;
	}
	DBG("<<< Pldm GetTid Success...\n");

	// SET TID
	DBG(">>> Send SetTid\n");
	if (discoveryCmd(PLDM_SETTID, PLDM_SETTID_SIZE, 0) != PLDM_SUCCESS) {
		ERR("pldm Discovery : SETTID command failed\n");
		return PLDM_ERROR;
	}
	DBG("<<< Pldm SetTid Success...\n");

	return PLDM_SUCCESS;
}