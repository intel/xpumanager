/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file frequency_data_handler.h
 */

#include "statistics_data_handler.h"

namespace xpum {

class FrequencyDataHandler : public StatisticsDataHandler {
   public:
    FrequencyDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~FrequencyDataHandler();
};
} // end namespace xpum
