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
	process() {}
	~process() {}
	ze_result_t getState(zes_device_handle_t device, std::vector<zes_process_state_t> *processList);
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif