#pragma once

#include "comlet_base.h"

#include <nlohmann/json.hpp>
#include <string>

namespace xpum::cli {

struct ComletPolicyOptions {
    bool listAll = false;
    int deviceId = -1;
    int groupId = -1;
    int powerThreshold = -2;
    int thermalThreshold = -2;
};

class ComletPolicy : public ComletBase {

  public:
    ComletPolicy() : ComletBase("policy", "Policy of the device") {}
    virtual ~ComletPolicy() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

  private:
    std::unique_ptr<ComletPolicyOptions> opts;
};
} // end namespace xpum::cli
