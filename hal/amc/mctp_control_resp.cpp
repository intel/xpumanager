/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "common.h"
#include "mctp.h"

/**
 * @brief Processes mctp control command responses
 *
 * Handles different types of mctp control command responses by parsing the
 * response payload and extracting relevant information. Updates internal
 * destination endpoint ID (EID) for specific commands.
 *
 * @param cmd mctp control command code that was sent
 * @param id Instance ID used in the original command
 * @return MCTP_SUCCESS if response processed successfully, MCTP_FAILURE on error
 *
 * @note Supported commands:
 *       - MCTP_SET_ENDPOINT_ID: Sets destination EID from response
 *       - MCTP_GET_ENDPOINT_ID: Gets and sets destination EID
 *       - MCTP_GET_ENDPOINT_UUID: Retrieves endpoint UUID
 *       - MCTP_GET_MESSAGE_TYPE: Gets supported message types
 *       - MCTP_GET_VERSION: Retrieves mctp version information
 */
uint8_t mctp::controlResponse(uint8_t cmd, uint8_t id)
{
	TRACING();
	uint8_t ret = MCTP_FAILURE;

	switch (cmd) {
	case MCTP_SET_ENDPOINT_ID: {
		if (getRespPayload(cmd, id) == MCTP_SUCCESS) {
			mDestEid = mI2cMctpRead->respPayload[2]; // Get the Dest EID from the response
			ret = MCTP_SUCCESS;
		}
		break;
	}
	case MCTP_GET_ENDPOINT_ID: {
		if (getRespPayload(cmd, id) == MCTP_SUCCESS) {
			if (mI2cMctpRead->respPayload[1] != 0x00) {
				mDestEid = mI2cMctpRead->respPayload[1]; // EID already assigned, we can use the same EID for XPUM
			}
			ret = MCTP_SUCCESS;
		}
		break;
	}
	case MCTP_GET_ENDPOINT_UUID:
	case MCTP_GET_MESSAGE_TYPE:
	case MCTP_GET_VERSION: {
		ret = getRespPayload(cmd, id);
		break;
	}
	default:
		ERR("mctp Invalid Control Command\n");
		break;
	}

	return ret;
}

/**
 * @brief Validates and processes mctp control response payload
 *
 * Validates the received mctp control response by checking command code,
 * instance ID, response type, and completion status. Provides debug output
 * of the received packet.
 *
 * @param cmd Expected mctp control command code
 * @param id Expected instance ID that was used in the original command
 * @return MCTP_SUCCESS if response is valid and completion successful, MCTP_FAILURE on any validation error
 *
 * @note Performs comprehensive validation:
 *       - Command code matches expected value
 *       - Instance ID matches original request
 *       - Response type indicates this is a response packet
 *       - Completion code indicates success (MCTP_COMPLETION_SUCCESS)
 * @note Provides debug hex dump of received packet for troubleshooting
 */
uint8_t mctp::getRespPayload(UNUSED uint8_t cmd, UNUSED uint8_t id)
{
	TRACING();
	unsigned int totalSize = 0;

	mI2cMctpRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cMctpRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	totalSize = mI2cMctpRead->mctpSmbusHdr.byteCount + 3;

	DBG("mctp RX  :: ");
	hexdump((uint8_t *)mI2cMctpRead, totalSize);

	if (mI2cMctpRead->respPayload[BYTE_0] != MCTP_COMPLETION_SUCCESS) {
		ERR("mctp : Command Completion Error!!!\n");
		ERR("Error Code : 0x{:02x}\n", mI2cMctpRead->respPayload[BYTE_0]);
		return MCTP_FAILURE;
	}

	return MCTP_SUCCESS;
}