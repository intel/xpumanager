/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device_stub.h
 */

#pragma once
#pragma warning(disable : 4996)

#include <mutex>

#include "device/device.h"
#include "device/performancefactor.h"
#include "device/pcie_manager.h"

namespace xpum {

    class GPUDeviceStub {
    public:
        static GPUDeviceStub& instance();

        static PCIeManager pcie_manager;

        static std::shared_ptr<std::vector<std::shared_ptr<Device>>> toDiscover();

        void toGetDeviceSusPower(const zes_device_handle_t& device, int32_t& Sus_power, bool& Sus_supported) noexcept;

        bool toSetPowerSustainedLimits(const zes_device_handle_t& device, int32_t powerLimit) noexcept;

        void toGetDeviceFrequencyRange(const zes_device_handle_t& device, int32_t tileId, double& min, double& max, std::string& clocks, bool& supported) noexcept;

        bool getEccState(const zes_device_handle_t& device, MemoryEcc& ecc);

        bool toSetDeviceFrequencyRange(const zes_device_handle_t& device, int32_t tileId, double min, double max) noexcept;

        static void toGetDeviceMediaEngineCount(const zes_device_handle_t& device, uint32_t& media_engine_count, uint32_t& meida_enhancement_engine_count, int32_t deviceId) noexcept;
        static void getPerformanceFactor(const zes_device_handle_t& device, std::vector<PerformanceFactor>& pf);

        void toGetSimpleEccState(uint8_t& current, uint8_t& pending) noexcept;
        uint32_t toGetDeviceId(const zes_device_handle_t& device) noexcept;
        uint32_t toGetDevicePowerLimit(const zes_device_handle_t& device) noexcept;
        void toGetFreqAvailableClocks(const zes_device_handle_t& device, int32_t tileId, std::vector<double>& clocksList) noexcept;

        std::shared_ptr<MeasurementData> toGetPower(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetActuralRequestFrequency(const zes_device_handle_t& device, MeasurementType type) noexcept;

        std::shared_ptr<MeasurementData> toGetTemperature(const zes_device_handle_t& device, MeasurementType type) noexcept;

        std::shared_ptr<MeasurementData> toGetMemoryUsedUtilization(const zes_device_handle_t& device, MeasurementType type) noexcept;

        std::shared_ptr<MeasurementData> toGetMemoryBandwidth(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetMemoryReadWrite(const zes_device_handle_t& device, MeasurementType type) noexcept;

        std::shared_ptr<MeasurementData> toGetEngineUtilization(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetGPUUtilization(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetEngineGroupUtilization(const zes_device_handle_t& device, MeasurementType type) noexcept;

        std::shared_ptr<MeasurementData> toGetEnergy(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetEuActiveStallIdle(const zes_device_handle_t& device, const ze_driver_handle_t& driver, MeasurementType type) noexcept;

        std::shared_ptr<MeasurementData> toGetRasError(const zes_device_handle_t& device, MeasurementType type) noexcept;

        std::shared_ptr<MeasurementData> toGetFrequencyThrottle(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetFrequencyThrottleReason(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetPCIeReadThroughput(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetPCIeWriteThroughput(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetPCIeRead(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetPCIeWrite(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetFabricThroughput(const zes_device_handle_t& device) noexcept;

        std::shared_ptr<MeasurementData> toGetPerfMetrics(const zes_device_handle_t& device, const ze_driver_handle_t& driver) noexcept;

    private:
        GPUDeviceStub();

        ~GPUDeviceStub();

        GPUDeviceStub& operator=(const GPUDeviceStub&) = delete;

        GPUDeviceStub(const GPUDeviceStub& other) = delete;

        void init();

        static std::string to_hex_string(uint32_t val);

        static std::string buildErrors(const std::map<std::string, ze_result_t>& exception_msgs, const char* func, uint32_t line);

        static std::string to_string(zes_pci_address_t address);

        bool initialized;

        std::mutex mutex;
    };

} // end namespace xpum