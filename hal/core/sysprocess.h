/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYSPROCESS_H
#define _SYSPROCESS_H

#include "sysman.h"
#include <vector>

class LIBXPUM_API process : public sysman
{
public:
	process() = default;
	~process() override = default;

	/**
	 * @brief Queries all processes currently using a device.
	 *
	 * Calls zesDeviceProcessesGetState(), filters out idle/unknown engine
	 * entries, and replaces memSize with fdinfo-based values to correct the
	 * drm-client-id double-counting bug in the Level Zero implementation.
	 *
	 * @param[in]  device       Sysman device handle.
	 * @param[out] processList  Populated with one entry per active process.
	 *
	 * @retval ZE_RESULT_SUCCESS       Enumeration succeeded.
	 * @retval ZE_RESULT_ERROR_*       Level Zero error from the underlying call.
	 */
	[[nodiscard]] ze_result_t getState(zes_device_handle_t device, std::vector<zes_process_state_t> *processList);

	/**
	 * @brief Runs a full process-monitoring cycle (used by the sysman loop).
	 *
	 * @param[in] device  Sysman device handle.
	 *
	 * @retval ZE_RESULT_SUCCESS  Cycle completed without error.
	 * @retval ZE_RESULT_ERROR_*  Propagated from getState().
	 */
	[[nodiscard]] ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
