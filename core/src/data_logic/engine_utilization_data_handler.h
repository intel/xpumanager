#pragma once

#include "metric_statistics_data_handler.h"

namespace xpum {

class EngineUtilizationDataHandler : public MetricStatisticsDataHandler {
   public:
    EngineUtilizationDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~EngineUtilizationDataHandler();

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

    void calculateData(std::shared_ptr<SharedData> &p_data);

   private:
    uint32_t getAverage(std::vector<uint32_t> &datas);
};
} // end namespace xpum
