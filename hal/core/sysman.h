/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYSMAN_H
#define _SYSMAN_H

#include "debug.h"
#include <cinttypes>
#include <os.h>
#include <zes_api.h>

LIBXPUM_API const char *l0_error_to_string(ze_result_t ret_val);

class LIBXPUM_API sysman
{
public:
	sysman() {}
	virtual ~sysman() {}
	virtual ze_result_t init(UNUSED zes_device_handle_t device) { return ZE_RESULT_SUCCESS; }
	virtual ze_result_t zesRun(UNUSED zes_device_handle_t device) { return ZE_RESULT_SUCCESS; }
	virtual ze_result_t zeRun(UNUSED ze_device_handle_t device, UNUSED void *args) { return ZE_RESULT_SUCCESS; }
	void printEngines(zes_engine_type_flags_t engines);
};

#endif