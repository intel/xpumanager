/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file vgpu_types.h
 */

#pragma once

#include <string>

#include "api/device_model.h"
#include "../include/xpum_structs.h"

namespace xpum {

struct AttrFromConfigFile {
    bool driversAutoprobe;
    bool schedIfIdle;
    uint64_t vfLmem;
    uint64_t vfLmemEcc;
    uint32_t vfContexts;
    uint32_t vfDoorbells;
    uint64_t vfGgtt;
    uint64_t vfExec;
    uint64_t vfPreempt;
    uint64_t pfExec;
    uint64_t pfPreempt;
};

struct DeviceSriovInfo {
    int deviceModel;
    std::string drmPath;
    std::string bdfAddress;
    xpum_ecc_state_t eccState;
    uint64_t lmemSizeFree;
    uint64_t ggttSizeFree;
    uint32_t doorbellFree;
    uint32_t contextFree;
    DeviceSriovInfo():
        deviceModel(0),
        lmemSizeFree(0),
        ggttSizeFree(0),
        doorbellFree(0),
        contextFree(0) {}
};

} // namespace xpum