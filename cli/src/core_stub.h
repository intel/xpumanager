/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.h
 */

#pragma once

// #include <grpc++/channel.h>

#include <chrono>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

#include "core.grpc.pb.h"
#include "xpum_structs.h"

namespace xpum::cli {

class CoreStub {
   public:

    virtual bool isChannelReady()=0;

    virtual std::unique_ptr<nlohmann::json> getVersion()=0;

    virtual std::unique_ptr<nlohmann::json> getDeviceList()=0;

    virtual std::unique_ptr<nlohmann::json> getDeviceProperties(int deviceId)=0;

    virtual std::unique_ptr<nlohmann::json> getDeviceProperties(const char *bdf)=0;

    virtual std::unique_ptr<nlohmann::json> getAMCFirmwareVersions()=0;
    virtual std::unique_ptr<nlohmann::json> getDeivceIdByBDF(const char* bdf, int *deviceId)=0;
    virtual std::unique_ptr<nlohmann::json> getTopology(int deviceId)=0;

    virtual std::unique_ptr<nlohmann::json> groupCreate(std::string groupName)=0;
    virtual std::unique_ptr<nlohmann::json> groupDelete(int groupId)=0;
    virtual std::unique_ptr<nlohmann::json> groupListAll()=0;
    virtual std::unique_ptr<nlohmann::json> groupList(int groupId)=0;
    virtual std::unique_ptr<nlohmann::json> groupAddDevice(int groupId, int deviceId)=0;
    virtual std::unique_ptr<nlohmann::json> groupRemoveDevice(int groupId, int deviceId)=0;

    virtual std::unique_ptr<nlohmann::json> runDiagnostics(int deviceId, int level, bool rawComponentTypeStr)=0;
    virtual std::unique_ptr<nlohmann::json> getDiagnosticsResult(int deviceId, bool rawComponentTypeStr)=0;
    virtual std::unique_ptr<nlohmann::json> runDiagnostics(const char *bdf, int level, bool rawComponentTypeStr)=0;
    virtual std::unique_ptr<nlohmann::json> getDiagnosticsResult(const char *bdf, bool rawComponentTypeStr)=0;
    virtual std::unique_ptr<nlohmann::json> runDiagnosticsByGroup(uint32_t groupId, int level, bool rawComponentTypeStr)=0;
    virtual std::unique_ptr<nlohmann::json> getDiagnosticsResultByGroup(uint32_t groupId, bool rawComponentTypeStr)=0;

    virtual std::unique_ptr<nlohmann::json> getAllHealth()=0;
    virtual std::unique_ptr<nlohmann::json> getHealth(int deviceId, int componentType)=0;
    virtual std::unique_ptr<nlohmann::json> getHealth(int deviceId, HealthType type)=0;
    virtual std::unique_ptr<nlohmann::json> getHealthByGroup(uint32_t groupId, int componentType)=0;
    virtual std::unique_ptr<nlohmann::json> getHealthByGroup(uint32_t groupId, HealthType type)=0;
    virtual std::unique_ptr<nlohmann::json> setHealthConfig(int deviceId, HealthConfigType cfgtype, int threshold)=0;
    virtual std::unique_ptr<nlohmann::json> setHealthConfigByGroup(uint32_t groupId, HealthConfigType cfgtype, int threshold)=0;
    virtual int getHealthConfig(int deviceId, HealthConfigType cfgtype)=0;
    virtual std::string healthStatusEnumToString(HealthStatusType status)=0;
    virtual std::string healthTypeEnumToString(HealthType type)=0;
    virtual nlohmann::json appendHealthThreshold(int deviceId, nlohmann::json, HealthType type, uint64_t throttleValue, uint64_t shutdownValue)=0;

