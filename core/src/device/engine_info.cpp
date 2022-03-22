/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file engine_info.cpp
 */

#include "engine_info.h"

#include "infrastructure/logger.h"

namespace xpum {

EngineInfo::EngineInfo() {
}

EngineInfo::EngineInfo(zes_engine_group_t type, bool on_subdevice, uint32_t subdevice_id) {
    this->type = type;
    this->on_subdevice = on_subdevice;
    this->subdevice_id = subdevice_id;
}

EngineInfo::EngineInfo(const EngineInfo& other) {
    this->type = other.type;
    this->on_subdevice = other.on_subdevice;
    this->subdevice_id = other.subdevice_id;
    this->index = other.index;
}

EngineInfo::~EngineInfo() {
}

zes_engine_group_t EngineInfo::getType() const {
    return type;
}

bool EngineInfo::onSubdevice() const {
    return on_subdevice;
}

uint32_t EngineInfo::getSubdeviceId() const {
    return subdevice_id;
}

void EngineInfo::setIndex(uint32_t index) {
    this->index = index;
}

uint32_t EngineInfo::getIndex() const {
    return index;
}

} // end namespace xpum
