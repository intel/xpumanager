/* 
 *  Copyright (C) 2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_util_by_proc.cpp
 */

#include "device_util_by_proc.h"

namespace xpum {

    device_util_by_proc::device_util_by_proc(uint32_t processId) {
        this->processId = processId;
        //The init value is also a flag to remove invalid process (e.g., 
        //stopped after the first round of data reading)
        elapsed = 0;
        this->memSize = 0;
        this->sharedMemSize = 0;
        for (int i = 0; i < 2; i++) {
            this->ceData[i] = 0;
            this->cpyData[i] = 0;
            this->reData[i] = 0;
            this->meData[i] = 0;
            this->meeData[i] = 0;
        }
    }

    void device_util_by_proc::setval(device_util_by_proc *putil) {
        this->processId = putil->getProcessId();
        this->deviceId = putil->getDeviceId();
        this->processName = putil->getProcessName();
        this->memSize = putil->getMemSize();
        this->sharedMemSize = putil->getSharedMemSize();
        for (int i = 0; i < 2; i++) {
            this->ceData[i] = putil->ceData[i];
            this->cpyData[i] = putil->cpyData[i];
            this->reData[i] = putil->reData[i];
            this->meData[i] = putil->meData[i];
            this->meeData[i] = putil->meeData[i];
        }
    }

    void device_util_by_proc::merge(device_util_by_proc *putil) {
        this->memSize += putil->getMemSize();
        this->sharedMemSize += putil->getSharedMemSize();
        for (int i = 0; i < 2; i++) {
            this->ceData[i] += putil->ceData[i];
            this->cpyData[i] += putil->cpyData[i];
            this->reData[i] += putil->reData[i];
            this->meData[i] += putil->meData[i];
            this->meeData[i] += putil->meeData[i];
        }
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
        return (ceData[1] - ceData[0]) * 1.0 * 100 / elapsed;
    }

    double device_util_by_proc::getRenderingEngineUtil() {
        return (reData[1] - reData[0]) * 1.0 * 100 / elapsed;
    }

    double device_util_by_proc::getCopyEngineUtil() {
        return (cpyData[1] - cpyData[0]) * 1.0 * 100 / elapsed;
    }   

    double device_util_by_proc::getMediaEnigineUtil() {
        return (meData[1] - meData[0]) * 1.0 * 100 / (elapsed * 2);
    }

    double device_util_by_proc::getMediaEnhancementUtil() {
        return (meeData[1]- meeData[0]) * 1.0 * 100 / elapsed; 
    }

} // end namespace xpum
