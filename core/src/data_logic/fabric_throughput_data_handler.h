/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fabric_throughput_data_handler.h
 */

#pragma once

#include "multi_metrics_stats_data_handler.h"

namespace xpum {

class FabricThroughputDataHandler : public MultiMetricsStatsDataHandler {
   public:
    FabricThroughputDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~FabricThroughputDataHandler();

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

    void calculateData(std::shared_ptr<SharedData> &p_data);
};

} // end namespace xpum
