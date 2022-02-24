#include "device.h"
#include <cstring>
#include "infrastructure/exception/ilegal_parameter_exception.h"
#include "infrastructure/logger.h"

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

bool Device::getProperty(xpum_device_internal_property_name_t name, Property& ret) noexcept {
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

void Device::removeProperty(xpum_device_internal_property_name_t name) {
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

xpum_result_t Device::runFirmwareFlash(const char* filePath, const std::string& toolPath) noexcept {
    return xpum_result_t::XPUM_GENERIC_ERROR;
}

xpum_result_t Device::runFirmwareFlash(const char* filePath) noexcept {
    return xpum_result_t::XPUM_GENERIC_ERROR;
}

xpum_firmware_flash_result_t Device::getFirmwareFlashResult(xpum_firmware_type_t type) noexcept {
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
        case DeviceCapability::METRIC_POWER:
            return [p_device](Callback_t callback) { p_device->getPower(callback); };
        case DeviceCapability::METRIC_FREQUENCY:
            return [p_device](Callback_t callback) { p_device->getActuralFrequency(callback); };
        case DeviceCapability::METRIC_TEMPERATURE:
            return [p_device](Callback_t callback) { p_device->getTemperature(callback, ZES_TEMP_SENSORS_GPU); };
        case DeviceCapability::METRIC_MEMORY_USED:
            return [p_device](Callback_t callback) { p_device->getMemory(callback); };
        case DeviceCapability::METRIC_COMPUTATION:
            return [p_device](Callback_t callback) { p_device->getGPUUtilization(callback); };
        case DeviceCapability::METRIC_ENGINE_UTILIZATION:
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
        case DeviceCapability::METRIC_MEMORY_READ_THROUGHPUT:
            return [p_device](Callback_t callback) { p_device->getMemoryReadThroughput(callback); };
        case DeviceCapability::METRIC_MEMORY_WRITE_THROUGHPUT:
            return [p_device](Callback_t callback) { p_device->getMemoryWriteThroughput(callback); };
        case DeviceCapability::METRIC_ENERGY:
            return [p_device](Callback_t callback) { p_device->getEnergy(callback); };
        case DeviceCapability::METRIC_MEMORY_UTILIZATION:
            return [p_device](Callback_t callback) { p_device->getMemoryUtilization(callback); };
        case DeviceCapability::METRIC_MEMORY_BANDWIDTH:
            return [p_device](Callback_t callback) { p_device->getMemoryBandwidth(callback); };
        case DeviceCapability::METRIC_EU_ACTIVE_STALL_IDLE:
            return [p_device](Callback_t callback) { p_device->getEuActiveStallIdle(callback, MeasurementType::METRIC_EU_ACTIVE); };
        case DeviceCapability::METRIC_RAS_ERROR:
            return [p_device](Callback_t callback) { p_device->getRasErrorOnSubdevice(callback); };
        case DeviceCapability::METRIC_REQUEST_FREQUENCY:
            return [p_device](Callback_t callback) { p_device->getRequestFrequency(callback); };
        case DeviceCapability::METRIC_MEMORY_TEMPERATURE:
            return [p_device](Callback_t callback) { p_device->getTemperature(callback, ZES_TEMP_SENSORS_MEMORY); };
        case DeviceCapability::METRIC_FREQUENCY_THROTTLE:
            return [p_device](Callback_t callback) { p_device->getFrequencyThrottle(callback); };
        case DeviceCapability::METRIC_PCIE_READ_THROUGHPUT:
            return [p_device](Callback_t callback) { p_device->getPCIeReadThroughput(callback); };
        case DeviceCapability::METRIC_PCIE_WRITE_THROUGHPUT:
            return [p_device](Callback_t callback) { p_device->getPCIeWriteThroughput(callback); };
        case DeviceCapability::METRIC_PCIE_READ:
            return [p_device](Callback_t callback) { p_device->getPCIeRead(callback); };
        case DeviceCapability::METRIC_PCIE_WRITE:
            return [p_device](Callback_t callback) { p_device->getPCIeWrite(callback); };
        default:
            break;
    }
    return nullptr;
}

void Device::addEngine(uint64_t handle, zes_engine_group_t type, bool on_subdevice, uint32_t subdevice_id) {
    std::unique_lock<std::mutex> lock(this->mutex);
    if (engines.find(handle) == engines.end()) {
        EngineInfo engine_info(type,on_subdevice,subdevice_id);
        uint32_t index = 0;
        for (auto& engine : engines) {
            if (engine.second.getSubdeviceId() == subdevice_id && engine.second.getType() == type) {
                index++;
            }
        }
        engine_info.setIndex(index);
        engines.insert(std::make_pair(handle , engine_info));
    }
}

uint32_t Device::getEngineCount() noexcept {
    std::unique_lock<std::mutex> lock(this->mutex);
    return engines.size();
}

uint32_t Device::getEngineCount(int32_t subdevice_id, zes_engine_group_t type) {
    std::unique_lock<std::mutex> lock(this->mutex);
    uint32_t count = 0;
    for (auto& engine:engines) {
        if ((subdevice_id == -1 || engine.second.getSubdeviceId() == (uint32_t)subdevice_id) 
            && (type == ZES_ENGINE_GROUP_FORCE_UINT32 || engine.second.getType() == type)) {
            count++;
        }
    }
    return count;
}

uint32_t Device::getEngineIndex(uint64_t handle) {
    if (engines.find(handle) != engines.end()) {
        return engines[handle].getIndex();
    }
    return std::numeric_limits<uint32_t>::max();
}

} // end namespace xpum
