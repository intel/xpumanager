/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __PLDM_AMC_GPU_RESET_H
#define __PLDM_AMC_GPU_RESET_H

#include "pldm.h"
#include "mctp_constants.h"
#include "pldm_constants.h"
#include "pldm_platform.h"

// PLDM Platform Monitoring and Control Command Code for AMC GPU Reset (DSP0248)
#define PLDM_SET_STATE_EFFECTER_STATES 0x39

// AMC-specific Effecter ID for GPU reset
#define PLDM_STATE_EFFECTER_AIC_RESET 402

enum setRequest
{
	SET_REQUEST_NO_CHANGE = 0x00,
	SET_REQUEST_SET = 0x01
};

enum effecterState
{
	EFFECTER_STATE_REQUEST_NORMAL = 0x01,
	EFFECTER_STATE_REQUEST_RESTART = 0x02
};

#pragma pack(push, 1)
struct stateEffecterStateField
{
	uint8_t set_request;
	uint8_t effecter_state;
};

// SetStateEffecterStates request payload fields as per spec (DSP0248)
struct setStateEffecterStatesReq
{
	uint16_t effecter_id;
	uint8_t composite_effecter_count;
	stateEffecterStateField state_field;
};
#pragma pack(pop)

#endif // __PLDM_AMC_GPU_RESET_H
