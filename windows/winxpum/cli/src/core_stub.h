/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.h
 */

#pragma once
#pragma warning(disable : 4996)

#include <memory>
#include <nlohmann/json.hpp>

#include "xpum_structs.h"

namespace xpum::cli {
	
	class CoreStub {
    public:
        virtual std::unique_ptr<nlohmann::json> getVersion() = 0;

        virtual std::unique_ptr<nlohmann::json> getDeviceList() = 0;

        virtual std::unique_ptr<nlohmann::json> getDeviceProperties(int deviceId, std::string username = "", std::string password = "") = 0;

        virtual std::unique_ptr<nlohmann::json> getDeviceProperties(const char* bdf, std::string username = "", std::string password = "") = 0;
        
        virtual std::unique_ptr<nlohmann::json> getDeivceIdByBDF(const char* bdf, int* deviceId) = 0;

        virtual std::string getSerailNumberIPMI(int deviceId) = 0;

        virtual std::unique_ptr<nlohmann::json> getAMCFirmwareVersions(std::string username, std::string password) = 0;

        virtual std::string getRedfishAmcWarnMsg() = 0;

        virtual std::unique_ptr<nlohmann::json> runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath, bool force) = 0;

        virtual std::unique_ptr<nlohmann::json> getFirmwareFlashResult(int deviceId, unsigned int type) = 0;

        virtual std::vector<int> getSiblingDevices(int deviceId) = 0;

        virtual std::unique_ptr<nlohmann::json> getRealtimeMetrics(int deviceId, bool enableScale = false) = 0;

        virtual std::unique_ptr<nlohmann::json> getDeviceConfig(int deviceId, int tileId) = 0;

        virtual std::unique_ptr<nlohmann::json> setDevicePowerlimit(int deviceId, int tileId, int powerLimit) = 0;

	virtual std::unique_ptr<nlohmann::json> setDevicePowerlimitExt(int deviceId, int tileId, const xpum_power_limit_ext_t& plimit_ext) = 0;

	virtual std::unique_ptr<nlohmann::json> setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq) = 0;

        virtual std::unique_ptr<nlohmann::json> setMemoryEccState(int deviceId, bool enabled) = 0;

        static std::string isotimestamp(uint64_t t, bool withoutDate = false);

        std::string metricsTypeToString(xpum_stats_type_t metricsType);
    };

}
