/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "scheduler.h"

/**
 * @brief Destructor for the scheduler class
 *
 * This destructor performs cleanup operations for the scheduler management
 * object, releasing allocated memory for scheduler handles and ensuring
 * proper resource deallocation when the scheduler object is destroyed.
 */
scheduler::~scheduler()
{
	if (schedulerHandles) {
		delete[] schedulerHandles;
		schedulerHandles = nullptr;
	}
}

/**
 * @brief Enumerates available scheduler controllers for a device
 *
 * This function discovers and catalogs all scheduler controllers available on the
 * specified device. Scheduler controllers manage GPU compute engine scheduling
 * and provide workload allocation and priority management capabilities.
 *
 * @param device Handle to the Level Zero Sysman device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful enumeration, error code otherwise
 */
ze_result_t scheduler::enumSchedulers(zes_device_handle_t device)
{
	ze_result_t result = zesDeviceEnumSchedulers(device, &schedulerCount, nullptr);
	if (result != ZE_RESULT_SUCCESS || schedulerCount == 0) {
		ERR("Failed to enumerate schedulers. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	schedulerHandles = new zes_sched_handle_t[schedulerCount];
	result = zesDeviceEnumSchedulers(device, &schedulerCount, schedulerHandles);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get schedulers. 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Found {} schedulers.\n", schedulerCount);
	return result;
}

/**
 * @brief Gets properties for a specific scheduler controller
 *
 * This function retrieves detailed properties and characteristics for a
 * specific scheduler controller, including supported engines, control capabilities,
 * scheduling modes, and subdevice association for workload management.
 *
 * @param schedulerHandle Handle to the specific scheduler controller
 * @return ze_result_t ZE_RESULT_SUCCESS on successful property retrieval, error code otherwise
 */
ze_result_t scheduler::getProperties(zes_sched_handle_t schedulerHandle)
{
	zes_sched_properties_t properties = {};
	ze_result_t result = zesSchedulerGetProperties(schedulerHandle, &properties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get properties for scheduler 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Scheduler Properties:\n");
	DBG("  onSubdevice: {}\n", properties.onSubdevice);
	DBG("  subdeviceId: {}\n", properties.subdeviceId);
	DBG("  canControl: {}\n", properties.canControl);
	printEngines(properties.engines);
	DBG("  supportedModes: {}\n", properties.supportedModes);

	return result;
}

/**
 * @brief Gets the current scheduling mode for a scheduler controller
 *
 * This function retrieves the current active scheduling mode for a specific
 * scheduler controller, indicating whether it's operating in timeout, timeslice,
 * exclusive, or other scheduling modes for workload management analysis.
 *
 * @param schedulerHandle Handle to the specific scheduler controller
 * @return ze_result_t ZE_RESULT_SUCCESS on successful mode retrieval, error code otherwise
 */
ze_result_t scheduler::getCurrentMode(zes_sched_handle_t schedulerHandle)
{
	zes_sched_mode_t mode = {};
	ze_result_t result = zesSchedulerGetCurrentMode(schedulerHandle, &mode);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get current mode for scheduler 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Current Scheduler Mode: {}\n", mode);

	return result;
}

/**
 * @brief Gets timeout mode properties for a scheduler controller
 *
 * This function retrieves timeout mode configuration properties for a specific
 * scheduler controller, including watchdog timeout values used for compute
 * workload monitoring and hang detection mechanisms.
 *
 * @param schedulerHandle Handle to the specific scheduler controller
 * @return ze_result_t ZE_RESULT_SUCCESS on successful timeout properties retrieval, error code otherwise
 */
ze_result_t scheduler::getTimeoutModeProperties(zes_sched_handle_t schedulerHandle)
{
	zes_sched_timeout_properties_t timeoutProperties = {};
	ze_result_t result = zesSchedulerGetTimeoutModeProperties(schedulerHandle, 0, &timeoutProperties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get timeout mode properties for scheduler 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Timeout Mode Properties:\n");
	DBG("  Timeout: {}us\n", timeoutProperties.watchdogTimeout);

	return result;
}

/**
 * @brief Gets timeslice mode properties for a scheduler controller
 *
 * This function retrieves timeslice mode configuration properties for a specific
 * scheduler controller, including scheduling interval and yield timeout values
 * used for time-sharing workload management and GPU resource allocation.
 *
 * @param schedulerHandle Handle to the specific scheduler controller
 * @return ze_result_t ZE_RESULT_SUCCESS on successful timeslice properties retrieval, error code otherwise
 */
ze_result_t scheduler::getTimesliceProperties(zes_sched_handle_t schedulerHandle)
{
	zes_sched_timeslice_properties_t timesliceProperties = {};
	ze_result_t result = zesSchedulerGetTimesliceModeProperties(schedulerHandle, 0, &timesliceProperties);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get timeslice properties for scheduler 0x{:X} ({})\n", result, l0_error_to_string(result));
		return result;
	}

	DBG("Timeslice Properties:\n");
	DBG("  Timeslice interval: {}us\n", timesliceProperties.interval);
	DBG("  Timeslice yield timeout: {}us\n", timesliceProperties.yieldTimeout);

	return result;
}

/**
 * @brief Gets properties for a specific scheduler controller
 *
 * This function retrieves the properties of a specific scheduler controller
 * identified by its handle, returning information such as the subdevice
 * association and scheduling engine type.
 *
 * @param schedulerHandle Handle to the specific scheduler controller
 * @param props Pointer to the scheduler properties structure to populate
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t scheduler::getProperties(zes_sched_handle_t schedulerHandle, zes_sched_properties_t *props)
{
	ze_result_t result = zesSchedulerGetProperties(schedulerHandle, props);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get properties for scheduler 0x{:X} ({})\n", result, l0_error_to_string(result));
	}
	return result;
}

/**
 * @brief Gets the current scheduling mode for a specific scheduler controller
 *
 * This function retrieves the current scheduling mode (timeout, timeslice,
 * or exclusive) for a specific scheduler controller identified by its handle.
 *
 * @param schedulerHandle Handle to the specific scheduler controller
 * @param mode Pointer to the scheduling mode value to populate
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t scheduler::getCurrentMode(zes_sched_handle_t schedulerHandle, zes_sched_mode_t *mode)
{
	ze_result_t result = zesSchedulerGetCurrentMode(schedulerHandle, mode);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get current mode for scheduler 0x{:X} ({})\n", result, l0_error_to_string(result));
	}
	return result;
}
/**
 * @brief Gets timeout mode properties for a specific scheduler controller
 *
 * This function retrieves timeout mode configuration properties for a specific
 * scheduler controller, including the watchdog timeout value used for hang
 * detection and workload monitoring.
 *
 * @param schedulerHandle Handle to the specific scheduler controller
 * @param getDefaults If true, retrieves default values instead of current settings
 * @param props Pointer to the timeout properties structure to populate
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t scheduler::getTimeoutModeProperties(zes_sched_handle_t schedulerHandle, bool getDefaults,
												zes_sched_timeout_properties_t *props)
{
	ze_result_t result = zesSchedulerGetTimeoutModeProperties(schedulerHandle, getDefaults ? 1 : 0, props);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get timeout mode properties for scheduler 0x{:X} ({})\n", result, l0_error_to_string(result));
	}
	return result;
}

/**
 * @brief Gets timeslice mode properties for a specific scheduler controller
 *
 * This function retrieves timeslice mode configuration properties for a specific
 * scheduler controller, including the scheduling interval and yield timeout values
 * used for time-sharing workload management.
 *
 * @param schedulerHandle Handle to the specific scheduler controller
 * @param getDefaults If true, retrieves default values instead of current settings
 * @param props Pointer to the timeslice properties structure to populate
 * @return ze_result_t ZE_RESULT_SUCCESS on successful retrieval, error code otherwise
 */
ze_result_t scheduler::getTimesliceProperties(zes_sched_handle_t schedulerHandle, bool getDefaults,
											  zes_sched_timeslice_properties_t *props)
{
	ze_result_t result = zesSchedulerGetTimesliceModeProperties(schedulerHandle, getDefaults ? 1 : 0, props);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get timeslice properties for scheduler 0x{:X} ({})\n", result, l0_error_to_string(result));
	}
	return result;
}

/**
 * @brief Sets timeout mode for all scheduler controllers
 *
 * This function configures timeout mode across all scheduler controllers with
 * the specified watchdog timeout value, enabling compute workload monitoring
 * and automatic hang detection for system stability and reliability.
 *
 * @param timeoutValue Timeout value in microseconds for watchdog monitoring
 * @return ze_result_t ZE_RESULT_SUCCESS if timeout mode set successfully, error code otherwise
 */
ze_result_t scheduler::setTimeoutMode(uint64_t timeoutValue)
{
	zes_sched_timeout_properties_t timeoutProperties = {};
	timeoutProperties.stype = ZES_STRUCTURE_TYPE_SCHED_TIMEOUT_PROPERTIES;
	timeoutProperties.pNext = nullptr;
	timeoutProperties.watchdogTimeout = timeoutValue;
	ze_bool_t pNeedReload;
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < schedulerCount; ++i) {
		result = zesSchedulerSetTimeoutMode(schedulerHandles[i], &timeoutProperties, &pNeedReload);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set scheduler mode. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}

		if (pNeedReload) {
			INFO("Scheduler mode set successfully. Device driver reload is needed to take effect.\n");
		} else {
			INFO("Scheduler mode set successfully. No device driver reload needed to take effect.\n");
		}
	}

	return result;
}

