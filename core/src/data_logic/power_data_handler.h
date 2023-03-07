/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file power_data_handler.h
 */

#include "time_weighted_average_data_handler.h"

namespace xpum {

class PowerDataHandler : public TimeWeightedAverageDataHandler {
   public:
    PowerDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~PowerDataHandler();
};
} // end namespace xpum
