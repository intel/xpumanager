/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _RAS_H
#define _RAS_H

#include "sysman.h"
#include <map>

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
	ze_result_t getErrorsPerTile(zes_ras_error_cat_t type, zes_ras_error_type_t errorType,
								 std::map<uint32_t, uint64_t> &countersPerTile, uint64_t *totalCounter);

	ze_result_t init(zes_device_handle_t device);
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif