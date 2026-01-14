/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _STANDBY_H
#define _STANDBY_H

#include "sysman.h"

class LIBXPUM_API standby : public sysman
{
private:
	uint32_t standbyCount;
	zes_standby_handle_t *standbyHandles;

public:
	standby() : standbyCount(0), standbyHandles(nullptr) {}
	~standby();
	ze_result_t enumStandbyDomains(zes_device_handle_t device);
	ze_result_t getProperties(zes_standby_handle_t standbyHandle);
	ze_result_t getMode(zes_standby_handle_t standbyHandle);
	ze_result_t setMode(zes_standby_promo_mode_t mode);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif