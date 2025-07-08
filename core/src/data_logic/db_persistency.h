/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file db_persistency.h
 */

#pragma once

#include "infrastructure/measurement_type.h"
#include "persistency.h"

namespace xpum {

class DBPersistency : public Persistency {
   public:
    virtual void storeData2PersistentStorage(
        MeasurementType type,
        Timestamp_t time,
        std::map<std::string, std::shared_ptr<MeasurementData>>& datas) override;
};

} // end namespace xpum
