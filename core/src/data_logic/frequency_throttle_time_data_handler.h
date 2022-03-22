/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file frequency_throttle_time_data_handler.h
 */

#include "time_weighted_average_data_handler.h"

namespace xpum {

class FrequencyThrottleTimeDataHandler : public TimeWeightedAverageDataHandler {
   public:
    FrequencyThrottleTimeDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~FrequencyThrottleTimeDataHandler();
};
} // end namespace xpum
