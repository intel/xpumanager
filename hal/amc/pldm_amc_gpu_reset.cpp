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
 * Uses SetStateEffecterEnable command (0x38) with DISABLED operational state
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
	uint8_t cmdLen = MCTP_HEADER_SIZE + PLDM_HEADER_SIZE + sizeof(setStateEffecterEnableReq);
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;
	uint8_t *rptr = (uint8_t *)mI2cPldmRead;

	if (instanceID == PLDM_INSTANCE_ID_MAX) {
		instanceID = 1;
	}

	// Build SetStateEffecterEnable request payload
	struct setStateEffecterEnableReq payload;
	payload.effecter_id = PLDM_STATE_EFFECTER_AIC_RESET;
	payload.composite_effecter_count = 1;
	payload.state_field.effecter_operational_state = PLDM_EFFECTER_DISABLED;
	payload.state_field.event_message_enable = EVENT_MSG_NO_CHANGE;

	memcpy(mI2cPldmWrite->respPayload, &payload, sizeof(payload));

	// Construct MCTP header (transport layer)
	commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (cmdLen - 3),
						MCTP_INTEGRITY_CHECK);
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;

	// Construct PLDM header (application layer)
	pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, instanceID, PLDM_PLATFORM_MONITORING, PLDM_SET_STATE_EFFECTER_ENABLE,
						PLDM_ASYNC_REQUEST_NOTIFY, PLDM_REQUEST);

	DBG("AMC Reset TX :: ");
	hexdump((uint8_t *)mI2cPldmWrite, cmdLen);

	// Send command over I2C
	if (i2cobj->writeAmc(wptr + 1, cmdLen) != true) {
		ERR("AMC Reset: I2C Write failure\n");
		return PLDM_ERROR;
	}

	// Wait for AMC to process command
	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);

	// Read response
	if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) != true) {
		ERR("AMC Reset: I2C Read failure\n");
		return PLDM_ERROR;
	}

	// Calculate response size
	unsigned int totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;

	DBG("AMC Reset RX :: ");
	hexdump((uint8_t *)mI2cPldmRead, totalSize);

	// Validate PLDM response
	// Check that rq bit is cleared (0=response, 1=request)
	if (mI2cPldmRead->pldmHdr.request != 0) {
		ERR("AMC Reset: Invalid response - request bit is 1 (expected 0 for response)\n");
		return PLDM_ERROR;
	}

	// Verify instance ID matches
	if (mI2cPldmRead->pldmHdr.instanceID != instanceID) {
		ERR("AMC Reset: Instance ID mismatch - sent: %u, received: %u\n", instanceID, mI2cPldmRead->pldmHdr.instanceID);
		return PLDM_ERROR;
	}

	// Check completion code (first byte of response payload per PLDM spec)
	if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
		ERR("AMC Reset: Command failed with completion code: 0x%02x\n", mI2cPldmRead->respPayload[BYTE_0]);
		return PLDM_ERROR;
	}

	instanceID++;

	DBG("AMC Reset: Reset completed successfully\n");

	return PLDM_SUCCESS;
}