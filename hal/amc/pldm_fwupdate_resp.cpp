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

#include "pldm_fwupdate.h"

static std::string fwuStates[] = {"Idle", "Learn Components", "Ready Xfer", "Download", "Verify", "Apply", "Active"};

/**
 * @brief Process pldm firmware update response messages
 *
 * Handles and validates response messages for various pldm firmware update
 * commands. Parses response data and updates internal state based on the
 * command type and response content.
 *
 * @param cmd Firmware update command that generated this response
 * @param id pldm instance ID used for the original command
 *
 * @return uint8_t Status of response processing
 * @retval PLDM_SUCCESS Response processed successfully
 * @retval PLDM_ERROR Invalid response or processing failure
 *
 * @note Validates response instance ID matches the request
 * @note Updates firmware state information based on response data
 * @note Handles different response formats for each command type
 */
uint8_t pldm::fwUpdateResp(uint8_t cmd, uint8_t id)
{
	TRACING();
	uint8_t ret = PLDM_ERROR;

	switch (cmd) {
	case QUERY_DEVICE_IDENTIFIERS:
	case CANCEL_UPDATE_COMPONENT:
	case CANCEL_UPDATE:
	case GET_STATUS:
		ret = pldmFwUpdateRespPayload(cmd, id);
		break;

	case GET_FIRMWARE_PARAMETERS:
		ret = pldmFwGetParamPayload(cmd, id);
		break;

	case REQUEST_UPDATE:
		ret = pldmFwUpdateRespPayload(cmd, id);
		if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
			if (mI2cPldmRead->respPayload[BYTE_0] == ALREADY_IN_UPDATE_MODE) {
				DBG("FWU : ALREADY_IN_UPDATE_MODE, proceed with firmware update\n");
				return PLDM_SUCCESS;
			} else if (mI2cPldmRead->respPayload[BYTE_0] == UNABLE_TO_INITIATE_UPDATE) {
				ERR("FWU : FD is not able to enter update mode - 0x%02x\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else if (mI2cPldmRead->respPayload[BYTE_0] == RETRY_REQUEST_UPDATE) {
				ERR("FWU : FD is not able to enter update mode immediately - 0x%02x\n",
					mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else {
				ERR("FWU Unknown Err :: Error Code - 0x%02x!!!\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			}
		}
		break;

	case PASS_COMPONENT_TABLE:
	case UPDATE_COMPONENT:
		ret = pldmFwUpdateRespPayload(cmd, id);
		if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
			if (mI2cPldmRead->respPayload[BYTE_0] == NOT_IN_UPDATE_MODE) {
				ERR("FWU :FD not in update mode - 0x%02x\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else if (mI2cPldmRead->respPayload[BYTE_0] == INVALID_STATE_FOR_COMMAND) {
				ERR("FWU : FD is not in a state to expect this command - 0x%02x\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else if (mI2cPldmRead->respPayload[BYTE_0] == PACKAGE_DATA_ERROR) {
				ERR("FWU : FD received invalid packet data - 0x%02x\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else {
				ERR("FWU Unknown Err :: Error Code - 0x%02x!!!\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			}
		}
		break;

	case ACTIVATE_FIRMWARE:
		ret = pldmFwUpdateRespPayload(cmd, id);
		break;

	default:
		ERR("pldm Invalid Firmware Update Command\n");
		break;
	}

	return ret;
}

/**
 * @brief Process payload data from firmware update response messages
 *
 * Parses and validates the payload content of firmware update response messages.
 * Extracts relevant information such as completion codes, status, and progress
 * data from the response payload.
 *
 * @param cmd Firmware update command that generated this response
 * @param id pldm instance ID used for the original command
 *
 * @return uint8_t Status of payload processing
 * @retval PLDM_SUCCESS Payload processed successfully
 * @retval PLDM_ERROR Invalid payload or processing failure
 *
 * @note Sets up proper addressing for response message handling
 * @note Validates message integrity and completion codes
 * @note Extracts command-specific response data
 */
uint8_t pldm::pldmFwUpdateRespPayload(uint8_t cmd, UNUSED uint8_t id)
{
	TRACING();
	unsigned int totalSize = 0;
	uint8_t status = 0;

	mI2cPldmRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cPldmRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;

	DBG("pldm RX  :: ");
	hexdump((uint8_t *)mI2cPldmRead, totalSize);

	if (cmd == GET_STATUS) {
		// Current State
		status = mI2cPldmRead->respPayload[1];
		DBG("GET_STATUS CMD :  Currrent State --> %s\n", fwuStates[status].c_str());
		mFwuCurrentState = status;

		// Previous State
		status = mI2cPldmRead->respPayload[2];
		DBG("GET_STATUS CMD :  Previous State --> %s\n", fwuStates[status].c_str());
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Process GET_FIRMWARE_PARAMETERS response payload
 *
 * Handles the response payload for GET_FIRMWARE_PARAMETERS command, which
 * provides detailed information about the firmware device capabilities,
 * supported update operations, and component information.
 *
 * @param cmd Firmware update command (should be GET_FIRMWARE_PARAMETERS)
 * @param id pldm instance ID used for the original command
 *
 * @return uint8_t Status of parameter processing
 * @retval PLDM_SUCCESS Parameters processed successfully
 * @retval PLDM_ERROR Invalid response or processing failure
 *
 * @note Extracts firmware device capabilities and component information
 * @note Validates response message format and addressing
 * @note Updates internal firmware parameter state
 */
uint8_t pldm::pldmFwGetParamPayload(uint8_t cmd, uint8_t id)
{
	TRACING();
	unsigned int totalSize = 0;

	mI2cPldmRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cPldmRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;

	DBG("pldm RX  :: ");
	hexdump((uint8_t *)mI2cPldmRead, totalSize);

	// Check the pldm response error in the first chunk of multi response
	if (mI2cMultiResp != true) {
		if ((mI2cPldmRead->pldmHdr.cmdCode != cmd) || (mI2cPldmRead->pldmHdr.instanceID != id) ||
			(mI2cPldmRead->pldmHdr.request != PLDM_RESPONSE)) {
			ERR("pldm Firmware Update : Command Response Error !!!\n");
			ERR("cmd code : 0x%02x, id : 0x%02x, request / response : 0x%02x\n", mI2cPldmRead->pldmHdr.cmdCode,
				mI2cPldmRead->pldmHdr.instanceID, mI2cPldmRead->pldmHdr.request);
			return PLDM_ERROR;
		}
	}

	// som = 1 and eom = 1 Single response
	// som = 1 and eom = 0 Multi response
	// som = 0 and eom = 1 Last response
	// som = 0 and eom = 0 Multi response
	if ((mI2cPldmRead->mctpSmbusHdr.som == PLDM_SOM_BIT_ON) && (mI2cPldmRead->mctpSmbusHdr.eom == PLDM_EOM_BIT_ON)) {
		DBG("SOM and EOM are set to 1, single response from AMC... Proceed with next command\n");
		mI2cMultiResp = false;
	} else if (((mI2cPldmRead->mctpSmbusHdr.som == PLDM_SOM_BIT_ON) &&
				(mI2cPldmRead->mctpSmbusHdr.eom == PLDM_EOM_BIT_OFF)) ||
			   ((mI2cPldmRead->mctpSmbusHdr.som == PLDM_SOM_BIT_OFF) &&
				(mI2cPldmRead->mctpSmbusHdr.eom == PLDM_EOM_BIT_OFF))) {
		DBG("Multi response from AMC...\n");
		mI2cMultiResp = true;
	} else if (((mI2cPldmRead->mctpSmbusHdr.som == PLDM_SOM_BIT_OFF) &&
				(mI2cPldmRead->mctpSmbusHdr.eom == PLDM_EOM_BIT_ON))) {
		DBG("Last Chunk of response from AMC...Proceed with next command\n");
		mI2cMultiResp = false;
	}

	return PLDM_SUCCESS;
}
