/* 
 *  Copyright (C) 2023-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file vgpu_manager.h
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "../include/xpum_structs.h"
#include "./vgpu_types.h"
#include "device/device.h"

namespace xpum {

class VgpuManager {
private:

    bool loadSriovData(xpum_device_id_t deviceId ,DeviceSriovInfo &datas);

    bool readConfigFromFile(xpum_device_id_t deviceId, uint32_t numVfs, AttrFromConfigFile &attrs);

    void writeFile(const std::string path, const std::string& content);

    xpum_result_t vgpuValidateDevice(xpum_device_id_t deviceId);

    bool createVfInternal(const DeviceSriovInfo& deviceInfo, AttrFromConfigFile& attrs, uint32_t numVfs, uint64_t lmem);
    
    void writeVfAttrToSysfs(std::string vfDir, AttrFromConfigFile attrs, uint64_t lmem);

    xpum_result_t getVfEngineUtil(xpum_device_id_t deviceId,
        std::vector<xpum_vf_metric_t> &metrics, uint32_t *count = nullptr);

    xpum_result_t getVfMemUtil(xpum_device_id_t deviceId,
        std::vector<xpum_vf_metric_t> &metrics, uint32_t *count = nullptr);

    std::mutex mutex;

public:

    // Check whether resources is enough before, then create VF
    xpum_result_t createVf(xpum_device_id_t deviceId, xpum_vgpu_config_t* config);

    // List VF info
    xpum_result_t getFunctionList(xpum_device_id_t deviceId, std::vector<xpum_vgpu_function_info_t> &functionList);

    // Clear all VFs
    xpum_result_t removeAllVf(xpum_device_id_t deviceId);

    // It returns metric count instead of metrics if count is not nullptr
    xpum_result_t getVfMetrics(xpum_device_id_t deviceId, std::vector<xpum_vf_metric_t> &metrics, uint32_t *count = nullptr);
    
};

} // namespace xpum
