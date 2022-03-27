/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_process.cpp
 */

#include "device_process.h"

namespace xpum {
device_process::device_process(uint32_t processId, uint64_t memSize, uint64_t sharedSize, zes_engine_type_flags_t engine, std::string processName) {
    this->processId = processId;
    this->memSize = memSize;
    this->sharedSize = sharedSize;
    this->engine = engine;
    this->processName = processName;
}
device_process::~device_process() {}

uint32_t device_process::getProcessId() {
    return this->processId;
}

uint64_t device_process::getMemSize() {
    return this->memSize;
}

uint64_t device_process::getSharedSize() {
    return this->sharedSize;
}

zes_engine_type_flags_t device_process::getEngine() {
    return this->engine;
}

std::string device_process::getProcessName() {
    return this->processName;
}

} // end namespace xpum
