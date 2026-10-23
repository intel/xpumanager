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

	// Every response starts with a completion code (DSP0240)
	if (mFwuRxPayload.empty()) {
		ERR("FWU : Response to cmd 0x{:02x} carries no completion code\n", cmd);
		return PLDM_ERROR;
	}

	// Record the device-supplied FWU completion code so the caller can distinguish recoverable
	// conditions (ALREADY_IN_UPDATE_MODE, NOT_IN_UPDATE_MODE, ...) from generic errors.
	mLastFwuCompletionCode = mI2cPldmRead->respPayload[BYTE_0];

	switch (cmd) {
	case QUERY_DEVICE_IDENTIFIERS:
	case GET_STATUS:
		ret = pldmFwUpdateRespPayload(cmd, id);
		break;

	case CANCEL_UPDATE_COMPONENT:
		ret = pldmFwUpdateRespPayload(cmd, id);
		if (ret == PLDM_SUCCESS && mFwuRxPayload[BYTE_0] != PLDM_SUCCESS) {
			DBG("FWU : CancelUpdateComponent returned 0x{:02x}\n", mFwuRxPayload[BYTE_0]);
			return PLDM_ERROR;
		}
		break;

	case CANCEL_UPDATE:
		ret = pldmFwUpdateRespPayload(cmd, id);
		if (ret != PLDM_SUCCESS) {
			break;
		}
		if (mFwuRxPayload[BYTE_0] != PLDM_SUCCESS) {
			DBG("FWU : CancelUpdate returned 0x{:02x}\n", mFwuRxPayload[BYTE_0]);
			return PLDM_ERROR;
		}
		// DSP0267 Table 42: CompletionCode, bool8 NonFunctioningComponentIndication,
		// bitfield64 NonFunctioningComponentBitmap
		if (mFwuRxPayload.size() < 1 + 1 + sizeof(uint64_t)) {
			ERR("FWU : CancelUpdate response too short ({} bytes); the AMC did not report which components "
				"remain functional\n",
				mFwuRxPayload.size());
		} else if (mFwuRxPayload[BYTE_1] != 0) {
			uint64_t bitmap = 0;
			for (size_t i = 0; i < sizeof(uint64_t); i++) {
				bitmap |= static_cast<uint64_t>(mFwuRxPayload[BYTE_2 + i]) << (8 * i);
			}
			// Bit n is the nth component passed in PassComponentTable
			ERR("AMC on card {:02} reports component(s) that will not be functional after the next activation "
				"(NonFunctioningComponentBitmap 0x{:016x})\n",
				mCardNum, bitmap);
		}
		break;

	case GET_FIRMWARE_PARAMETERS:
		ret = pldmFwUpdateRespPayload(cmd, id);
		if (ret == PLDM_SUCCESS) {
			// The response is longer than respPayload; parseFwParamResponse() walks the full payload.
			mFwParamRawData = mFwuRxPayload;
		}
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
		if (ret != PLDM_SUCCESS) {
			break;
		}
		if (cmd == UPDATE_COMPONENT) {
			ret = fwUpdUpdateComponentResp();
			break;
		}
		// DSP0267 Table 30: CompletionCode, ComponentResponse, ComponentResponseCode. A component that
		// "may be updateable" is only refused, with its reason, by the UpdateComponent response.
		if (mFwuRxPayload.size() < 3) {
			ERR("FWU : PassComponentTable response too short ({} bytes)\n", mFwuRxPayload.size());
			return PLDM_ERROR;
		}
		if (mFwuRxPayload[BYTE_1] != 0) {
			DBG("FWU : FD reports the component may not be updateable (ComponentResponseCode 0x{:02x})\n",
				mFwuRxPayload[BYTE_2]);
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
 * @note Validates the PLDM header against the request
 * @note Extracts command-specific response data
 */
uint8_t pldm::pldmFwUpdateRespPayload(uint8_t cmd, uint8_t id)
{
	TRACING();
	uint8_t status = 0;

	if (mI2cPldmRead->pldmHdr.cmdCode != cmd || mI2cPldmRead->pldmHdr.instanceID != id ||
		mI2cPldmRead->pldmHdr.request != PLDM_RESPONSE || mI2cPldmRead->pldmHdr.cmdType != PLDM_FIRMWARE_UPDATE) {
		ERR("FWU : PLDM header mismatch for cmd 0x{:02x} (got cmdType=0x{:02x} cmdCode=0x{:02x} "
			"instanceID=0x{:02x} request={})\n",
			cmd, mI2cPldmRead->pldmHdr.cmdType, mI2cPldmRead->pldmHdr.cmdCode, mI2cPldmRead->pldmHdr.instanceID,
			(uint32_t)mI2cPldmRead->pldmHdr.request);
		return PLDM_ERROR;
	}

	if (cmd == GET_STATUS) {
		// CompletionCode, CurrentState, PreviousState, ... (DSP0267 GetStatus response)
		if (mFwuRxPayload[BYTE_0] != PLDM_SUCCESS) {
			ERR("FWU : GET_STATUS failed - 0x{:02x}\n", mFwuRxPayload[BYTE_0]);
			return PLDM_ERROR;
		}
		if (mFwuRxPayload.size() < 3 || mFwuRxPayload[1] >= ARRAY_SIZE(fwuStates) ||
			mFwuRxPayload[2] >= ARRAY_SIZE(fwuStates)) {
			ERR("FWU : Malformed GET_STATUS response ({} bytes)\n", mFwuRxPayload.size());
			return PLDM_ERROR;
		}

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
 * @brief Describe a ComponentCompatibilityResponseCode (DSP0267 Table 31)
 *
 * @param code ComponentCompatibilityResponseCode from the UpdateComponent response
 *
 * @return const char* Short description
 */
static const char *componentCompatibilityReason(uint8_t code)
{
	switch (code) {
	case 0x01:
		return "the component comparison stamp is identical to the installed one and force update is not set";
	case 0x02:
		return "the component comparison stamp is lower than the installed one and force update is not set";
	case 0x03:
		return "invalid component comparison stamp or version";
	case 0x04:
		return "the component conflicts with another component of the package";
	case 0x05:
		return "pre-requisites for this component have not been met";
	case 0x06:
		return "the component is not supported by the device";
	case 0x07:
		return "security restrictions prevent the component from being downgraded";
	case 0x08:
		return "an incomplete component image set was passed";
	case 0x09:
		return "the component does not match the details passed in PassComponentTable";
	case 0x0A:
		return "the component version string is identical to the installed one and force update is not set";
	case 0x0B:
		return "the component version string is lower than the installed one and force update is not set";
	default:
		return (code >= 0xD0 && code <= 0xEF) ? "vendor defined reason" : "reserved reason code";
	}
}

/**
 * @brief Check the UpdateComponent response (DSP0267 Table 31)
 *
 * A ComponentCompatibilityResponse of 1 means the FD will not update the component and will not
 * request its image, so the update cannot go on with it. EstimatedTimeBeforeSendingRequestFirmwareData
 * is kept in mFwuEstTimeBeforeReqFwDataSec: the UA must not start its Table 2 timers before it elapses.
 *
 * @return uint8_t PLDM_SUCCESS when the FD will update the component, PLDM_ERROR otherwise
 */
uint8_t pldm::fwUpdUpdateComponentResp()
{
	TRACING();

	// CompletionCode, ComponentCompatibilityResponse, ComponentCompatibilityResponseCode,
	// bitfield32 UpdateOptionFlagsEnabled, uint16 EstimatedTimeBeforeSendingRequestFirmwareData
	constexpr size_t kMinRespLen = 1 + 1 + 1 + 4 + 2;
	if (mFwuRxPayload.size() < kMinRespLen) {
		ERR("FWU : UpdateComponent response too short ({} bytes, need {})\n", mFwuRxPayload.size(), kMinRespLen);
		return PLDM_ERROR;
	}

	const uint8_t compatibility = mFwuRxPayload[BYTE_1];
	const uint8_t reason = mFwuRxPayload[BYTE_2];
	if (compatibility != 0) {
		ERR("AMC on card {:02} will not update component 0x{:04x}: {} (ComponentCompatibilityResponseCode "
			"0x{:02x})\n",
			mCardNum, mUpdComp.id, componentCompatibilityReason(reason), reason);
		return PLDM_ERROR;
	}

	const uint32_t flagsEnabled =
		static_cast<uint32_t>(mFwuRxPayload[BYTE_3]) | (static_cast<uint32_t>(mFwuRxPayload[BYTE_4]) << 8) |
		(static_cast<uint32_t>(mFwuRxPayload[BYTE_5]) << 16) | (static_cast<uint32_t>(mFwuRxPayload[BYTE_6]) << 24);
	if ((mUpdComp.updateoptionflags & FWU_FORCEUPDATE) && !(flagsEnabled & FWU_FORCEUPDATE)) {
		DBG("FWU : Force update was requested but the FD did not enable it (UpdateOptionFlagsEnabled 0x{:08x})\n",
			flagsEnabled);
	}

	mFwuEstTimeBeforeReqFwDataSec =
		static_cast<uint16_t>(mFwuRxPayload[BYTE_7] | (static_cast<uint16_t>(mFwuRxPayload[BYTE_8]) << 8));
	DBG("FWU : EstimatedTimeBeforeSendingRequestFirmwareData = {} s\n", mFwuEstTimeBeforeReqFwDataSec);
	return PLDM_SUCCESS;
}
