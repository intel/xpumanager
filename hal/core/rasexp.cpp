/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "rasexp.h"
#include "debug.h"
#include <vector>
#include <map>

/**
 * @brief Initializes the rasExp class with the given driver and device handles
 * @param [in] driver The handle to the ZES driver
 * @param [in] device The handle to the ZES device
 * @return ze_result_t ZE_RESULT_SUCCESS on successful initialization, error code otherwise
 */
ze_result_t rasExp::init(zes_driver_handle_t driver, zes_device_handle_t device)
{
	zesDriver = driver;
	zesDevice = device;
	rasExpEnabled = false;
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves RAS error counts per tile using the experimental API
 * This function is a placeholder for the experimental RAS API that will provide error monitoring capabilities. It
 * currently returns an unsupported feature error.
 * @param [out] rasErrStates Map to store error categories for each error type
 * @return ze_result_t ZE_RESULT_SUCCESS on successful error count retrieval, error code otherwise
 */
ze_result_t rasExp::getErrorsPerTileRasExp(std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> &rasErrStates)
{
	TRACING();
	ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	rasErrStates.clear();
	ERR("Experimental RAS API is not implemented yet.\n");
	return result;
}