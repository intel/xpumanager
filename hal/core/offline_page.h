/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _PAGE_OFFLINE_H
#define _PAGE_OFFLINE_H

#include "sysman.h"
#include <vector>
#include <zes_api.h>

typedef struct memPageOfflineInfo
{
	uint64_t pageAddress; // address of the page
	uint32_t pageSize;	  // size of the page in bytes
} memPageOfflineInfo;

class LIBXPUM_API pageOffline
{
private:
	zes_driver_handle_t zesDriver;
	zes_device_handle_t zesDevice;

public:
	pageOffline() : zesDriver(nullptr), zesDevice(nullptr) {}
	ze_result_t init(zes_driver_handle_t driver, zes_device_handle_t device);
	ze_result_t getDeviceMemoryPageOfflineState(uint32_t *pCount, std::vector<memPageOfflineInfo> *pPageOfflineInfo);
};

#endif
