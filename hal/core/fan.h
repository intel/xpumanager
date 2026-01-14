/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FAN_H
#define _FAN_H

#include "sysman.h"

class LIBXPUM_API fan : public sysman
{
private:
	uint32_t fanCount;
	zes_fan_handle_t *fanHandles;

public:
	fan() : fanCount(0), fanHandles(nullptr) {}
	~fan();
	void printSupportedModes(const uint32_t mode);
	ze_result_t enumFans(zes_device_handle_t device);
	ze_result_t getProperties(zes_fan_handle_t fanHandle);
	ze_result_t getConfig(zes_fan_handle_t fanHandle);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
