#pragma once

#include "core.grpc.pb.h"

#include <memory>
#include <thread>
#include <nlohmann/json.hpp>
#include <string>

namespace xpum::cli {

class CoreStub {
  public:
    CoreStub();

    std::unique_ptr<nlohmann::json> getVersion();

    std::unique_ptr<nlohmann::json> getDeviceList();

    std::unique_ptr<nlohmann::json> getDeviceProperties(int deviceId);

    std::unique_ptr<nlohmann::json> getTopology(int deviceId);

    std::unique_ptr<nlohmann::json> groupCreate(std::string groupName);
    std::unique_ptr<nlohmann::json> groupDelete(int groupId);
    std::unique_ptr<nlohmann::json> groupListAll();
    std::unique_ptr<nlohmann::json> groupList(int groupId);
    std::unique_ptr<nlohmann::json> groupAddDevice(int groupId, int deviceId);
    std::unique_ptr<nlohmann::json> groupRemoveDevice(int groupId, int deviceId);

    std::unique_ptr<nlohmann::json> runDiagnostics(int deviceId, int level);
    std::unique_ptr<nlohmann::json> getDiagnosticsResult(int deviceId);
    std::unique_ptr<nlohmann::json> runDiagnosticsByGroup(int groupId, int level);
    std::unique_ptr<nlohmann::json> getDiagnosticsResultByGroup(int groupId);
    std::string diagnosticResultEnumToString(DiagnosticsComponentInfo_Result result);
    std::string diagnosticTypeEnumToString(DiagnosticsComponentInfo_Type type);

    std::unique_ptr<nlohmann::json> getAllHealth();
    std::unique_ptr<nlohmann::json> getHealth(int deviceId);
    std::unique_ptr<nlohmann::json> getHealth(int deviceId, HealthType type);
    std::unique_ptr<nlohmann::json> getHealthByGroup(int groupId);
    std::unique_ptr<nlohmann::json> getHealthByGroup(int groupId, HealthType type);
    std::unique_ptr<nlohmann::json> setHealthConfig(int deviceId, HealthConfigType cfgtype, int threshold);
    std::unique_ptr<nlohmann::json> setHealthConfigByGroup(int groupId, HealthConfigType cfgtype, int threshold);
    int getHealthConfig(int deviceId, HealthConfigType cfgtype);
    std::string healthStatusEnumToString(HealthStatusType status);
    std::string healthTypeEnumToString(HealthType type);

    std::unique_ptr<nlohmann::json> getStatistics(int deviceId);
    std::unique_ptr<nlohmann::json> getStatisticsByGroup(int groupId);

    std::string policyTypeEnumToString(XpumPolicyType type);
    std::string policyConditionTypeEnumToString(XpumPolicyConditionType type);
    std::string policyActionTypeEnumToString(XpumPolicyActionType type);
    std::unique_ptr<nlohmann::json> getPolicy(int deviceId);

  private:
    std::unique_ptr<XpumCoreService::Stub> stub;
    static std::string isotimestamp(uint64_t t);
};
} // end namespace xpum::cli
