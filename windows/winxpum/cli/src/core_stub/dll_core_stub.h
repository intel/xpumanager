/*
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file lib_core_stub.h
 */

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "xpum_structs.h"
#include "core_stub.h"
#include "exit_code.h"

namespace xpum::cli {

    class DllCoreStub : public CoreStub {
    public:
        DllCoreStub(bool initCore = true);
        ~DllCoreStub();

        bool initCore;

        std::unique_ptr<nlohmann::json> getVersion() override;

        std::unique_ptr<nlohmann::json> getDeviceList() override;

        std::unique_ptr<nlohmann::json> getDeviceProperties(int deviceId, std::string username = "", std::string password = "") override;

        std::unique_ptr<nlohmann::json> getDeviceProperties(const char* bdf, std::string username = "", std::string password = "") override;

        std::unique_ptr<nlohmann::json> getDeivceIdByBDF(const char* bdf, int* deviceId) override;

        std::string getSerailNumberIPMI(int deviceId) override;

        std::unique_ptr<nlohmann::json> getAMCFirmwareVersions(std::string username, std::string password) override;

        std::string getRedfishAmcWarnMsg() override;

        std::unique_ptr<nlohmann::json> runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath, bool force) override;

        std::unique_ptr<nlohmann::json> getFirmwareFlashResult(int deviceId, unsigned int type) override;

        std::vector<int> getSiblingDevices(int deviceId) override;

        std::unique_ptr<nlohmann::json> getRealtimeMetrics(int deviceId, bool enableScale = false) override;

        std::unique_ptr<nlohmann::json> getDeviceConfig(int deviceId, int tileId) override;

        std::unique_ptr<nlohmann::json> setDevicePowerlimit(int deviceId, int tileId, int powerLimit) override;

        std::unique_ptr<nlohmann::json> setDevicePowerlimitExt(int deviceId, int tileId,
                                                               const xpum_power_limit_ext_t& power_limit_ext);

	std::unique_ptr<nlohmann::json> setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq) override;

        std::unique_ptr<nlohmann::json> setMemoryEccState(int deviceId, bool enabled) override;

        std::string eccStateToString(uint8_t state);

        std::string eccStateToString(xpum_ecc_state_t state);
    };
} // end namespace xpum::cli
