#pragma once

#include "core.grpc.pb.h"
#include <nlohmann/json.hpp>

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

  private:
    std::unique_ptr<XpumCoreService::Stub> stub;
};