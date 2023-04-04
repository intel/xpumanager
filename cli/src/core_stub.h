/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.h
 */

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <cstdint>

#include "xpum_structs.h"

namespace xpum::cli {

struct PolicyData {
    xpum_policy_type_t type;
    xpum_policy_condition_t condition;
    xpum_policy_action_t action;
    std::string notifyCallBackUrl;
    uint32_t deviceId;
    bool isDeletePolicy;
};

class CoreStub {
   public:

    virtual bool isChannelReady()=0;

    virtual std::unique_ptr<nlohmann::json> getVersion()=0;

    virtual std::unique_ptr<nlohmann::json> getDeviceList()=0;

    virtual std::unique_ptr<nlohmann::json> getDeviceProperties(int deviceId, std::string username="", std::string password="")=0;

    virtual std::unique_ptr<nlohmann::json> getDeviceProperties(const char *bdf, std::string username="", std::string password="")=0;

    virtual std::string getSerailNumberIPMI(int deviceId)=0;

    virtual std::unique_ptr<nlohmann::json> getAMCFirmwareVersions(std::string username, std::string password)=0;
    virtual std::unique_ptr<nlohmann::json> getDeivceIdByBDF(const char* bdf, int *deviceId)=0;

    virtual std::unique_ptr<nlohmann::json> getTopology(int deviceId)=0;

    virtual std::unique_ptr<nlohmann::json> groupCreate(std::string groupName)=0;
    virtual std::unique_ptr<nlohmann::json> groupDelete(int groupId)=0;
    virtual std::unique_ptr<nlohmann::json> groupListAll()=0;
    virtual std::unique_ptr<nlohmann::json> groupList(int groupId)=0;
    virtual std::unique_ptr<nlohmann::json> groupAddDevice(int groupId, int deviceId)=0;
    virtual std::unique_ptr<nlohmann::json> groupRemoveDevice(int groupId, int deviceId)=0;

    virtual std::unique_ptr<nlohmann::json> runDiagnostics(int deviceId, int level, std::vector<int> targetTypes, bool rawComponentTypeStr)=0;
    virtual std::unique_ptr<nlohmann::json> getDiagnosticsResult(int deviceId, bool rawComponentTypeStr)=0;
    virtual std::shared_ptr<nlohmann::json> getDiagnosticsMediaCodecResult(int deviceId, bool rawFpsStr)=0;
    virtual std::unique_ptr<nlohmann::json> runDiagnosticsByGroup(uint32_t groupId, int level, std::vector<int> targetTypes, bool rawComponentTypeStr)=0;
    virtual std::unique_ptr<nlohmann::json> getDiagnosticsResultByGroup(uint32_t groupId, bool rawComponentTypeStr)=0;
    virtual std::unique_ptr<nlohmann::json> runStress(int deviceId, uint32_t stressTime)=0;
    virtual std::unique_ptr<nlohmann::json> checkStress(int deviceId)=0;


    virtual std::unique_ptr<nlohmann::json> getAllHealth()=0;
    virtual std::unique_ptr<nlohmann::json> getHealth(int deviceId, int componentType)=0;
    virtual std::unique_ptr<nlohmann::json> getHealthByGroup(uint32_t groupId, int componentType)=0;
    virtual std::unique_ptr<nlohmann::json> setHealthConfig(int deviceId, int cfgtype, int threshold)=0;
    virtual std::unique_ptr<nlohmann::json> setHealthConfigByGroup(uint32_t groupId, int cfgtype, int threshold)=0;

