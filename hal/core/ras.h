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
#ifndef _RAS_H
#define _RAS_H

#include "sysman.h"

class LIBXPUM_API ras : public sysman
{
private:
	uint32_t rasCount;
	zes_ras_handle_t *rasHandles;

public:
	ras() : rasCount(0), rasHandles(nullptr) {}
	~ras();
	ze_result_t enumRasErrorSets(zes_device_handle_t device);
	ze_result_t getProperties(zes_ras_handle_t rasHandle, zes_ras_properties_t *properties);
	ze_result_t getConfig(zes_ras_handle_t rasHandle, zes_ras_config_t *config);
	ze_result_t getState(zes_ras_handle_t rasHandle, zes_ras_state_t *state);
	ze_result_t getErrors(zes_ras_error_cat_t type, zes_ras_error_type_t errorType, uint64_t *rasCounter);

	ze_result_t init(zes_device_handle_t device);
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif