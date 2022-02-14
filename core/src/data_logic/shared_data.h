#pragma once

#include <map>

#include "infrastructure/const.h"
#include "infrastructure/measurement_data.h"

namespace xpum {

class SharedData {
   public:
    SharedData(Timestamp_t time, std::shared_ptr<std::map<std::string, std::shared_ptr<MeasurementData>>> datas);

    virtual ~SharedData();

   public:
    std::map<std::string, std::shared_ptr<MeasurementData>>& getData() noexcept;

    Timestamp_t getTime() noexcept;

   private:
    Timestamp_t time;

    std::map<std::string, std::shared_ptr<MeasurementData>> datas;
};

} // end namespace xpum
