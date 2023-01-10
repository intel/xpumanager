/* 
 *  Copyright (C) 2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_util_by_proc.cpp
 */

#include "device_util_by_proc.h"

namespace xpum {

    device_util_by_proc::device_util_by_proc(uint32_t processId) {
        this->processId = processId;
        this->memSize = 0;
        this->sharedMemSize = 0;
   }

    void device_util_by_proc::setval(device_util_by_proc *putil) {
        this->processId = putil->getProcessId();
        this->deviceId = putil->getDeviceId();
        this->processName = putil->getProcessName();
        this->memSize = putil->getMemSize();
        this->sharedMemSize = putil->getSharedMemSize();
   }

    void device_util_by_proc::merge(device_util_by_proc *putil) {
        this->memSize += putil->getMemSize();
        this->sharedMemSize += putil->getSharedMemSize();
   }

    device_util_by_proc::~device_util_by_proc() {}

    void device_util_by_proc::setDeviceId(uint32_t deviceId) {
        this->deviceId = deviceId;
    }

    void device_util_by_proc::setMemSize(uint64_t memSize) {
        this->memSize = memSize;
    }

    void device_util_by_proc::setSharedMemSize(uint64_t sharedMemSize) {
        this->sharedMemSize = sharedMemSize;
    }

    void device_util_by_proc::setProcessName(std::string processName) {
        this->processName = processName;
    }

    uint32_t device_util_by_proc::getDeviceId() {
        return this->deviceId;
    }

    uint32_t device_util_by_proc::getProcessId() {
        return this->processId;
    }

    uint64_t device_util_by_proc::getMemSize() {
        return this->memSize;
    }

    uint64_t device_util_by_proc::getSharedMemSize() {
        return this->sharedMemSize;
    }

    std::string device_util_by_proc::getProcessName() {
        return this->processName;
    }

    double device_util_by_proc::getComputeEngineUtil() {
        return 0;
    }

    double device_util_by_proc::getRenderingEngineUtil() {
        return 0;
    }

    double device_util_by_proc::getCopyEngineUtil() {
        return 0;
    }   

    double device_util_by_proc::getMediaEnigineUtil() {
        return 0;
    }

    double device_util_by_proc::getMediaEnhancementUtil() {
        return 0;
    }

} // end namespace xpum
