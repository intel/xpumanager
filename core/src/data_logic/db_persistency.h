#pragma once

#include "infrastructure/measurement_type.h"
#include "persistency.h"

namespace xpum {

class DBPersistency : public Persistency {
   public:
    virtual void storeMeasurementData(
        MeasurementType type,
        Timestamp_t time,
        std::map<std::string, MeasurementData>& datas) override;
};

} // end namespace xpum
