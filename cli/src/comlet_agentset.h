#pragma once

#include "comlet_base.h"

namespace xpum::cli {

struct ComletAgentSetOptions {
    bool list;
    int samplingInterval = -1;
};

class ComletAgentSet : public ComletBase {
   private:
    std::unique_ptr<ComletAgentSetOptions> opts;

   public:
    ComletAgentSet() : ComletBase("agentset", "Get or change some XPU Manager settings.") {
    }

    virtual void setupOptions() override;

    virtual std::unique_ptr<nlohmann::json> run() override;

};
} // namespace xpum::cli