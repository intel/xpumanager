/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file temperature_data_handler.cpp
 */

#include "temperature_data_handler.h"

namespace xpum {

TemperatureDataHandler::TemperatureDataHandler(MeasurementType type,
                                               std::shared_ptr<Persistency> &p_persistency)
    : StatisticsDataHandler(type, p_persistency) {
}

TemperatureDataHandler::~TemperatureDataHandler() {
    close();
}
} // end namespace xpum
