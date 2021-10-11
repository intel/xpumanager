#pragma once

#include "core.grpc.pb.h"
#include <nlohmann/json.hpp>

class CoreStub {
  public:
    CoreStub();

    std::unique_ptr<nlohmann::json> getVersion();

    std::unique_ptr<nlohmann::json> getDeviceList();

    std::unique_ptr<nlohmann::json> getTopology(DeviceId deviceId);

  private:
    std::unique_ptr<XpumCoreService::Stub> stub;
};