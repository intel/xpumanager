/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.h
 */

#pragma once
#pragma warning(disable:4996)

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <chrono>
#include <map>
#include <set>
#include <vector>
#include <future>
#include <unordered_map>
#include <unordered_set>
#include <ze_api.h>
#include <zes_api.h>
#include "xpum_structs.h"
#include "igsc_manager.h"
using namespace xpum;

class CoreStub {
public:
    CoreStub();

    ~CoreStub();

    std::unique_ptr<nlohmann::json> getVersion();

    std::unique_ptr<nlohmann::json> getDeviceList();

    std::unique_ptr<nlohmann::json> getDeviceProperties(int deviceId);

    std::unique_ptr<nlohmann::json> getDeviceConfig(int deviceId, int tileId);

    std::unique_ptr<nlohmann::json> setDevicePowerlimit(int deviceId, int tileId, int power, int interval);

    std::unique_ptr<nlohmann::json> setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq);

    std::unique_ptr<nlohmann::json> getStatistics(int deviceId, bool enableFilter = false);

    std::unique_ptr<nlohmann::json> setMemoryEccState(int deviceId, bool enabled);

    std::unique_ptr<nlohmann::json> runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath);
    
    std::unique_ptr<nlohmann::json> getFirmwareFlashResult(int deviceId, unsigned int type);

    std::vector<int> getSiblingDevices(int deviceId);

    static std::string isotimestamp(uint64_t t, bool withoutDate = false);

private:

    std::string getUUID(uint8_t(&uuidBuf)[16]);

    std::string getBdfAddress(const zes_device_handle_t& zes_device);

    static long long getCurrentMillisecond();

    std::vector<std::string> handleFreqByLevel0(zes_device_handle_t device, bool set, int minFreq, int maxFreq, bool& supported);

    std::vector<int> handlePowerByLevel0(zes_device_handle_t device, bool set, int limit, int interval, bool& supported);

    std::string metricsTypeToString(xpum_stats_type_t metricsType);

    xpum_device_stats_data_t getMetricsByLevel0(zes_device_handle_t device, xpum_stats_type_t metricsType);

    ze_driver_handle_t ze_driver_handle;

    std::string driver_version;

    std::vector<ze_device_handle_t>  ze_device_handles;

    std::vector<zes_device_handle_t> zes_device_handles;

    int memory_sampling_interval = 100;

    int measurement_data_scale = 100;

    int engine_sampling_interval = 100;

    int power_limit = 300;

    IGSC_Manager igsc_instance;

    std::unordered_map<int, std::set<int>> sibling_devices;

    std::vector<std::future<xpum_firmware_flash_result_t>> flash_results;
};

