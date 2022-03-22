/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file data_logic_persistence_interface.h
 */

#pragma once

#include <map>

#include "infrastructure/const.h"
#include "infrastructure/measurement_data.h"
#include "infrastructure/measurement_type.h"

namespace xpum {

class DataLogicPersistenceInterface {
   public:
    virtual ~DataLogicPersistenceInterface() {}

    virtual void storeMeasurementData(
        MeasurementType type,
        Timestamp_t time,
        std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas) = 0;
};

} // end namespace xpum
