#pragma once

#include <memory>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletTopologyOptions {
    int deviceId = -1;
    std::string xmlFile = "";
    bool xeLink = false;
};

class ComletTopology : public ComletBase {
   public:
    ComletTopology() : ComletBase("topology", "Get the GPU to CPU and GPU to PCIe switch topology info.") {
        printHelpWhenNoArgs = true;
    }
    virtual ~ComletTopology() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;
    virtual void getTableResult(std::ostream &out) override;

    inline const bool isDeviceOperation() const {
        return opts->deviceId >= 0;
    }

   private:
    std::unique_ptr<ComletTopologyOptions> opts;
};
} // end namespace xpum::cli
