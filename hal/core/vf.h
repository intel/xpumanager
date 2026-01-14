/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _VF_H
#define _VF_H

#include "sysman.h"
#include <osvf.h>

class LIBXPUM_API vf : public sysman
{
private:
	uint32_t vfEnabledCount;
	zes_vf_handle_t *vfEnabledHandles;

public:
	vf() : vfEnabledCount(0), vfEnabledHandles(nullptr) {}
	~vf();
	ze_result_t enumEnabledVF(zes_device_handle_t device);
	ze_result_t getVFCapabilities(zes_vf_handle_t vfHandle, zes_vf_exp2_capabilities_t *outCaps = nullptr);
	ze_result_t getVFMemoryUtilization(zes_vf_handle_t vfHandle, zes_vf_util_mem_exp2_t *outMem = nullptr);
	ze_result_t getVFEngineUtilization(zes_vf_handle_t vfHandle, zes_vf_util_engine_exp2_t *outEngine = nullptr);
	ze_result_t createVFs(DeviceSriovInfo *deviceInfo);
	ze_result_t removeVFs(DeviceSriovInfo *deviceInfo);
	ze_result_t listVFs(DeviceSriovInfo *deviceInfo, std::vector<DeviceSriovInfo> &vfDeviceInfoList);
	ze_result_t getVFStatsList(DeviceSriovInfo *deviceInfo, std::vector<VFStatsInfo> &statsList);
	bool vmxSupport();
	bool iommuSupport();
	bool sriovSupport(DeviceSriovInfo *deviceInfo);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
