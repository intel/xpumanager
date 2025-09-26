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