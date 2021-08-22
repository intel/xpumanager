#include "logger.h"
#include "gpu_device_stub.h"
#include "gpu_device.h"


GPUDevice::GPUDevice() {
  Logger::instance().info("GPUDevice()");
}

GPUDevice::GPUDevice(const std::string& id,
                     const zes_device_handle_t& zes_device,
                     std::vector<DeviceCapability>& capabilities) {
  this->id = id;
  this->zes_device_handle = zes_device;                       
  for (DeviceCapability& cap : capabilities) {
    this->capabilities.push_back(cap);
  }
}

GPUDevice::~GPUDevice() {
  Logger::instance().info("~GPUDevice()");
}

void GPUDevice::getPower(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getPower(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getActuralFrequency(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getActuralFrequency(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getTemperature(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getTemperature(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getMemory(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getMemory(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getMemoryRead(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getMemoryRead(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getMemoryWrite(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getMemoryWrite(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getEnergy(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getEnergy(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}

void GPUDevice::getEngineUtilization(Callback_t callback) noexcept {
  GPUDeviceStub::instance().getEngineUtilization(zes_device_handle, 
    [callback](std::shared_ptr<void> ret, std::shared_ptr<BaseException> e) {
    callback(ret, e);
  });    
}
