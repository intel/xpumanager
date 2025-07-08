/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file counter_data_handler.h
 */

#pragma once

#include "stats_data_handler.h"

namespace xpum {

class CounterDataHandler : public StatsDataHandler {
   public:
    CounterDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~CounterDataHandler();

    void counterOverflowDetection(std::shared_ptr<SharedData> &p_data) noexcept;

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

};
} // end namespace xpum