/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file persistency.h
 */

#pragma once

#include <map>

#include "infrastructure/measurement_data.h"
#include "infrastructure/measurement_type.h"

namespace xpum {

class Persistency {
   public:
    virtual ~Persistency(){};

    virtual void storeMeasurementData(
        MeasurementType type,
        Timestamp_t time,
        std::map<std::string, std::shared_ptr<MeasurementData>>& datas) = 0;
};

} // end namespace xpum
