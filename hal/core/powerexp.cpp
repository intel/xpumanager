/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "powerexp.h"
#include "debug.h"
#include <vector>

/**
 * @brief Initializes the powerExp class with the given driver and device handles
 * @param [in] driver The handle to the ZES driver
 * @param [in] device The handle to the ZES device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t powerExp::init(zes_driver_handle_t driver, zes_device_handle_t device)
{
	zesDriver = driver;
	zesDevice = device;
	powerExpEnabled = false;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves instant and average power usage using the experimental API
 * This function is a placeholder for the experimental Power Usage API.
 * It currently returns an unsupported feature error.
 * @param [out] powerUsages Vector to populate with per-domain power usage entries
 * @return ze_result_t ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 */
ze_result_t powerExp::getPowerUsage(UNUSED std::vector<power_usage_exp_t> &powerUsages)
{
	TRACING();
	ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	ERR("Experimental Power Usage API is not implemented yet.\n");
	return result;
}

/**
 * @brief Retrieves power limits using the experimental API
 * This function is a placeholder for the experimental Power Limits API.
 * It currently returns an unsupported feature error.
 * @param [out] powerLimits Vector to populate with per-domain power limit entries
 * @return ze_result_t ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 */
ze_result_t powerExp::getPowerLimits(UNUSED std::vector<power_limits_exp_t> &powerLimits)
{
	TRACING();
	ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	ERR("Experimental Power Limits API is not implemented yet.\n");
	return result;
}

/**
 * @brief Sets power limit for a specific power domain using the experimental API
 * This function is a placeholder for the experimental Power Limits API.
 * It currently returns an unsupported feature error.
 * @param [in] limitMw Power limit value in milliwatts
 * @return ze_result_t ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
 */
ze_result_t powerExp::setPowerLimit(UNUSED uint32_t limitMw)
{
	TRACING();
	ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	ERR("Experimental Set Power Limit API is not implemented yet.\n");
	return result;
}
