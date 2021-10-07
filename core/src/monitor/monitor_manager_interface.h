#pragma once

#include <vector>

#include "infrastructure/init_close_interface.h"

class MonitorManagerInterface : public InitCloseInterface {
 public:
  virtual ~MonitorManagerInterface() {};
  virtual void addMetricTask(MeasurementType type, int freq) = 0;
  virtual void removeMetricTask(MeasurementType type) = 0;
  virtual void resetMetricTasksFrequency(int freq) = 0;
};
