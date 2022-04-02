/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fabric_throughput_data_handler.h
 */

#pragma once

#include "metric_collection_statistics_data_handler.h"

namespace xpum {

class FabricThroughputDataHandler : public MetricCollectionStatisticsDataHandler {
   public:
    FabricThroughputDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~FabricThroughputDataHandler();

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

    void calculateData(std::shared_ptr<SharedData> &p_data);
};

} // end namespace xpum
