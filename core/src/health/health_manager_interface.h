#pragma once

#include <vector>
#include <string>

#include "init_close_interface.h"
#include "health_data_type.h"

class HealthManagerInterface : public InitCloseInterface {
 public:
  virtual ~HealthManagerInterface() {};

  virtual void getDeviceHealthStatus(const std::string& id, HealthType& type, HealthStatus& status, std::string& description) = 0;
};