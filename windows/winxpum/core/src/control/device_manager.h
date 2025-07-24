/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_manager.h
 */

#pragma once

#include "device_manager_interface.h"

#include <memory>

namespace xpum {

    /*
      DeviceManager class provides various interfaces for managing all devices.
    */

    class DeviceManager : public DeviceManagerInterface,
        public std::enable_shared_from_this<DeviceManager> {
    public:
        DeviceManager();

        ~DeviceManager();

        void init() override;

        void close() override;

        void getDeviceList(std::vector<std::shared_ptr<Device>>& devices) override;

        std::shared_ptr<Device> getDevicebyBDF(const std::string& bdf) override;

        std::shared_ptr<Device> getDevice(const std::string& id) override;

        void getDeviceSusPower(const std::string& id, int32_t& Sus_power, bool& Sus_supported) override;

        bool setDevicePowerSustainedLimits(const std::string& id, int powerLimit) override;

        void getDevicePowerMaxLimit(const std::string& id, int32_t& max_limit, bool& supported) override;

        void getSimpleEccState(const std::string& id, uint8_t& current, uint8_t& pending) override;

        bool getEccState(const std::string& id, MemoryEcc& ecc) override;

        void getDeviceFrequencyRange(const std::string& id, int32_t tileId, double& min, double& max, std::string& clocks, bool& supported) override;

        bool setDeviceFrequencyRange(const std::string& id, int32_t tileId, double min, double max) override;

        void getFreqAvailableClocks(const std::string& id, int32_t tileId, std::vector<double>& clocksList) override;
    private:
        std::vector<std::shared_ptr<Device>> devices;

        std::mutex mutex;
    };

} // end namespace xpum
