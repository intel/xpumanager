#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletHealthOptions {
    bool listAll = false;
    int deviceId = -1;
    int groupId = -1;
    int powerThreshold = -2;
    int thermalThreshold = -2;
};

class ComletHealth : public ComletBase {
   public:
    ComletHealth() : ComletBase("health", "Health of the device") {}
    virtual ~ComletHealth() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

   private:
    std::unique_ptr<ComletHealthOptions> opts;
};
} // end namespace xpum::cli
