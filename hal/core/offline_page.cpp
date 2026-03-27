/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_page.h"
#include "debug.h"

/**
 * @brief Initializes the pageOffline object with the given driver and device handles.
 *
 * @param[in] driver  The ZES driver handle.
 * @param[in] device  The ZES device handle.
 */
ze_result_t pageOffline::init(UNUSED zes_driver_handle_t driver, UNUSED zes_device_handle_t device)
{
	DBG("Experimental page offline functionality not implemented yet\n");
	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

/**
 * @brief Retrieves the memory page offline state for the device.
 *
 * @param[in,out] pCount          Pointer to the number of offline page entries. On input, specifies
 *                        the size of the @p pPageOfflineInfo vector; on output, the number
 *                        of entries written.
 * @param[out] pPageOfflineInfo Pointer to a vector that will be populated with offline page info.
 * @return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE when the feature is not yet implemented.
 */
ze_result_t pageOffline::getDeviceMemoryPageOfflineState(UNUSED uint32_t *pCount,
														 UNUSED std::vector<memPageOfflineInfo> *pPageOfflineInfo)
{
	DBG("Experimental page offline functionality not implemented yet\n");
	return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
