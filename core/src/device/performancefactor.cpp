/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file performancefactor.cpp
 */

#include "performancefactor.h"

#include "infrastructure/logger.h"

namespace xpum {

PerformanceFactor::PerformanceFactor(bool on_subdevice, uint32_t subdevice_id, zes_engine_type_flags_t engine, double factor) {
    this->engine = engine;
    this->on_subdevice = on_subdevice;
    this->subdevice_id = subdevice_id;
    this->factor = factor;
}

PerformanceFactor::~PerformanceFactor() {
}

zes_engine_type_flags_t PerformanceFactor::getEngine() const {
    return engine;
}

bool PerformanceFactor::onSubdevice() const {
    return on_subdevice;
}

uint32_t PerformanceFactor::getSubdeviceId() const {
    return subdevice_id;
}

double PerformanceFactor::getFactor() const {
    return factor;
}
} // end namespace xpum
