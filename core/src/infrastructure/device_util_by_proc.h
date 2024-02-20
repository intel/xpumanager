/* 
 *  Copyright (C) 2022-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_util_by_proc.h
 */

#pragma once
#include <string>
#include <cstdint>

namespace xpum {

class device_util_by_proc {
private:
    uint32_t deviceId;
    uint32_t processId;
    uint64_t memSize;
    uint64_t sharedMemSize;
    std::string processName;

public:
    device_util_by_proc(uint32_t processId);
    ~device_util_by_proc();
    uint32_t getDeviceId();
    uint32_t getProcessId();
    double getRenderingEngineUtil();
    double getComputeEngineUtil();
    double getCopyEngineUtil();
    double getMediaEnigineUtil();
    double getMediaEnhancementUtil();
    uint64_t getMemSize();
    uint64_t getSharedMemSize(); 
    std::string getProcessName();

    void setval(device_util_by_proc *putil);
    void merge(device_util_by_proc *ptuil);
    void setDeviceId(uint32_t deviceId);
    void setMemSize(uint64_t memSize);
    void setSharedMemSize(uint64_t sharedMemSize);
    void setProcessName(std::string processName);

    char d_name[32];
};

} // end namespace xpum
