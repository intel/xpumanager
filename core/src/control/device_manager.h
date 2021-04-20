#pragma once
#include <mutex>
#include <vector>

#include "data_logic_interface.h"
#include "device_manager_interface.h"

class DeviceManager : public DeviceManagerInterface,
                      public std::enable_shared_from_this<DeviceManager> {
 public:
  DeviceManager(std::shared_ptr<DataLogicInterface>&);

  ~DeviceManager();

  void init() override;

  void close() override;

  void getDeviceList(std::vector<std::shared_ptr<Device>>& devices) override;

  void getDeviceList(DeviceCapability cap,
                     std::vector<std::shared_ptr<Device>>& devices) override;

  MeasurementData getRealtimeMeasurementData(MeasurementType type,
                                             std::string& device_id) override;

 private:
  DeviceManager() = default;

  DeviceManager& operator=(const DeviceManager&) = delete;

  DeviceManager(const DeviceManager& other) = delete;

  std::shared_ptr<Device> getDevice(const std::string& id);

 private:
  std::shared_ptr<DataLogicInterface> p_data_logic;

  std::vector<std::shared_ptr<Device>> devices;

  std::mutex mutex;
};
