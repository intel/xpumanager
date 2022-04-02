/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file frequency_throttle_time_data_handler.cpp
 */

#include "frequency_throttle_time_data_handler.h"

namespace xpum {

FrequencyThrottleTimeDataHandler::FrequencyThrottleTimeDataHandler(MeasurementType type,
                                                                   std::shared_ptr<Persistency>& p_persistency)
    : TimeWeightedAverageDataHandler(type, p_persistency) {
}

FrequencyThrottleTimeDataHandler::~FrequencyThrottleTimeDataHandler() {
    close();
}
} // end namespace xpum