/**
 * @brief Sets timeslice mode for all scheduler controllers
 *
 * This function configures timeslice mode across all scheduler controllers with
 * specified interval and yield timeout values, enabling time-sharing workload
 * management for fair GPU resource allocation among multiple processes.
 *
 * @param timesliceValue Timeslice interval in microseconds for scheduling quantum
 * @param yieldTimeoutValue Yield timeout in microseconds for preemption control
 * @return ze_result_t ZE_RESULT_SUCCESS if timeslice mode set successfully, error code otherwise
 */
ze_result_t scheduler::setTimesliceMode(uint64_t timesliceValue, uint64_t yieldTimeoutValue)
{
	zes_sched_timeslice_properties_t timesliceProperties = {};
	timesliceProperties.stype = ZES_STRUCTURE_TYPE_SCHED_TIMESLICE_PROPERTIES;
	timesliceProperties.pNext = nullptr;
	timesliceProperties.interval = timesliceValue;
	timesliceProperties.yieldTimeout = yieldTimeoutValue;
	ze_bool_t pNeedReload;
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < schedulerCount; ++i) {
		result = zesSchedulerSetTimesliceMode(schedulerHandles[i], &timesliceProperties, &pNeedReload);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set scheduler mode. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}

		if (pNeedReload) {
			INFO("Scheduler mode set successfully. Device driver reload is needed to take effect.\n");
		} else {
			INFO("Scheduler mode set successfully. No device driver reload needed to take effect.\n");
		}
	}

	return result;
}

