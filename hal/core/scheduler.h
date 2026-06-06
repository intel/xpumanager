/*
 * Copyright (C) 2025-2026 Intel Corporation
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
	ze_result_t getProperties(zes_sched_handle_t schedulerHandle, zes_sched_properties_t *props);
	ze_result_t getCurrentMode(zes_sched_handle_t schedulerHandle);
	ze_result_t getCurrentMode(zes_sched_handle_t schedulerHandle, zes_sched_mode_t *mode);
	ze_result_t getTimeoutModeProperties(zes_sched_handle_t schedulerHandle);
	ze_result_t getTimeoutModeProperties(zes_sched_handle_t schedulerHandle, bool getDefaults,
										 zes_sched_timeout_properties_t *props);
	ze_result_t getTimesliceProperties(zes_sched_handle_t schedulerHandle);
	ze_result_t getTimesliceProperties(zes_sched_handle_t schedulerHandle, bool getDefaults,
									   zes_sched_timeslice_properties_t *props);

	uint32_t getSchedulerCount() const { return schedulerCount; }
	zes_sched_handle_t *getSchedulerHandles() const { return schedulerHandles; }

	ze_result_t setTimeoutMode(uint64_t timeoutValue);
	ze_result_t setTimesliceMode(uint64_t timesliceValue, uint64_t yieldTimeoutValue);
	ze_result_t setExclusiveMode();

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif