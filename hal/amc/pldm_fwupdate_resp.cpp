/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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

	// Record the device-supplied FWU completion code so the caller can distinguish recoverable
	// conditions (ALREADY_IN_UPDATE_MODE, NOT_IN_UPDATE_MODE, ...) from generic errors.
	mLastFwuCompletionCode = mI2cPldmRead->respPayload[BYTE_0];

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
				// Do NOT silently proceed: a stale update session on the device
				// can carry parameters that conflict with the current package.
				// Signal the caller (fwUpdInitialize) to CancelUpdate and retry
				// RequestUpdate once, matching amctool's Go implementation.
				DBG("FWU : ALREADY_IN_UPDATE_MODE - caller should cancel and retry\n");
				return PLDM_ERROR;
			} else if (mI2cPldmRead->respPayload[BYTE_0] == UNABLE_TO_INITIATE_UPDATE) {
				ERR("FWU : FD is not able to enter update mode - 0x{:02x}\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else if (mI2cPldmRead->respPayload[BYTE_0] == RETRY_REQUEST_UPDATE) {
				ERR("FWU : FD is not able to enter update mode immediately - 0x{:02x}\n",
					mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else {
				ERR("FWU Unknown Err :: Error Code - 0x{:02x}!!!\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			}
		}
		break;

	case PASS_COMPONENT_TABLE:
	case UPDATE_COMPONENT:
		ret = pldmFwUpdateRespPayload(cmd, id);
		if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
			if (mI2cPldmRead->respPayload[BYTE_0] == NOT_IN_UPDATE_MODE) {
				ERR("FWU :FD not in update mode - 0x{:02x}\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else if (mI2cPldmRead->respPayload[BYTE_0] == INVALID_STATE_FOR_COMMAND) {
				ERR("FWU : FD is not in a state to expect this command - 0x{:02x}\n",
					mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else if (mI2cPldmRead->respPayload[BYTE_0] == PACKAGE_DATA_ERROR) {
				ERR("FWU : FD received invalid packet data - 0x{:02x}\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			} else {
				ERR("FWU Unknown Err :: Error Code - 0x{:02x}!!!\n", mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_ERROR;
			}
		}
		break;

	case ACTIVATE_FIRMWARE:
		ret = pldmFwUpdateRespPayload(cmd, id);
		if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
			// The device may have self-activated after ApplyComplete and left
			// update mode before we sent ActivateFirmware. In that case it
			// reports NOT_IN_UPDATE_MODE (0x80) or INVALID_STATE_FOR_COMMAND
			// (0x84) - both mean "activation already happened", not a failure.
			if (mI2cPldmRead->respPayload[BYTE_0] == NOT_IN_UPDATE_MODE ||
				mI2cPldmRead->respPayload[BYTE_0] == INVALID_STATE_FOR_COMMAND) {
				DBG("FWU : Device already left update mode after ApplyComplete (0x{:02x}) - "
					"treating ActivateFirmware as successful\n",
					mI2cPldmRead->respPayload[BYTE_0]);
				return PLDM_SUCCESS;
			}
			ERR("FWU : ActivateFirmware failed - 0x{:02x}\n", mI2cPldmRead->respPayload[BYTE_0]);
			return PLDM_ERROR;
		}
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

	if (mI2cPldmRead->pldmHdr.cmdCode != cmd || mI2cPldmRead->pldmHdr.instanceID != id ||
		mI2cPldmRead->pldmHdr.request != PLDM_RESPONSE || mI2cPldmRead->pldmHdr.cmdType != PLDM_FIRMWARE_UPDATE) {
		ERR("FWU : PLDM header mismatch for cmd 0x{:02x} (got cmdType=0x{:02x} cmdCode=0x{:02x} "
			"instanceID=0x{:02x} request={})\n",
			cmd, mI2cPldmRead->pldmHdr.cmdType, mI2cPldmRead->pldmHdr.cmdCode, mI2cPldmRead->pldmHdr.instanceID,
			(uint32_t)mI2cPldmRead->pldmHdr.request);
		return PLDM_ERROR;
	}

	if (cmd == GET_STATUS) {
		// Current State
		status = mI2cPldmRead->respPayload[1];
		DBG("GET_STATUS CMD :  Current State --> {}\n", fwuStates[status].c_str());
		mFwuCurrentState = status;

		// Previous State
		status = mI2cPldmRead->respPayload[2];
		DBG("GET_STATUS CMD :  Previous State --> {}\n", fwuStates[status].c_str());
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
			ERR("cmd code : 0x{:02x}, id : 0x{:02x}, request / response : 0x{:02x}\n", mI2cPldmRead->pldmHdr.cmdCode,
				mI2cPldmRead->pldmHdr.instanceID, mI2cPldmRead->pldmHdr.request);
			return PLDM_ERROR;
		}
	}

	// som = 1 and eom = 1 Single response
	// som = 1 and eom = 0 Multi response
	// som = 0 and eom = 1 Last response
	// som = 0 and eom = 0 Multi response
	if ((mI2cPldmRead->mctpSmbusHdr.som == PLDM_SOM_BIT_ON) && (mI2cPldmRead->mctpSmbusHdr.eom == PLDM_EOM_BIT_ON)) {
		DBG("som and EOM are set to 1, single response from AMC... Proceed with next command\n");
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
