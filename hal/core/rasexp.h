/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef RASEXP_H
#define RASEXP_H

#include <os.h>
#include "zes_api.h"
#include "debug.h"
#include <vector>
#include <map>

struct ras_state_exp_t
{
	zes_ras_error_category_exp_t category;
	uint64_t errorCount;
};

class LIBXPUM_API rasExp
{
private:
	zes_driver_handle_t zesDriver;
	zes_device_handle_t zesDevice;
	zes_ras_handle_t *rasHandles = nullptr;
	uint32_t rasCount = 0;
	bool rasExpEnabled;

public:
	rasExp() : zesDriver(nullptr), zesDevice(0), rasHandles(nullptr), rasCount(0), rasExpEnabled(false){};
	~rasExp()
	{
		delete[] rasHandles;
		rasHandles = nullptr;
	}
	ze_result_t init(zes_driver_handle_t driver, zes_device_handle_t device);
	ze_result_t getErrorsPerTileRasExp(std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> &rasErrStates);
	bool isRasExpEnabled() const { return rasExpEnabled; }
};
#endif // RASEXP_H