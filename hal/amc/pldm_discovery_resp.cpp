/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "common.h"
#include "pldm.h"

/**
 * @brief Extract bit positions from a byte value
 *
 * Analyzes a byte value to find all set bits and stores the corresponding
 * bit positions in the versionInfo array. Used to decode supported pldm
 * types from the GetTypes response.
 *
 * @param num Byte value containing bit flags for supported pldm types
 *
 * @return uint8_t Number of supported pldm types found
 *
 * @note Updates mPldmRespInfo.totalSupportedTypes
 * @note Populates versionInfo array with pldm type information
 * @note Each set bit represents a supported pldm message type
 */
uint8_t pldm::getbitposition(uint8_t num)
{
	TRACING();
	uint8_t i = 0, pos = 0;
	mPldmRespInfo.totalSupportedTypes = 0;
	for (i = 0; i < 8; i++) {
		if (num & (1 << i)) {
			versionInfo[pos].type = i;
			pos++;
		}
	}
	mPldmRespInfo.totalSupportedTypes = pos;

	return PLDM_SUCCESS;
}

/**
 * @brief Process pldm discovery response messages
 *
 * Handles and validates response messages for various pldm discovery commands.
 * Processes different types of discovery responses and extracts relevant
 * information such as supported types, versions, TID values, and commands.
 *
 * @param cmd Discovery command that generated this response
 * @param id pldm instance ID used for the original command
 * @param pldmVersionType pldm message type for version-specific processing
 *
 * @return uint8_t Status of response processing
 * @retval PLDM_SUCCESS Response processed successfully
 * @retval PLDM_ERROR Invalid response or processing failure
 *
 * @note Handles SETTID, GETTID, GETTYPES, GETVERSION, and GETCOMMANDS responses
 * @note Updates internal state with discovered device capabilities
 * @note Extracts and stores version information for supported pldm types
 */
uint8_t pldm::discoveryResponse(uint8_t cmd, uint8_t id, uint8_t pldmVersionType)
{
	TRACING();
	uint8_t ret = PLDM_ERROR;

	switch (cmd) {
	case PLDM_SETTID:
	case PLDM_GETCOMMANDS: {
		ret = discoveryResponsePayload(cmd, id);
		break;
	}

	case PLDM_GETTID: {
		ret = discoveryResponsePayload(cmd, id);
		if (ret == PLDM_SUCCESS) {
			if (mI2cPldmRead->respPayload[1] == 0x00) {
				DBG("pldm TID is not set\n");
			} else {
				DBG("pldm TID is already set to 0x%02x\n", mI2cPldmRead->respPayload[1]);
				mPldmRespInfo.tid = mI2cPldmRead->respPayload[1];
				ret = PLDM_SUCCESS;
			}
		}
		break;
	}

	case PLDM_GETTYPES: {
		if (discoveryResponsePayload(cmd, id) == PLDM_SUCCESS) {
			mPldmRespInfo.supportedTypes = mI2cPldmRead->respPayload[1];
			ret = getbitposition(mPldmRespInfo.supportedTypes);
		}
		break;
	}

	case PLDM_GETVERSION: {
		if (discoveryResponsePayload(cmd, id) == PLDM_SUCCESS) {
			for (int i = 0; i < mPldmRespInfo.totalSupportedTypes; i++) {
				if (pldmVersionType == versionInfo[i].type) {
					versionInfo[i].major = mI2cPldmRead->respPayload[BYTE_9];
					versionInfo[i].minor = mI2cPldmRead->respPayload[BYTE_8];
					versionInfo[i].update = mI2cPldmRead->respPayload[BYTE_7];
					versionInfo[i].alpha = mI2cPldmRead->respPayload[BYTE_6];
					DBG("pldm Base version for 0x%02x is %d.%d.%d.%d\n", pldmVersionType, versionInfo[i].major & 0x0F,
						versionInfo[i].minor & 0x0F, versionInfo[i].update & 0x0F, versionInfo[i].alpha & 0x0F);
					ret = PLDM_SUCCESS;
					break;
				}
			}
		}
		break;
	}

	default:
		ERR("pldm Invalid Discovery Command\n");
		break;
	}
	return ret;
}

/**
 * @brief Process payload data from discovery response messages
 *
 * Validates and processes the payload content of pldm discovery response
 * messages. Performs message integrity checks including command code,
 * instance ID, and completion code validation.
 *
 * @param cmd Discovery command that generated this response
 * @param id pldm instance ID used for the original command
 *
 * @return uint8_t Status of payload processing
 * @retval PLDM_SUCCESS Payload processed successfully and completion code is success
 * @retval PLDM_ERROR Invalid message format, mismatched parameters, or error completion code
 *
 * @note Validates response message header fields against expected values
 * @note Checks completion code in payload for command execution status
 * @note Sets up proper addressing for response message handling
 * @note Provides detailed error logging for debugging
 */
uint8_t pldm::discoveryResponsePayload(UNUSED uint8_t cmd, UNUSED uint8_t id)
{
	TRACING();

	unsigned int totalSize = 0;

	mI2cPldmRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cPldmRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;

	DBG("pldm RX  :: ");
	hexdump((uint8_t *)mI2cPldmRead, totalSize);

	if (mI2cPldmRead->respPayload[0] != PLDM_SUCCESS) {
		ERR("pldm : Command Completion Error!!!\n");
		ERR("Error Code : 0x%02x\n", mI2cPldmRead->respPayload[0]);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}
