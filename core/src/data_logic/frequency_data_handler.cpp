/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file frequency_data_handler.cpp
 */

#include "frequency_data_handler.h"

namespace xpum {

FrequencyDataHandler::FrequencyDataHandler(MeasurementType type,
                                           std::shared_ptr<Persistency>& p_persistency)
    : StatisticsDataHandler(type, p_persistency) {
}

FrequencyDataHandler::~FrequencyDataHandler() {
    close();
}

} // end namespace xpum
