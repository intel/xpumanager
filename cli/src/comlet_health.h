#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <climits>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletHealthOptions {
    bool listAll = false;
    int deviceId = INT_MIN;
    uint32_t groupId = UINT_MAX;
    int componentType = INT_MIN;
    int threshold = INT_MIN;
};

class ComletHealth : public ComletBase {
   public:
    ComletHealth() : ComletBase("health", "Get the GPU device component health status") {}
    virtual ~ComletHealth() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

   private:
    std::unique_ptr<ComletHealthOptions> opts;
};
} // end namespace xpum::cli
