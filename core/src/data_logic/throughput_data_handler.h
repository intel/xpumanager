/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file throughput_data_handler.h
 */

#pragma once

#include "time_weighted_average_data_handler.h"

namespace xpum {

class ThroughputDataHandler : public TimeWeightedAverageDataHandler {
   public:
    ThroughputDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~ThroughputDataHandler();
};
} // end namespace xpum