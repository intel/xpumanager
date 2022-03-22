/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file temperature_data_handler.h
 */

#include "statistics_data_handler.h"

namespace xpum {

class TemperatureDataHandler : public StatisticsDataHandler {
   public:
    TemperatureDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~TemperatureDataHandler();
};
} // end namespace xpum
