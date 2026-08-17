/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

// Level Zero loader (zel*) API. Not part of Sysman, but needed by callers that
// dispatch to the backend driver directly, bypassing the loader.

#include "sysman_state.h"

// The stub does not wrap the handles it hands out, so translation is a
// pass-through, like in the real loader with handle interception disabled.
ze_result_t zelLoaderTranslateHandle(zel_handle_type_t handleType, void *handleIn, void **handleOut)
{
	(void)handleType;

	sysman_state_lock();
	ze_result_t ret = g_sysman_state.return_values.zelLoaderTranslateHandle;
	sysman_state_unlock();

	if (ret != ZE_RESULT_SUCCESS)
		return ret;
	if (!handleOut)
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;

	*handleOut = handleIn;
	return ZE_RESULT_SUCCESS;
}
