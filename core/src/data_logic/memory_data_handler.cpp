/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file memory_data_handler.cpp
 */

#include "memory_data_handler.h"

namespace xpum {

MemoryDataHandler::MemoryDataHandler(MeasurementType type,
                                     std::shared_ptr<Persistency> &p_persistency)
    : StatisticsDataHandler(type, p_persistency) {
}

MemoryDataHandler::~MemoryDataHandler() {
    close();
}
} // end namespace xpum
