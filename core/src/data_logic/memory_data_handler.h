/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file memory_data_handler.h
 */

#include "statistics_data_handler.h"

namespace xpum {

class MemoryDataHandler : public StatisticsDataHandler {
   public:
    MemoryDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~MemoryDataHandler();
};
} // end namespace xpum
