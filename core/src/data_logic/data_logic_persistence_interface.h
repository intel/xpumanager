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
        MeasurementType type, Timestamp_t time,
        std::map<std::string, std::shared_ptr<MeasurementData>>& datas) = 0;
};

} // end namespace xpum
