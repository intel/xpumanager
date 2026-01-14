/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _PERFORMANCE_H
#define _PERFORMANCE_H

#include "sysman.h"
#include <vector>

class LIBXPUM_API performance : public sysman
{
private:
	std::vector<zes_perf_handle_t> *perfHandles;

public:
	performance() { perfHandles = new std::vector<zes_perf_handle_t>; }
	~performance() { delete perfHandles; }
	ze_result_t enumPerformanceFactorDomains(zes_device_handle_t device);
	ze_result_t getProperties(zes_perf_handle_t perfHandle, zes_perf_properties_t *properties);
	ze_result_t getConfig(zes_perf_handle_t perfHandle);
	ze_result_t setConfig(zes_engine_type_flag_t engineType, double factor);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif