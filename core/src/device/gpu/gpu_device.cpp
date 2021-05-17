#include "logger.h"
#include "gpu_device_stub.h"
#include "gpu_device.h"


GPUDevice::GPUDevice() {
  Logger::instance().info("GPUDevice()");
}

GPUDevice::GPUDevice(const std::string& id,
                     std::vector<DeviceCapability>& capabilities) {
  this->id = id;                       
  for (DeviceCapability& cap : capabilities) {
    this->capabilities.push_back(cap);
  }
}

GPUDevice::~GPUDevice() {
  Logger::instance().info("~GPUDevice()");
}

void GPUDevice::getPower(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getPower(id.c_str(), 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getActuralFrequency(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getActuralFrequency(id.c_str(), 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getTemperature(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getTemperature(id.c_str(), 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}
