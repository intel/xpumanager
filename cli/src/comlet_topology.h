#pragma once

#include <memory>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletTopologyOptions {
    int deviceId = -1;
};

class ComletTopology : public ComletBase {
   public:
    ComletTopology() : ComletBase("topology", "Help info of get GPU to CPU and GPU to PCIe switch topology") {}
    virtual ~ComletTopology() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

   private:
    std::unique_ptr<ComletTopologyOptions> opts;
};
} // end namespace xpum::cli