    virtual std::unique_ptr<nlohmann::json> getStatistics(int deviceId, bool enableFilter = false)=0;
    virtual std::unique_ptr<nlohmann::json> getStatistics(const char *bdf, bool enableFilter = false)=0;
    virtual std::unique_ptr<nlohmann::json> getStatisticsByGroup(uint32_t groupId, bool enableFilter = false)=0;
    virtual std::shared_ptr<nlohmann::json> getEngineStatistics(int deviceId)=0;
    virtual std::shared_ptr<std::map<int, std::map<int, int>>> getEngineCount(int deviceId)=0;
    virtual std::shared_ptr<std::map<int, std::map<int, int>>> getEngineCount(const char *bdf)=0;
    virtual std::shared_ptr<nlohmann::json> getFabricStatistics(int deviceId)=0;
    //config related interface
    virtual std::unique_ptr<nlohmann::json> getDeviceConfig(int deviceId, int tileId)=0;
    virtual std::unique_ptr<nlohmann::json> setDeviceSchedulerMode(int deviceId, int tileId, XpumSchedulerMode mode, int val1, int val2)=0;
    virtual std::unique_ptr<nlohmann::json> setDevicePowerlimit(int deviceId, int tileId, int power, int interval)=0;
    virtual std::unique_ptr<nlohmann::json> setDeviceStandby(int deviceId, int tileId, XpumStandbyMode mode)=0;
    virtual std::unique_ptr<nlohmann::json> setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq)=0;
    virtual std::unique_ptr<nlohmann::json> getDeviceProcessState(int deviceId)=0;
    virtual std::unique_ptr<nlohmann::json> getPerformanceFactor(int deviceId, int tileId)=0;
    virtual std::unique_ptr<nlohmann::json> setPerformanceFactor(int deviceId, int tileId, xpum_engine_type_flags_t engine, double factor)=0;
    virtual std::unique_ptr<nlohmann::json> setFabricPortEnabled(int deviceId, int tileId, uint32_t port, uint32_t enabled)=0;
    virtual std::unique_ptr<nlohmann::json> setFabricPortBeaconing(int deviceId, int tileId, uint32_t port, uint32_t beaconing)=0;
    virtual std::unique_ptr<nlohmann::json> setMemoryEccState(int deviceId, bool enabled)=0;
    virtual std::unique_ptr<nlohmann::json> resetDevice(int deviceId, bool force)=0;
    virtual std::string schedulerModeEnumToString(XpumSchedulerMode mode)=0;
    virtual std::string standbyModeEnumToString(XpumStandbyMode mode)=0;

    virtual std::string policyTypeEnumToString(XpumPolicyType type)=0;
    virtual std::string policyConditionTypeEnumToString(XpumPolicyConditionType type)=0;
    virtual std::string policyActionTypeEnumToString(XpumPolicyActionType type)=0;
    virtual std::unique_ptr<nlohmann::json> getAllPolicyType()=0;
    virtual std::unique_ptr<nlohmann::json> getAllPolicyConditionType()=0;
    virtual std::unique_ptr<nlohmann::json> getAllPolicyActionType()=0;
    virtual std::unique_ptr<nlohmann::json> getAllPolicy()=0;
    virtual std::unique_ptr<nlohmann::json> getPolicyById(bool isDevice, int id)=0;
    virtual std::unique_ptr<nlohmann::json> getPolicy(bool isDevcie, int id)=0;
    virtual std::unique_ptr<nlohmann::json> setPolicy(bool isDevcie, int id, XpumPolicyData& policy)=0;
    virtual bool isCliSupportedPolicyType(XpumPolicyType type)=0;

    virtual std::unique_ptr<nlohmann::json> runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath)=0;
    virtual std::unique_ptr<nlohmann::json> getFirmwareFlashResult(int deviceId, unsigned int type)=0;

    virtual std::unique_ptr<nlohmann::json> startDumpRawDataTask(uint32_t deviceId, int tileId, std::vector<xpum_dump_type_t> metricsTypeList)=0;
    virtual std::unique_ptr<nlohmann::json> stopDumpRawDataTask(int taskId)=0;
    virtual std::unique_ptr<nlohmann::json> listDumpRawDataTasks()=0;


    virtual std::unique_ptr<nlohmann::json> setAgentConfig(std::string key, void* pValue)=0;

    virtual std::unique_ptr<nlohmann::json> getAgentConfig()=0;

    virtual std::string getTopoXMLBuffer()=0;

    virtual std::unique_ptr<nlohmann::json> getXelinkTopology()=0;

    virtual std::shared_ptr<nlohmann::json> getFabricCount(int deviceId)=0;

    virtual std::shared_ptr<nlohmann::json> getFabricCount(const char *bdf)=0;

    static std::string isotimestamp(uint64_t t);

    static std::string metricsTypeToString(xpum_stats_type_t metricsType);

   private:
    // std::unique_ptr<XpumCoreService::Stub> stub;

    // std::shared_ptr<grpc::Channel> channel;

    std::string getCardUUID(const std::string& rawUUID);
};
} // end namespace xpum::cli
