#include <vector>
#include <atomic>
#include <iostream>

#include "logger.h"
#include "ilegal_parameter_exception.h"
#include "gpu_device_stub.h"
#include "utility.h"
#include "device_manager.h"

DeviceManager::DeviceManager(std::shared_ptr<DataLogicInterface>& p_data_logic) 
  : p_data_logic(p_data_logic) {
  Logger::instance().info("DeviceManager()");
}

DeviceManager::~DeviceManager() {
  close();
  Logger::instance().info("~DeviceManager()");
}

void DeviceManager::init() {
  std::unique_lock<std::mutex> lock(this->mutex);
  std::condition_variable cv;
  std::atomic<bool> ready(false);
  std::weak_ptr<DeviceManager> this_weak_ptr = shared_from_this();

  GPUDeviceStub::instance().discoverDevices([&cv, &ready, this_weak_ptr]
  (std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    auto p_this = this_weak_ptr.lock();
    if (p_this == nullptr) {
      return ;
    }

    if (e != nullptr) {
      Logger::instance().error(std::string("Failed to init device list:") + e->what());
      ready = true;
      cv.notify_all();
      return ;
    }

    if (ret != nullptr) {
      auto p_devices = std::static_pointer_cast<std::vector<std::shared_ptr<Device>>>(ret); 
      
      for (auto& p_device : *p_devices) {
        p_this->devices.emplace_back(p_device);
      }
    }  

    ready = true;
    cv.notify_all();
  });

  while (!ready) {
    cv.wait(lock);
  }
}

void DeviceManager::close() {
}

void DeviceManager::getDeviceList(std::vector<std::shared_ptr<Device>>& devices) {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& p_device : this->devices) {
    devices.emplace_back(p_device);
  }
}

void DeviceManager::getDeviceList(
    DeviceCapability cap, std::vector<std::shared_ptr<Device>>& devices) {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& p_device : this->devices) {
    if (p_device->hasCapability(cap)) {
      devices.emplace_back(p_device);
    }
  }
}

MeasurementData DeviceManager::getRealtimeMeasurementData(
    MeasurementType type, std::string& device_id) {
  std::shared_ptr<Device> p_device = getDevice(device_id);
  if (p_device == nullptr) {
    throw IlegalParameterException("device does not exist");
  }

  DeviceCapability capability = Utility::capabilityFromMeasurementType(type);
  auto method = Utility::getDeviceMethod(capability, p_device.get());
  if (method == nullptr) {
    throw IlegalParameterException("method does not exist");
  }

  std::shared_ptr<MeasurementData> p_data;
  std::shared_ptr<BaseException> exception;
  std::condition_variable data_cv; 
  std::mutex data_mutex; 
  std::atomic<bool> ready(false);
  std::weak_ptr<DeviceManager> this_weak_ptr = shared_from_this();

  method([p_device, this_weak_ptr, &p_data, &data_cv, &exception, &ready]
  (std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) mutable {
    auto p_this = this_weak_ptr.lock();
    if (p_this == nullptr) {
      return ;
    }

    if (e == nullptr && ret != nullptr) {
      p_data = std::static_pointer_cast<MeasurementData>(ret);
    } else if (e != nullptr){
      exception = e;
    }

    ready = true;
    data_cv.notify_all();
  });

  std::unique_lock<std::mutex> lock(data_mutex);
  while (!ready) {
    data_cv.wait(lock);
  }

  if (exception != nullptr) {
    throw (*exception);
  }

  return *p_data;
}

std::shared_ptr<Device> DeviceManager::getDevice(const std::string& id) {
  std::unique_lock<std::mutex> lock(this->mutex);
  for (auto& p_device : this->devices) {      
    if (p_device->getId() == id) {
      return p_device;
    }
  }

  return nullptr;    
}

void DeviceManager::getDeviceSchedulers(const std::string &id, std::vector<Scheduler>& schedulers) {
  std::unique_lock<std::mutex> lock(this->mutex);
  zes_device_handle_t device = getDeviceHandle(id);
  GPUDeviceStub::instance().getSchedulers(device,schedulers);
}

void DeviceManager::getDeviceStandbys(const std::string& id, std::vector<Standby>& standbys) {
  std::unique_lock<std::mutex> lock(this->mutex);
  GPUDeviceStub::instance().getStandbys(getDeviceHandle(id),standbys);
}

void DeviceManager::getDevicePowerProps(const std::string& id, std::vector<Power>& powers) {
  std::unique_lock<std::mutex> lock(this->mutex);
  GPUDeviceStub::instance().getPowerProps(getDeviceHandle(id),powers);
}

void DeviceManager::getDevicePowerLimits(const std::string& id,
                                         Power_sustained_limit_t& sustained_limit,
                                         Power_burst_limit_t& burst_limit,
                                         Power_peak_limit_t& peak_limit) {
  std::unique_lock<std::mutex> lock(this->mutex);
  GPUDeviceStub::instance().getPowerLimits(getDeviceHandle(id), sustained_limit, burst_limit, peak_limit);
}

bool DeviceManager::setDevicePowerSustainedLimits(const std::string& id,
                                                  const Power_sustained_limit_t& sustained_limit) {
  std::unique_lock<std::mutex> lock(this->mutex);
  return GPUDeviceStub::instance().setPowerSustainedLimits(getDeviceHandle(id), sustained_limit);
}

bool DeviceManager::setDevicePowerBurstLimits(const std::string& id,
                                              const Power_burst_limit_t& burst_limit) {
  std::unique_lock<std::mutex> lock(this->mutex);
  return GPUDeviceStub::instance().setPowerBurstLimits(getDeviceHandle(id), burst_limit);
}

bool DeviceManager::setDevicePowerPeakLimits(const std::string& id,
                                             const Power_peak_limit_t& peak_limit) {
  std::unique_lock<std::mutex> lock(this->mutex);
  return GPUDeviceStub::instance().setPowerPeakLimits(getDeviceHandle(id), peak_limit);
}

void DeviceManager::getDeviceFrequencyRanges(const std::string& id,
                                             std::vector<Frequency>& frequencies) {
  std::unique_lock<std::mutex> lock(this->mutex);
  GPUDeviceStub::instance().getFrequencyRanges(getDeviceHandle(id), frequencies);
}

zes_device_handle_t DeviceManager::getDeviceHandle(const std::string& id) {
  for (auto& p_device : this->devices) {      
    if (p_device->getId() == id) {
      return p_device->getDeviceHandle();
    }
  }
  return nullptr;    
}

bool DeviceManager::setDeviceFrequencyRange(const std::string& id,
                                            const Frequency& freq) {
  std::unique_lock<std::mutex> lock(this->mutex);
  return GPUDeviceStub::instance().setFrequencyRange(getDeviceHandle(id), freq);
}