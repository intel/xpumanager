#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletResetOptions {
    int deviceId = -1;
};

class ComletReset : public ComletBase {
   public:
    ComletReset() : ComletBase("reset", "reset the device") {}
    virtual ~ComletReset() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

   private:
    std::unique_ptr<ComletResetOptions> opts;
};
} // end namespace xpum::cli
