/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_utilization_data_handler.h
 */

#pragma once

#include "stats_data_handler.h"

namespace xpum {

class PerfMetricsHandler : public StatsDataHandler {
   public:
    PerfMetricsHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~PerfMetricsHandler();

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

    void calculateData(std::shared_ptr<SharedData> &p_data);

   private:
    
};
} // end namespace xpum
