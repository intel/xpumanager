/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device.h
 */

#pragma once

#include <string>
#include <mutex>
#include <vector>

#include "level_zero/ze_api.h"
#include "level_zero/zes_api.h"
#include "level_zero/zet_api.h"

#include "infrastructure/measurement_data.h"
#include "infrastructure/measurement_type.h"
#include "infrastructure/device_capability.h"
#include "infrastructure/property.h"
#include "device/memoryEcc.h"

namespace xpum {
    class Device {
    public:
        Device();

        std::string getId() noexcept;

        void getCapability(std::vector<DeviceCapability>& capabilites) noexcept;

        bool hasCapability(DeviceCapability& cap) noexcept;

        void getProperties(std::vector<Property>& properties) noexcept;

        bool getProperty(xpum_device_internal_property_name_t name, Property& ret) noexcept;

        void addProperty(Property prop);

        virtual void getPower(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getActuralRequestFrequency(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept = 0;

        virtual void getTemperature(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept = 0;

        virtual void getMemoryUsedUtilization(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept = 0;

        virtual void getMemoryBandwidth(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getMemoryReadWrite(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept = 0;

        virtual void getEngineUtilization(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getGPUUtilization(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getEngineGroupUtilization(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept = 0;

        virtual void getEnergy(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getEuActiveStallIdle(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept = 0;

        virtual void getRasError(std::shared_ptr<MeasurementData>& data, MeasurementType type) noexcept = 0;

        virtual void getFrequencyThrottle(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getFrequencyThrottleReason(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getPCIeReadThroughput(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getPCIeWriteThroughput(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getPCIeRead(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getPCIeWrite(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getFabricThroughput(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getPerfMetrics(std::shared_ptr<MeasurementData>& data) noexcept = 0;

        virtual void getDeviceSusPower(int32_t& Sus_power_t, bool& Sus_supported_t) noexcept = 0;

        virtual bool setDevicePowerSustainedLimits(int powerLimit) = 0;

        virtual void getDevicePowerMaxLimit(int32_t& max_limit, bool& supported) = 0;

        virtual void getSimpleEccState(uint8_t& current, uint8_t& pending) noexcept = 0;

        virtual bool getEccState(MemoryEcc& ecc) noexcept = 0;

        virtual void getDeviceFrequencyRange(int32_t tileId, double& min, double& max, std::string& clocks, bool& supported) noexcept = 0;

        virtual bool setDeviceFrequencyRange(int32_t tileId, double min, double max) noexcept = 0;

        virtual std::map<MeasurementType, std::shared_ptr<MeasurementData>> getRealtimeMetrics() noexcept = 0;

        virtual void getFreqAvailableClocks(int32_t tileId, std::vector<double>& clocksList) noexcept = 0;
        virtual ~Device() {}

        int getDeviceModel();

    protected:
        std::string id;
        std::mutex mutex;
        std::vector<DeviceCapability> capabilities;
        std::vector<Property> properties;
    };
}