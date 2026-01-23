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
#define PLDM_SET_STATE_EFFECTER_ENABLE 0x38

// Event Message Enable
enum eventMsgEnable
{
	EVENT_MSG_ENABLE_EVENTS = 0x00,
	EVENT_MSG_DISABLE_EVENTS = 0x01,
	EVENT_MSG_NO_CHANGE = 0xFF
};

// AMC-specific Effecter ID for GPU reset
#define PLDM_STATE_EFFECTER_AIC_RESET 402

#pragma pack(push, 1)
struct stateEffecterOpField
{
	uint8_t effecter_operational_state;
	uint8_t event_message_enable;
};

// SetStateEffecterEnable request payload fields as per spec (DSP0248)
struct setStateEffecterEnableReq
{
	uint16_t effecter_id;
	uint8_t composite_effecter_count;
	stateEffecterOpField state_field;
};
#pragma pack(pop)

#endif // __PLDM_AMC_GPU_RESET_H