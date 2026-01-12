/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ECC_H
#define _ECC_H

#include "sysman.h"

struct ecc_state_t
{
	ze_bool_t configurable;
	ze_bool_t available;
	zes_device_ecc_state_t currentState;
	zes_device_ecc_state_t pendingState;
	zes_device_action_t pendingAction;
};

class LIBXPUM_API ecc : public sysman
{
public:
	ecc() {}
	~ecc() {}
	bool available(zes_device_handle_t device);
	bool configurable(zes_device_handle_t device);
	std::string printEccState(const zes_device_ecc_state_t state);
	std::string printEccPendingAction(const zes_device_action_t action);

	ze_result_t getState(zes_device_handle_t device, zes_device_ecc_properties_t *eccState);

	ze_result_t setState(zes_device_handle_t device, bool enable, ecc_state_t *state = nullptr);
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif