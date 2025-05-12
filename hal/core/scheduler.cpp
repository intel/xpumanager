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
#include "scheduler.h"

scheduler::~scheduler()
{
	if (schedulerHandles)
	{
		delete[] schedulerHandles;
		schedulerHandles = nullptr;
	}
}

ze_result_t scheduler::enumSchedulers(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumSchedulers(device, &schedulerCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || schedulerCount == 0)
	{
		ERR("Failed to enumerate schedulers. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	schedulerHandles = new zes_sched_handle_t[schedulerCount];
	result = zesDeviceEnumSchedulers(device, &schedulerCount, schedulerHandles);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get schedulers. 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found %d schedulers.\n", schedulerCount);
	return result;
}

ze_result_t scheduler::getProperties(zes_sched_handle_t schedulerHandle)
{
	zes_sched_properties_t properties = {};
	ze_result_t result = zesSchedulerGetProperties(schedulerHandle, &properties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get properties for scheduler 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Scheduler Properties:\n");
	DBG("  onSubdevice: %d\n", properties.onSubdevice);
	DBG("  subdeviceId: %d\n", properties.subdeviceId);
	DBG("  canControl: %d\n", properties.canControl);
	printEngines(properties.engines);
	DBG("  supportedModes: %d\n", properties.supportedModes);

	return result;
}

ze_result_t scheduler::getCurrentMode(zes_sched_handle_t schedulerHandle)
{
	zes_sched_mode_t mode = {};
	ze_result_t result = zesSchedulerGetCurrentMode(schedulerHandle, &mode);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get current mode for scheduler 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Current Scheduler Mode: %d\n", mode);

	return result;
}

ze_result_t scheduler::getTimeoutModeProperties(zes_sched_handle_t schedulerHandle)
{
	zes_sched_timeout_properties_t timeoutProperties = {};
	ze_result_t result = zesSchedulerGetTimeoutModeProperties(schedulerHandle, 0, &timeoutProperties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get timeout mode properties for scheduler 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Timeout Mode Properties:\n");
	DBG("  Timeout: %" PRIu64 "us\n", timeoutProperties.watchdogTimeout);

	return result;
}

ze_result_t scheduler::getTimesliceProperties(zes_sched_handle_t schedulerHandle)
{
	zes_sched_timeslice_properties_t timesliceProperties = {};
	ze_result_t result = zesSchedulerGetTimesliceModeProperties(schedulerHandle, 0, &timesliceProperties);
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to get timeslice properties for scheduler 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Timeslice Properties:\n");
	DBG("  Timeslice interval: %" PRIu64 "us\n", timesliceProperties.interval);
	DBG("  Timeslice yield timeout: %" PRIu64 "us\n", timesliceProperties.yieldTimeout);

	return result;
}

ze_result_t scheduler::setTimeoutMode(float timeoutValue)
{
	zes_sched_timeout_properties_t timeoutProperties = {};
	timeoutProperties.watchdogTimeout = static_cast<uint64_t>(timeoutValue);
	ze_bool_t pNeedReload;
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < schedulerCount; ++i)
	{
		result = zesSchedulerSetTimeoutMode(schedulerHandles[i], &timeoutProperties, &pNeedReload);
		if (result != ZE_RESULT_SUCCESS)
		{
			ERR("Failed to set scheduler mode. 0x%X (%s)\n", result, l0_error_to_string(result));
			return result;
		}

		if (pNeedReload)
		{
			INFO("Scheduler mode set successfully. Device driver reload is needed to take effect.\n");
		}
		else
		{
			INFO("Scheduler mode set successfully. No device driver reload needed to take effect.\n");
		}
	}

	return result;
}

ze_result_t scheduler::setTimesliceMode(float timesliceValue, float yieldTimeoutValue)
{
	zes_sched_timeslice_properties_t timesliceProperties = {};
	timesliceProperties.interval = static_cast<uint64_t>(timesliceValue);
	timesliceProperties.yieldTimeout = static_cast<uint64_t>(yieldTimeoutValue);
	ze_bool_t pNeedReload;
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < schedulerCount; ++i)
	{
		result = zesSchedulerSetTimesliceMode(schedulerHandles[i], &timesliceProperties, &pNeedReload);
		if (result != ZE_RESULT_SUCCESS)
		{
			ERR("Failed to set scheduler mode. 0x%X (%s)\n", result, l0_error_to_string(result));
			return result;
		}

		if (pNeedReload)
		{
			INFO("Scheduler mode set successfully. Device driver reload is needed to take effect.\n");
		}
		else
		{
			INFO("Scheduler mode set successfully. No device driver reload needed to take effect.\n");
		}
	}

	return result;
}

ze_result_t scheduler::setExclusiveMode()
{
	ze_bool_t pNeedReload;
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < schedulerCount; ++i)
	{
		result = zesSchedulerSetExclusiveMode(schedulerHandles[i], &pNeedReload);
		if (result != ZE_RESULT_SUCCESS)
		{
			ERR("Failed to set scheduler mode. 0x%X (%s)\n", result, l0_error_to_string(result));
			return result;
		}

		if (pNeedReload)
		{
			INFO("Scheduler mode set successfully. Device driver reload is needed to take effect.\n");
		}
		else
		{
			INFO("Scheduler mode set successfully. No device driver reload needed to take effect.\n");
		}
	}

	return result;
}

ze_result_t scheduler::init(zes_device_handle_t device)
{
	return enumSchedulers(device);
}

ze_result_t scheduler::zesRun(zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;
	UNUSED(device);

	for (uint32_t i = 0; i < schedulerCount; ++i)
	{
		result = getProperties(schedulerHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = getCurrentMode(schedulerHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = getTimeoutModeProperties(schedulerHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}

		result = getTimesliceProperties(schedulerHandles[i]);
		if (result != ZE_RESULT_SUCCESS)
		{
			return result;
		}
	}

	return result;
}