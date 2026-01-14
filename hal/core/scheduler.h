/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "sysman.h"

class LIBXPUM_API scheduler : public sysman
{
private:
	uint32_t schedulerCount;
	zes_sched_handle_t *schedulerHandles;

public:
	scheduler() : schedulerCount(0), schedulerHandles(nullptr) {}
	~scheduler();
	ze_result_t enumSchedulers(zes_device_handle_t device);
	ze_result_t getProperties(zes_sched_handle_t schedulerHandle);
	ze_result_t getCurrentMode(zes_sched_handle_t schedulerHandle);
	ze_result_t getTimeoutModeProperties(zes_sched_handle_t schedulerHandle);
	ze_result_t getTimesliceProperties(zes_sched_handle_t schedulerHandle);

	ze_result_t setTimeoutMode(float timeoutValue);
	ze_result_t setTimesliceMode(float timesliceValue, float yieldTimeoutValue);
	ze_result_t setExclusiveMode();

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif