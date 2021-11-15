#include "device.h"

#include <cstring>

#include "infrastructure/exception/ilegal_parameter_exception.h"

namespace xpum {

std::string Device::getId() noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    return id;
}

void Device::getCapability(std::vector<DeviceCapability>& capabilites) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& cap : capabilities) {
        capabilites.push_back(cap);
    }
}

bool Device::hasCapability(DeviceCapability& cap) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& existing_cap : capabilities) {
        if (existing_cap == cap) {
            return true;
        }
    }
    return false;
}

void Device::getProperties(std::vector<Property>& properties) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& prop : this->properties) {
        properties.push_back(prop);
    }
}

bool Device::getProperty(xpum_device_property_name_t name, Property& ret) noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& prop : this->properties) {
        if (prop.getName() == name) {
            ret.setValue(prop.getValue());
            return true;
        }
    }

    return false;
}

void Device::addCapability(DeviceCapability& capability) {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto& cap : capabilities) {
        if (cap == capability) {
            return;
        }
    }

    this->capabilities.push_back(capability);
}

void Device::removeCapability(DeviceCapability& capability) {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto it = capabilities.begin(); it != capabilities.end(); ++it) {
        if (*it == capability) {
            capabilities.erase(it);
            return;
        }
    }
}

void Device::addProperty(Property prop) {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        if (prop.getName() == it->getName()) {
            it->setValue(prop.getValue());
            return;
        }
    }

    this->properties.push_back(prop);
}

void Device::removeProperty(xpum_device_property_name_t name) {
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        if (it->getName() == name) {
            properties.erase(it);
            return;
        }
    }
}

zes_device_handle_t Device::getDeviceHandle() {
    std::unique_lock<std::mutex> lock(this->mutex);
    return zes_device_handle;
}

bool Device::runFirmwareFlash(const char* filePath, const std::string& toolPath) noexcept {
    return false;
}

xpum_firmware_flash_result_t Device::getFirmwareFlashResult() noexcept {
    return XPUM_DEVICE_FIRMWARE_FLASH_OK;
}

ze_device_handle_t Device::getDeviceZeHandle() {
    std::unique_lock<std::mutex> lock(this->mutex);
    return ze_device_handle;
}

ze_driver_handle_t Device::getDriverHandle() {
    std::unique_lock<std::mutex> lock(this->mutex);
    return ze_driver_handle;
}

bool Device::isUpgradingFw(void) noexcept {
    return false;
}

std::function<void(Callback_t)> Device::getDeviceMethod(DeviceCapability& capability, Device* p_device) {
    switch (capability) {
        case DeviceCapability::POWER:
        case DeviceCapability::METRIC_POWER:
            return [p_device](Callback_t callback) { p_device->getPower(callback); };
        case DeviceCapability::FREQUENCY:
        case DeviceCapability::METRIC_FREQUENCY:
            return [p_device](Callback_t callback) { p_device->getActuralFrequency(callback); };
        case DeviceCapability::TEMPERATURE:
        case DeviceCapability::METRIC_TEMPERATURE:
            return [p_device](Callback_t callback) { p_device->getTemperature(callback, ZES_TEMP_SENSORS_GPU); };
        case DeviceCapability::MEMORY:
        case DeviceCapability::METRIC_MEMORY_USED:
            return [p_device](Callback_t callback) { p_device->getMemory(callback); };
        case DeviceCapability::ENGINE_UTILIZATION:
        case DeviceCapability::METRIC_COMPUTATION:
            return [p_device](Callback_t callback) { p_device->getEngineUtilization(callback); };
        case DeviceCapability::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
            return [p_device](Callback_t callback) { p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_COMPUTE_ALL); };
        case DeviceCapability::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
            return [p_device](Callback_t callback) { p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_MEDIA_ALL); };
        case DeviceCapability::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION:
            return [p_device](Callback_t callback) { p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_COPY_ALL); };
        case DeviceCapability::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
            return [p_device](Callback_t callback) { p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_RENDER_ALL); };
        case DeviceCapability::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION:
            return [p_device](Callback_t callback) { p_device->getEngineGroupUtilization(callback, ZES_ENGINE_GROUP_3D_ALL); };
        case DeviceCapability::METRIC_MEMORY_READ:
            return [p_device](Callback_t callback) { p_device->getMemoryRead(callback); };
        case DeviceCapability::METRIC_MEMORY_WRITE:
            return [p_device](Callback_t callback) { p_device->getMemoryWrite(callback); };
        case DeviceCapability::METRIC_ENERGY:
            return [p_device](Callback_t callback) { p_device->getEnergy(callback); };
        case DeviceCapability::METRIC_MEMORY_UTILIZATION:
            return [p_device](Callback_t callback) { p_device->getMemoryUtilization(callback); };
        case DeviceCapability::METRIC_MEMORY_BANDWIDTH:
            return [p_device](Callback_t callback) { p_device->getMemoryBandwidth(callback); };
        case DeviceCapability::METRIC_EU_ACTIVE_STALL_IDLE:
            return [p_device](Callback_t callback) { p_device->getEuActiveStallIdle(callback, MeasurementType::METRIC_EU_ACTIVE); };
        case DeviceCapability::METRIC_RAS_ERROR_CAT_RESET:
            return [p_device](Callback_t callback) { p_device->getRasError(callback, ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
        case DeviceCapability::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
            return [p_device](Callback_t callback) { p_device->getRasErrorOnSubdevice(callback, ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS, ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
        case DeviceCapability::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS:
            return [p_device](Callback_t callback) { p_device->getRasErrorOnSubdevice(callback, ZES_RAS_ERROR_CAT_DRIVER_ERRORS, ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
        case DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
            return [p_device](Callback_t callback) { p_device->getRasErrorOnSubdevice(callback, ZES_RAS_ERROR_CAT_CACHE_ERRORS, ZES_RAS_ERROR_TYPE_CORRECTABLE); };
        case DeviceCapability::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
            return [p_device](Callback_t callback) { p_device->getRasErrorOnSubdevice(callback, ZES_RAS_ERROR_CAT_CACHE_ERRORS, ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
        case DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
            return [p_device](Callback_t callback) { p_device->getRasErrorOnSubdevice(callback, ZES_RAS_ERROR_CAT_DISPLAY_ERRORS, ZES_RAS_ERROR_TYPE_CORRECTABLE); };
        case DeviceCapability::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
            return [p_device](Callback_t callback) { p_device->getRasErrorOnSubdevice(callback, ZES_RAS_ERROR_CAT_DISPLAY_ERRORS, ZES_RAS_ERROR_TYPE_UNCORRECTABLE); };
        case DeviceCapability::METRIC_REQUEST_FREQUENCY:
            return [p_device](Callback_t callback) { p_device->getRequestFrequency(callback); };
        case DeviceCapability::METRIC_MEMORY_TEMPERATURE:
            return [p_device](Callback_t callback) { p_device->getTemperature(callback, ZES_TEMP_SENSORS_MEMORY); };
        default:
            break;
    }
    return nullptr;
}

} // end namespace xpum
