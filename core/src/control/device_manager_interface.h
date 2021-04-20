#pragma once

#include <mutex>
#include <vector>

#include "device_capability.h"
#include "init_close_interface.h"
#include "device.h"
#include "measurement_data.h"
#include "measurement_type.h"

class DeviceManagerInterface : public InitCloseInterface {
 public:
  virtual ~DeviceManagerInterface() {}

  virtual void getDeviceList(std::vector<std::shared_ptr<Device>>& devices) = 0;

  virtual void getDeviceList(DeviceCapability cap,
                             std::vector<std::shared_ptr<Device>>& devices) = 0;

  virtual MeasurementData getRealtimeMeasurementData(
      MeasurementType type, std::string& device_id) = 0;
};
