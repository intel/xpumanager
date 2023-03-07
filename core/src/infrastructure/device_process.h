/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_process.h
 */

#pragma once
#include <string>

#include "level_zero/zes_api.h"

namespace xpum {

class device_process {
   private:
    uint32_t processId;
    uint64_t memSize;
    uint64_t sharedSize;
    zes_engine_type_flags_t engine;
    std::string processName;

   public:
    device_process(uint32_t processId, uint64_t memSize, uint64_t sharedSize, zes_engine_type_flags_t engine, std::string processName);
    ~device_process();
    uint32_t getProcessId();
    uint64_t getMemSize();
    uint64_t getSharedSize();
    zes_engine_type_flags_t getEngine();
    std::string getProcessName();
};

} // end namespace xpum
