#pragma once

#include <vector>

#include "infrastructure/init_close_interface.h"
#include "infrastructure/measurement_type.h"

namespace xpum {

class MonitorManagerInterface : public InitCloseInterface {
   public:
    virtual ~MonitorManagerInterface(){};
    virtual void addMetricTask(MeasurementType type, int freq) = 0;
    virtual void removeMetricTask(MeasurementType type) = 0;
    virtual void resetMetricTasksFrequency() = 0;
};

} // end namespace xpum