    virtual std::unique_ptr<nlohmann::json> getStatistics(int deviceId, bool enableFilter = false, bool enableScale = false)=0;
    virtual std::unique_ptr<nlohmann::json> getStatisticsByGroup(uint32_t groupId, bool enableFilter = false, bool enableScale = false)=0;
    virtual std::shared_ptr<nlohmann::json> getEngineStatistics(int deviceId)=0;
    virtual std::shared_ptr<std::map<int, std::map<int, int>>> getEngineCount(int deviceId)=0;
    virtual std::shared_ptr<nlohmann::json> getFabricStatistics(int deviceId)=0;
    //config related interface
    virtual std::unique_ptr<nlohmann::json> getDeviceConfig(int deviceId, int tileId)=0;
    virtual std::unique_ptr<nlohmann::json> setDeviceSchedulerMode(int deviceId, int tileId, int mode, int val1, int val2)=0;
    virtual std::unique_ptr<nlohmann::json> setDevicePowerlimit(int deviceId, int tileId, int power, int interval)=0;
    virtual std::unique_ptr<nlohmann::json> setDeviceStandby(int deviceId, int tileId, int mode)=0;
    virtual std::unique_ptr<nlohmann::json> setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq)=0;
    virtual std::unique_ptr<nlohmann::json> getDeviceProcessState(int deviceId)=0;
    virtual std::unique_ptr<nlohmann::json> getDeviceComponentOccupancyRatio(int deviceId, int tileId, int samplingInterval)=0;
    virtual std::unique_ptr<nlohmann::json> getDeviceUtilizationByProcess(int deviceId, int utilizationInterval)=0;
    virtual std::unique_ptr<nlohmann::json> getAllDeviceUtilizationByProcess(int utilizationInterval)=0;
    virtual std::unique_ptr<nlohmann::json> getPerformanceFactor(int deviceId, int tileId)=0;
    virtual std::unique_ptr<nlohmann::json> setPerformanceFactor(int deviceId, int tileId, xpum_engine_type_flags_t engine, double factor)=0;
    virtual std::unique_ptr<nlohmann::json> setFabricPortEnabled(int deviceId, int tileId, uint32_t port, uint32_t enabled)=0;
    virtual std::unique_ptr<nlohmann::json> setFabricPortBeaconing(int deviceId, int tileId, uint32_t port, uint32_t beaconing)=0;
    virtual std::unique_ptr<nlohmann::json> setMemoryEccState(int deviceId, bool enabled)=0;
    virtual std::unique_ptr<nlohmann::json> resetDevice(int deviceId, bool force)=0;
    std::string schedulerModeToString(int mode);
    std::string standbyModeToString(int mode);
    std::string deviceFunctionTypeEnumToString(xpum_device_function_type_t type);

    virtual std::unique_ptr<nlohmann::json> getAllPolicyType()=0;
    virtual std::unique_ptr<nlohmann::json> getAllPolicyConditionType()=0;
    virtual std::unique_ptr<nlohmann::json> getAllPolicyActionType()=0;
    virtual std::unique_ptr<nlohmann::json> getAllPolicy()=0;
    virtual std::unique_ptr<nlohmann::json> getPolicyById(bool isDevice, uint32_t id)=0;
    virtual std::unique_ptr<nlohmann::json> getPolicy(bool isDevcie, uint32_t id)=0;
    virtual std::unique_ptr<nlohmann::json> setPolicy(bool isDevcie, uint32_t id, PolicyData& policy)=0;

    virtual std::string getRedfishAmcWarnMsg()=0;
    virtual std::unique_ptr<nlohmann::json> runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath, std::string username, std::string password, bool force=false)=0;
    virtual std::unique_ptr<nlohmann::json> getFirmwareFlashResult(int deviceId, unsigned int type)=0;

    virtual std::unique_ptr<nlohmann::json> startDumpRawDataTask(uint32_t deviceId, int tileId, std::vector<xpum_dump_type_t> metricsTypeList)=0;
    virtual std::unique_ptr<nlohmann::json> stopDumpRawDataTask(int taskId)=0;
    virtual std::unique_ptr<nlohmann::json> listDumpRawDataTasks()=0;
    virtual std::unique_ptr<nlohmann::json> genDebugLog(const std::string &fileName) = 0;

    static std::string isotimestamp(uint64_t t, bool withoutDate = false);

    static std::string metricsTypeToString(xpum_stats_type_t metricsType);

    virtual std::unique_ptr<nlohmann::json> setAgentConfig(std::string key, void* pValue)=0;

    virtual std::unique_ptr<nlohmann::json> getAgentConfig()=0;

    virtual std::string getTopoXMLBuffer()=0;

    virtual std::unique_ptr<nlohmann::json> getXelinkTopology()=0;

    virtual std::shared_ptr<nlohmann::json> getFabricCount(int deviceId)=0;

    virtual std::unique_ptr<nlohmann::json> getSensorReading()=0;

    virtual std::vector<std::unique_ptr<nlohmann::json>> getMetricsFromSysfs(std::vector<std::string> bdfs)=0;

    virtual std::string getPciSlotName(std::vector<std::string> &bdfs)=0;

    virtual std::unique_ptr<nlohmann::json> doVgpuPrecheck()=0;

   protected:
    std::string getCardUUID(const std::string& rawUUID);
};
} // end namespace xpum::cli
