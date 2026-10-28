/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _SYSPROCESS_H
#define _SYSPROCESS_H

#include "sysman.h"
#include <cstdint>
#include <functional>
#include <vector>

/** zesDeviceProcessesGetState() with the device bound; tests substitute a fake. */
using ProcessStateQuery = std::function<ze_result_t(uint32_t *count, zes_process_state_t *states)>;

/** Result of fetchProcessStates(). */
struct ProcessFetch
{
	ze_result_t status{};
	std::vector<zes_process_state_t> states;
};

/**
 * @brief Fetches the process list in one query when it fits in @p capacityHint entries
 *
 * Each zesDeviceProcessesGetState() call makes the runtime walk every process's fds, so the
 * count-only call before the data call doubled that cost. A buffer that is big enough gets the
 * whole list and the real count in one call; a list that outgrew it is retried with the size
 * the runtime reported (from a count-only query when the short-buffer call left the count as
 * it was), a fixed number of times.
 *
 * @param[in] query         Bound zesDeviceProcessesGetState().
 * @param[in] capacityHint  Entries to offer on the first call; at least 1 is used.
 * @retval {ZE_RESULT_SUCCESS, states}            the list, sized to the real count
 * @retval {ZE_RESULT_ERROR_INVALID_SIZE, empty}  the list kept growing past every retry
 * @retval {other ZE_RESULT_ERROR_*, empty}       the error of the data or the count-only query
 */
[[nodiscard]] LIBXPUM_API ProcessFetch fetchProcessStates(const ProcessStateQuery &query, uint32_t capacityHint);

class LIBXPUM_API process : public sysman
{
public:
	process() = default;
	~process() override = default;

	/**
	 * @brief Queries all processes currently using a device.
	 *
	 * Calls zesDeviceProcessesGetState() once when the list fits (see
	 * fetchProcessStates()), filters out idle/unknown engine
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
