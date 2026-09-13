/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm_amc_gpu_reset.h"
#include "pldm.h"

/**
 * @brief Execute AMC GPU reset via PLDM Platform Monitoring & Control
 *
 * Uses SetStateEffecterStates command (0x39)
 * on effecter ID 402 (AIC_RESET).
 *
 * Based on PLDM Platform Monitoring & Control Specification (DSP0248)
 *
 * @return uint8_t Status of reset command
 * @retval PLDM_SUCCESS Reset command sent successfully
 * @retval PLDM_ERROR Reset command failed
 *
 */
uint8_t pldm::amcGpuReset()
{
	TRACING();

	// Calculate command length: MCTP header + PLDM header + payload size
	uint8_t cmdLen = MCTP_HEADER_SIZE + PLDM_HEADER_SIZE + sizeof(setStateEffecterStatesReq);
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;
	uint8_t *rptr = (uint8_t *)mI2cPldmRead;

	if (instanceID == PLDM_INSTANCE_ID_MAX) {
		instanceID = 1;
	}

	// Build SetStateEffecterStates request payload
	struct setStateEffecterStatesReq payload;
	payload.effecter_id = PLDM_STATE_EFFECTER_AIC_RESET;
	payload.composite_effecter_count = 1;
	payload.state_field.set_request = SET_REQUEST_SET;
	payload.state_field.effecter_state = EFFECTER_STATE_REQUEST_RESTART;

	memcpy(mI2cPldmWrite->respPayload, &payload, sizeof(payload));

	// Construct MCTP header (transport layer)
	commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (cmdLen - 3),
						MCTP_INTEGRITY_CHECK);
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;

	// Construct PLDM header (application layer)
	pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, instanceID, PLDM_PLATFORM_MONITORING, PLDM_SET_STATE_EFFECTER_STATES,
						PLDM_ASYNC_REQUEST_NOTIFY, PLDM_REQUEST);

	DBG("AMC Reset TX :: ");
	hexdump((uint8_t *)mI2cPldmWrite, cmdLen);

	// Send command over I2C
	if (i2cobj->writeAmc(wptr + 1, cmdLen) != true) {
		ERR("AMC Reset: I2C Write failure\n");
		return PLDM_ERROR;
	}

	constexpr uint8_t amcResetResponseRetries = 3;
	constexpr uint8_t amcResetProcessingWaitMs = 30;
	for (uint8_t retry = 0; retry <= amcResetResponseRetries; retry++) {
		// GPU reset processing on AMC can take longer than a regular command.
		MSLEEP(amcResetProcessingWaitMs);

		if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) == true) {
			unsigned int totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;

			DBG("AMC Reset RX :: ");
			hexdump((uint8_t *)mI2cPldmRead, totalSize);

			if (mI2cPldmRead->mctpSmbusHdr.cmdCode != MCTP_CMD_CODE ||
				mI2cPldmRead->mctpSmbusHdr.msgType != PLDM_OVER_MCTP ||
				mI2cPldmRead->pldmHdr.cmdType != PLDM_PLATFORM_MONITORING ||
				mI2cPldmRead->pldmHdr.cmdCode != PLDM_SET_STATE_EFFECTER_STATES) {
				ERR("AMC Reset: Unexpected response - MCTP command: 0x{:02x}, message type: 0x{:02x}, "
					"PLDM type: 0x{:02x}, command: 0x{:02x}\n",
					mI2cPldmRead->mctpSmbusHdr.cmdCode, mI2cPldmRead->mctpSmbusHdr.msgType,
					mI2cPldmRead->pldmHdr.cmdType, mI2cPldmRead->pldmHdr.cmdCode);
			} else if (mI2cPldmRead->pldmHdr.request != PLDM_RESPONSE) {
				ERR("AMC Reset: Invalid response - request bit is 1 (expected 0 for response)\n");
			} else if (mI2cPldmRead->pldmHdr.instanceID != instanceID) {
				ERR("AMC Reset: Instance ID mismatch - sent: {}, received: {}\n", instanceID,
					mI2cPldmRead->pldmHdr.instanceID);
			} else if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
				ERR("AMC Reset: Command failed with completion code: 0x{:02x}\n", mI2cPldmRead->respPayload[BYTE_0]);
			} else {
				instanceID++;
				DBG("AMC Reset: Reset completed successfully\n");
				return PLDM_SUCCESS;
			}
		} else {
			ERR("AMC Reset: I2C Read failure\n");
		}

		if (retry < amcResetResponseRetries) {
			DBG("AMC Reset: Retry response ({}/{})\n", retry + 1, amcResetResponseRetries);
		}
	}

	ERR("AMC Reset: Invalid response after {} retries\n", amcResetResponseRetries);
	return PLDM_ERROR;
}
