/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _STANDBY_H
#define _STANDBY_H

#include <vector>

#include "sysman.h"

class LIBXPUM_API standby : public sysman
{
private:
	uint32_t standbyCount;
	zes_standby_handle_t *standbyHandles;
	// Mode each domain had before preventStandby() forced it to NEVER; NEVER means nothing to restore
	std::vector<zes_standby_promo_mode_t> savedModes;
	bool restorePending;
	bool preventApplied;

	void restoreMode();

public:
	standby() : standbyCount(0), standbyHandles(nullptr), restorePending(false), preventApplied(false) {}
	~standby();
	ze_result_t enumStandbyDomains(zes_device_handle_t device);
	ze_result_t getProperties(zes_standby_handle_t standbyHandle);
	ze_result_t getProperties(zes_standby_handle_t standbyHandle, zes_standby_properties_t *props);
	ze_result_t getMode(zes_standby_handle_t standbyHandle);
	ze_result_t getMode(zes_standby_handle_t standbyHandle, zes_standby_promo_mode_t *mode);
	ze_result_t setMode(zes_standby_promo_mode_t mode);
	void preventStandby();

	uint32_t getStandbyCount() const { return standbyCount; }
	zes_standby_handle_t *getStandbyHandles() const { return standbyHandles; }

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif