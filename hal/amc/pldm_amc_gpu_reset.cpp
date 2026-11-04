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

	// Build SetStateEffecterStates request payload
	struct setStateEffecterStatesReq payload;
	payload.effecter_id = PLDM_STATE_EFFECTER_AIC_RESET;
	payload.composite_effecter_count = 1;
	payload.state_field.set_request = SET_REQUEST_SET;
	payload.state_field.effecter_state = EFFECTER_STATE_REQUEST_RESTART;

	memcpy(mI2cPldmWrite->respPayload, &payload, sizeof(payload));

	// Not resent: if the AMC acted on the first try and only its response was lost or damaged, a resend
	// could restart the GPU a second time. A missing or damaged response is reported instead.
	constexpr MctpRetryTiming noResend{std::chrono::milliseconds(PLDM_PT2_MS), 0, 0,
									   std::chrono::milliseconds(PLDM_PT2_MS)};
	uint8_t id = 0;
	if (pldmRequest(PLDM_PLATFORM_MONITORING, PLDM_SET_STATE_EFFECTER_STATES, sizeof(payload), id, noResend) !=
		PLDM_SUCCESS) {
		ERR("AMC Reset: No response to SetStateEffecterStates\n");
		return PLDM_ERROR;
	}

	if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
		ERR("AMC Reset: Command failed with completion code: 0x{:02x}\n", mI2cPldmRead->respPayload[BYTE_0]);
		return PLDM_ERROR;
	}

	DBG("AMC Reset: Reset completed successfully\n");
	return PLDM_SUCCESS;
}
