/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file time_weighted_average_data_handler.h
 */

#pragma once

#include "stats_data_handler.h"

namespace xpum {

class TimeWeightedAverageDataHandler : public StatsDataHandler {
   public:
    TimeWeightedAverageDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~TimeWeightedAverageDataHandler();

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

    void counterOverflowDetection(std::shared_ptr<SharedData> &p_data) noexcept;

    void calculateData(std::shared_ptr<SharedData> &p_data);
};
} // end namespace xpum