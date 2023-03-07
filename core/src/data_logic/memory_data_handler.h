/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file memory_data_handler.h
 */

#include "counter_data_handler.h"

namespace xpum {

class MemoryDataHandler : public CounterDataHandler {
   public:
    MemoryDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~MemoryDataHandler();
};
} // end namespace xpum
