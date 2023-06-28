/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device.cpp
 */

#include "pch.h"
#include "gpu_device.h"
#include "device/gpu/gpu_device_stub.h"
#include "device/win_native.h"

#include "infrastructure/configuration.h"

namespace xpum {
    GPUDevice::GPUDevice() {
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

    GPUDevice::GPUDevice(const std::string& id, const zes_device_handle_t& zes_device, const ze_driver_handle_t& ze_driver, std::vector<DeviceCapability>& capabilities) {
        this->id = id;
        this->zes_device_handle = zes_device;
        this->ze_driver_handle = ze_driver;
        for (DeviceCapability& cap : capabilities) {
            this->capabilities.push_back(cap);
        }
    }

    void GPUDevice::getDeviceSusPower(int32_t& susPower, bool& supported) noexcept {
        GPUDeviceStub::instance().toGetDeviceSusPower(zes_device_handle, susPower,supported);
    }

    bool GPUDevice::setDevicePowerSustainedLimits(int32_t powerLimit) noexcept {
        return GPUDeviceStub::instance().toSetPowerSustainedLimits(zes_device_handle, powerLimit);
    }

    void GPUDevice::getDevicePowerMaxLimit(int32_t& max_limit, bool& supported) noexcept {
        max_limit = (int32_t)GPUDeviceStub::instance().toGetDevicePowerLimit(zes_device_handle);
        supported = true;
        return;
    }

    void GPUDevice::getDeviceFrequencyRange(int32_t tileId, double& min, double& max, std::string& clocks, bool& supported) noexcept {
        GPUDeviceStub::instance().toGetDeviceFrequencyRange(zes_device_handle, tileId, min, max, clocks, supported);
    }

    bool GPUDevice::setDeviceFrequencyRange(int32_t tileId, double min, double max) noexcept {
        return GPUDeviceStub::instance().toSetDeviceFrequencyRange(zes_device_handle, tileId, min, max);
    }

    
     void GPUDevice::getFreqAvailableClocks(int32_t tileId, std::vector<double>& clocksList) noexcept {
        return GPUDeviceStub::instance().toGetFreqAvailableClocks(zes_device_handle, tileId, clocksList);
    }

    void GPUDevice::getSimpleEccState(uint8_t& current, uint8_t& pending) noexcept {
        return;
    }

    void GPUDevice::getsubDeviceList(std::vector<int32_t>& subDeviceList) noexcept {
        subDeviceList.clear();
        for (int i = 0; i < this->zes_sub_device_handle_num; i++) {
            subDeviceList.emplace_back(i);
        }
    }
    /* void* GPUDevice::getZesDeviceHandle() noexcept {
        return zes_device_handle;
    }*/

    void GPUDevice::getPower(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetPower(zes_device_handle);
    }

    void GPUDevice::getActuralRequestFrequency(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept {
        data = GPUDeviceStub::instance().toGetActuralRequestFrequency(zes_device_handle, type);
    }

    void GPUDevice::getTemperature(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept {
        data = GPUDeviceStub::instance().toGetTemperature(zes_device_handle, type);
    }

    void GPUDevice::getMemoryUsedUtilization(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept {
        data = GPUDeviceStub::instance().toGetMemoryUsedUtilization(zes_device_handle, type);
    }

    void GPUDevice::getMemoryBandwidth(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetMemoryBandwidth(zes_device_handle);
    }

    void GPUDevice::getMemoryReadWrite(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept {
        data = GPUDeviceStub::instance().toGetMemoryReadWrite(zes_device_handle, type);
    }

    void GPUDevice::getEngineUtilization(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetEngineUtilization(zes_device_handle);
    }

    void GPUDevice::getGPUUtilization(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetGPUUtilization(zes_device_handle);
    }

    void GPUDevice::getEngineGroupUtilization(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept {
        data = GPUDeviceStub::instance().toGetEngineGroupUtilization(zes_device_handle, type);
    }

    void GPUDevice::getEnergy(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetEnergy(zes_device_handle);
    }

    void GPUDevice::getEuActiveStallIdle(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept {
        data = GPUDeviceStub::instance().toGetEuActiveStallIdle(zes_device_handle, ze_driver_handle, type);
    }

    void GPUDevice::getRasError(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept {
        data = GPUDeviceStub::instance().toGetRasError(zes_device_handle, type);
    }

    void GPUDevice::getFrequencyThrottle(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetFrequencyThrottle(zes_device_handle);
    }

    void GPUDevice::getFrequencyThrottleReason(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetFrequencyThrottleReason(zes_device_handle);
    }

    void GPUDevice::getPCIeReadThroughput(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetPCIeReadThroughput(zes_device_handle);
    }

    void GPUDevice::getPCIeWriteThroughput(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetPCIeWriteThroughput(zes_device_handle);
    }

    void GPUDevice::getPCIeRead(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetPCIeRead(zes_device_handle);
    }

    void GPUDevice::getPCIeWrite(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetPCIeWrite(zes_device_handle);
    }

    void GPUDevice::getFabricThroughput(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetFabricThroughput(zes_device_handle);
    }

    void GPUDevice::getPerfMetrics(std::shared_ptr<MeasurementData>& data) noexcept {
        data = GPUDeviceStub::instance().toGetPerfMetrics(zes_device_handle, ze_driver_handle);
    }

    std::map<MeasurementType, std::shared_ptr<MeasurementData>> GPUDevice::getRealtimeMetrics() noexcept {
        updatePDHQuery();
        std::map<MeasurementType, std::shared_ptr<MeasurementData>> datas;
       
        std::shared_ptr<MeasurementData> gpu_utilization = std::make_shared<MeasurementData>();
        int val = -1;
        int scale = 1;

        for (auto type : Configuration::getEnabledMetrics()) {
            if (type == METRIC_COMPUTATION) {
                continue;
            }
            std::shared_ptr<MeasurementData> data = std::make_shared<MeasurementData>();
            switch (type) {
                case xpum::METRIC_POWER:
                    getPower(data);
                    break;
                case xpum::METRIC_ENERGY:
                    getEnergy(data);
                    break;
                case xpum::METRIC_FREQUENCY:
                case xpum::METRIC_REQUEST_FREQUENCY:
                    getActuralRequestFrequency(data, type);
                    break;
                case xpum::METRIC_TEMPERATURE:
                case xpum::METRIC_MEMORY_TEMPERATURE:
                    getTemperature(data, type);
                    break;
                case xpum::METRIC_MEMORY_USED:
                case xpum::METRIC_MEMORY_UTILIZATION:
                    getMemoryUsedUtilization(data, type);
                    break;
                case xpum::METRIC_MEMORY_BANDWIDTH:
                    getMemoryBandwidth(data);
                    break;
                case xpum::METRIC_MEMORY_READ:
                case xpum::METRIC_MEMORY_WRITE:
                case xpum::METRIC_MEMORY_READ_THROUGHPUT:
                    getMemoryReadWrite(data, type);
                    break;
                case xpum::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
                case xpum::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
                case xpum::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION:
                case xpum::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
                case xpum::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION:
                    getEngineGroupUtilization(data, type);
                    if (data->getCurrent() != UINT64_MAX) {
                        val = max(val, (int)data->getCurrent());
                        scale = data->getScale();
                    }
                    break;
                case xpum::METRIC_EU_ACTIVE:
                    getEuActiveStallIdle(data, type);
                    if (!data->getErrors().empty()) {
                        XPUM_LOG_ERROR("GPUDevice::getRealtimeMetrics() retry to invoke getEuActiveStallIdle()");
                        getEuActiveStallIdle(data, type);
                    }
                    break;
                case xpum::METRIC_RAS_ERROR_CAT_RESET:
                case xpum::METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS:
                case xpum::METRIC_RAS_ERROR_CAT_DRIVER_ERRORS:
                case xpum::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE:
                case xpum::METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE:
                case xpum::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE:
                case xpum::METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE:
                case xpum::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE:
                case xpum::METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE:
                    getRasError(data, type);
                    break;
                case xpum::METRIC_FREQUENCY_THROTTLE:
                    getFrequencyThrottle(data);
                    break;
                case xpum::METRIC_PCIE_READ_THROUGHPUT:
                    getPCIeReadThroughput(data);
                    break;
                case xpum::METRIC_PCIE_WRITE_THROUGHPUT:
                    getPCIeWriteThroughput(data);
                    break;
                case xpum::METRIC_PCIE_READ:
                    getPCIeRead(data);
                    break;
                case xpum::METRIC_PCIE_WRITE:
                    getPCIeWrite(data);
                    break;
                case xpum::METRIC_ENGINE_UTILIZATION:
                    getEngineUtilization(data);
                    break;
                case xpum::METRIC_FABRIC_THROUGHPUT:
                    getFabricThroughput(data);
                    break;
                case xpum::METRIC_PERF:
                    getPerfMetrics(data);
                    break;
                case xpum::METRIC_FREQUENCY_THROTTLE_REASON_GPU:
                    getFrequencyThrottleReason(data);
                    break;
                default:
                    break;
            }
            if (data->getErrors().empty()) {
                datas.insert({type, data});
                if (data->hasAdditionalData()) {
                    auto additional_types = data->getAdditionalDataTypes();
                    for (auto additional_type : additional_types) {
                        if (data->getAdditionalData(additional_type) != UINT64_MAX) {
                            std::shared_ptr<MeasurementData> tmp_data = std::make_shared<MeasurementData>();
                            tmp_data->setCurrent(data->getAdditionalData(additional_type));
                            tmp_data->setScale(data->getScale());
                            datas.insert({additional_type, tmp_data});
                        }
                    }
                }
            }
               
        }

        if (val >= 0) {
            gpu_utilization->setCurrent(val);
            gpu_utilization->setScale(scale);
            datas.insert({METRIC_COMPUTATION, gpu_utilization});
            XPUM_LOG_TRACE("set GPU utilization val to {} from compute, media, copy and render group utilization", val);
        }
        return datas;
    }

    GPUDevice::~GPUDevice() {

    }
}