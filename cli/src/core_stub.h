#pragma once

#include "core.grpc.pb.h"
#include <nlohmann/json.hpp>

class CoreStub {
  public:
    CoreStub();

    std::unique_ptr<nlohmann::json> getVersion();

  private:
    std::unique_ptr<XpumCoreService::Stub> stub;
};