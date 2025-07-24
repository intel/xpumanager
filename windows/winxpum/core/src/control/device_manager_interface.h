/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_manager_interface.h
 */

#pragma once

#include <vector>

#include "device/device.h"
#include "device/memoryEcc.h"
#include "infrastructure/init_close_interface.h"

namespace xpum {

    /*
	    DeviceManagerInterface class defines various interfaces for managing devices.
    */
    class DeviceManagerInterface : public InitCloseInterface {
    public:
        virtual ~DeviceManagerInterface() {}

        virtual void getDeviceList(std::vector<std::shared_ptr<Device>>& devices) = 0;

        virtual std::shared_ptr<Device> getDevicebyBDF(const std::string& bdf) = 0;

        virtual std::shared_ptr<Device> getDevice(const std::string& id) = 0;

        virtual void getDeviceSusPower(const std::string& id, int32_t& Sus_power, bool& Sus_supported) = 0;

        virtual void getDevicePowerMaxLimit(const std::string& id, int32_t& max_limit, bool& supported) = 0;

        virtual bool setDevicePowerSustainedLimits(const std::string& id, int powerLimit) = 0;

        virtual void getSimpleEccState(const std::string& id, uint8_t& current, uint8_t& pending) = 0;

        virtual bool getEccState(const std::string& id, MemoryEcc& ecc) = 0;

        virtual void getDeviceFrequencyRange(const std::string& id, int32_t tileId, double& min, double& max, std::string& clocks, bool& supported) = 0;

        virtual bool setDeviceFrequencyRange(const std::string& id, int32_t tileId, double min, double max) = 0;

        virtual void getFreqAvailableClocks(const std::string& id, int32_t tileId, std::vector<double>& clocksList) = 0;
    };
}