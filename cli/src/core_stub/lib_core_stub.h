/* 
 *  Copyright (C) 2021-2023 Intel Corporation
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

namespace xpum::cli {

class LibCoreStub : public CoreStub {
   public:
    LibCoreStub(bool initCore = true);
    ~LibCoreStub();

    bool initCore;

    bool isChannelReady();

    std::unique_ptr<nlohmann::json> getVersion();

    std::unique_ptr<nlohmann::json> getDeviceList();

    std::unique_ptr<nlohmann::json> getDeviceProperties(int deviceId, std::string username="", std::string password="");

    std::unique_ptr<nlohmann::json> getDeviceProperties(const char *bdf, std::string username="", std::string password="");

    std::unique_ptr<nlohmann::json> getSerailNumberAndAmcVersion(int deviceId, std::string username="", std::string password="");

    std::unique_ptr<nlohmann::json> getAMCFirmwareVersions(std::string username, std::string password);

    std::unique_ptr<nlohmann::json> getDeivceIdByBDF(const char* bdf, int *deviceId);

    std::unique_ptr<nlohmann::json> getTopology(int deviceId);

    std::unique_ptr<nlohmann::json> groupCreate(std::string groupName);
    std::unique_ptr<nlohmann::json> groupDelete(int groupId);
    std::unique_ptr<nlohmann::json> groupListAll();
    std::unique_ptr<nlohmann::json> groupList(int groupId);
    std::unique_ptr<nlohmann::json> groupAddDevice(int groupId, int deviceId);
    std::unique_ptr<nlohmann::json> groupRemoveDevice(int groupId, int deviceId);

    std::unique_ptr<nlohmann::json> runDiagnostics(int deviceId, int level, std::vector<int> targetTypes, bool rawComponentTypeStr);
    std::unique_ptr<nlohmann::json> getDiagnosticsResult(int deviceId, bool rawComponentTypeStr);
    std::shared_ptr<nlohmann::json> getDiagnosticsMediaCodecResult(int deviceId, bool rawFpsStr);
    std::shared_ptr<nlohmann::json> getDiagnosticsXeLinkThroughputResult(int deviceId, bool rawFpsStr);
    std::unique_ptr<nlohmann::json> runDiagnosticsByGroup(uint32_t groupId, int level, std::vector<int> targetTypes, bool rawComponentTypeStr);
    std::unique_ptr<nlohmann::json> getDiagnosticsResultByGroup(uint32_t groupId, bool rawComponentTypeStr);
    std::unique_ptr<nlohmann::json> runStress(int deviceId, uint32_t stressTime);
    std::unique_ptr<nlohmann::json> checkStress(int deviceId);

    std::unique_ptr<nlohmann::json> precheck(xpum_precheck_options options, bool rawComponentTypeStr);
    std::unique_ptr<nlohmann::json> getPrecheckErrorTypes();
    
    std::unique_ptr<nlohmann::json> getAllHealth();
    std::unique_ptr<nlohmann::json> getHealth(int deviceId, int componentType);
    std::unique_ptr<nlohmann::json> getHealthByGroup(uint32_t groupId, int componentType);
    std::unique_ptr<nlohmann::json> setHealthConfig(int deviceId, int cfgtype, int threshold);
    std::unique_ptr<nlohmann::json> setHealthConfigByGroup(uint32_t groupId, int cfgtype, int threshold);

    std::unique_ptr<nlohmann::json> getHealth(int deviceId, xpum_health_type_t type);
    nlohmann::json appendHealthThreshold(int deviceId, nlohmann::json json, xpum_health_type_t type,
                                               uint64_t throttleValue, uint64_t shutdownValue);

    std::unique_ptr<nlohmann::json> getStatistics(int deviceId, bool enableFilter = false, bool enableScale = false);
    std::unique_ptr<nlohmann::json> getStatisticsByGroup(uint32_t groupId, bool enableFilter = false, bool enableScale = false);
    std::shared_ptr<nlohmann::json> getEngineStatistics(int deviceId);
    std::shared_ptr<std::map<int, std::map<int, int>>> getEngineCount(int deviceId);
    std::shared_ptr<nlohmann::json> getFabricStatistics(int deviceId);
    std::unique_ptr<nlohmann::json> getXelinkThroughputAndUtilMatrix();
    //config related interface
    std::unique_ptr<nlohmann::json> getDeviceConfig(int deviceId, int tileId);
    std::unique_ptr<nlohmann::json> setDeviceSchedulerMode(int deviceId, int tileId, int mode, int val1, int val2);
    std::unique_ptr<nlohmann::json> setDevicePowerlimit(int deviceId, int tileId, int power, int interval);
    std::unique_ptr<nlohmann::json> setDeviceStandby(int deviceId, int tileId, int mode);
    std::unique_ptr<nlohmann::json> setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq);
    std::unique_ptr<nlohmann::json> getDeviceProcessState(int deviceId);
    std::unique_ptr<nlohmann::json> getDeviceComponentOccupancyRatio(int deviceId, int tileId, int samplingInterval);
    std::unique_ptr<nlohmann::json> getDeviceUtilizationByProcess(int deviceId, int utilizationInterval);
    std::unique_ptr<nlohmann::json> getAllDeviceUtilizationByProcess(int utilizationInterval);
    std::unique_ptr<nlohmann::json> getPerformanceFactor(int deviceId, int tileId);
    std::unique_ptr<nlohmann::json> setPerformanceFactor(int deviceId, int tileId, xpum_engine_type_flags_t engine, double factor);
    std::unique_ptr<nlohmann::json> setFabricPortEnabled(int deviceId, int tileId, uint32_t port, uint32_t enabled);
    std::unique_ptr<nlohmann::json> setFabricPortBeaconing(int deviceId, int tileId, uint32_t port, uint32_t beaconing);
    std::unique_ptr<nlohmann::json> setMemoryEccState(int deviceId, bool enabled);
    std::unique_ptr<nlohmann::json> resetDevice(int deviceId, bool force);
    std::unique_ptr<nlohmann::json> applyPPR(int deviceId, bool force);

    std::unique_ptr<nlohmann::json> getAllPolicyType();
    std::unique_ptr<nlohmann::json> getAllPolicyConditionType();
    std::unique_ptr<nlohmann::json> getAllPolicyActionType();
    std::unique_ptr<nlohmann::json> getAllPolicy();
    std::unique_ptr<nlohmann::json> getPolicyById(bool isDevice, uint32_t id);
    std::unique_ptr<nlohmann::json> getPolicy(bool isDevcie, uint32_t id);
    std::unique_ptr<nlohmann::json> setPolicy(bool isDevcie, uint32_t id, PolicyData& policy);

    std::string getRedfishAmcWarnMsg();
    std::unique_ptr<nlohmann::json> runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath, std::string username, std::string password, bool force=false);
    std::unique_ptr<nlohmann::json> getFirmwareFlashResult(int deviceId, unsigned int type);

    std::unique_ptr<nlohmann::json> startDumpRawDataTask(uint32_t deviceId, int tileId, std::vector<xpum_dump_type_t> metricsTypeList);
    std::unique_ptr<nlohmann::json> stopDumpRawDataTask(int taskId);
    std::unique_ptr<nlohmann::json> listDumpRawDataTasks();

    std::unique_ptr<nlohmann::json> setAgentConfig(std::string key, void* pValue);

    std::unique_ptr<nlohmann::json> getAgentConfig();

    std::string getTopoXMLBuffer();

    std::unique_ptr<nlohmann::json> getXelinkTopology();

    std::shared_ptr<nlohmann::json> getFabricCount(int deviceId);

    std::unique_ptr<nlohmann::json> getSensorReading();

    std::vector<std::unique_ptr<nlohmann::json>> getMetricsFromSysfs(std::vector<std::string> bdfs);

    std::unique_ptr<nlohmann::json> genDebugLog(const std::string &fileName);
    std::string getPciSlotName(std::vector<std::string> &bdfs);

    std::unique_ptr<nlohmann::json> doVgpuPrecheck();

    std::unique_ptr<nlohmann::json> createVf(int deviceId, uint32_t numVfs, uint64_t lmem);

    std::unique_ptr<nlohmann::json> getDeviceFunction(int deviceId);

    std::unique_ptr<nlohmann::json> removeAllVf(int deviceId);

};
} // end namespace xpum::cli
