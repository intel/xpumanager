#pragma once

#include "metric_statistics_data_handler.h"

namespace xpum {

class CounterDataHandler : public MetricStatisticsDataHandler {
   public:
    CounterDataHandler(MeasurementType type, std::shared_ptr<Persistency> &p_persistency);

    virtual ~CounterDataHandler();

    void counterOverflowDetection(std::shared_ptr<SharedData> &p_data) noexcept;

    virtual void handleData(std::shared_ptr<SharedData> &p_data) noexcept;

};
} // end namespace xpum