/**
 * @brief Sets exclusive mode for all scheduler controllers
 *
 * This function configures exclusive mode across all scheduler controllers,
 * allowing a single process to have dedicated access to GPU compute resources
 * without time-sharing or preemption for maximum performance scenarios.
 *
 * @return ze_result_t ZE_RESULT_SUCCESS if exclusive mode set successfully, error code otherwise
 */
ze_result_t scheduler::setExclusiveMode()
{
	ze_bool_t pNeedReload;
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < schedulerCount; ++i) {
		result = zesSchedulerSetExclusiveMode(schedulerHandles[i], &pNeedReload);
		if (result != ZE_RESULT_SUCCESS) {
			ERR("Failed to set scheduler mode. 0x{:X} ({})\n", result, l0_error_to_string(result));
			return result;
		}

		if (pNeedReload) {
			INFO("Scheduler mode set successfully. Device driver reload is needed to take effect.\n");
		} else {
			INFO("Scheduler mode set successfully. No device driver reload needed to take effect.\n");
		}
	}

	return result;
}

/**
 * @brief Initializes the scheduler management module for a device
 *
 * This function performs initial setup of scheduler monitoring capabilities by
 * enumerating all available scheduler controllers on the specified device for
 * subsequent workload management and scheduling operations.
 *
 * @param device Handle to the device for scheduler initialization
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t scheduler::init(zes_device_handle_t device)
{
	TRACING();
	return enumSchedulers(device);
}

/**
 * @brief Performs comprehensive scheduler monitoring runtime operations
 *
 * This function executes a complete scheduler monitoring cycle including property
 * retrieval, current mode checking, timeout properties, and timeslice properties
 * for all scheduler controllers to provide comprehensive scheduling analysis.
 *
 * @param device Handle to the device (unused in current implementation)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful execution, error code otherwise
 */
ze_result_t scheduler::zesRun(UNUSED zes_device_handle_t device)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	for (uint32_t i = 0; i < schedulerCount; ++i) {
		result = getProperties(schedulerHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getCurrentMode(schedulerHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getTimeoutModeProperties(schedulerHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}

		result = getTimesliceProperties(schedulerHandles[i]);
		if (result != ZE_RESULT_SUCCESS) {
			return result;
		}
	}

	return result;
}