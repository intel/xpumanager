#pragma once

#include "comlet_base.h"

struct ComletTopologyOptions {
    int deviceId = -1;
};

class ComletTopology : public ComletBase {

  public:
    ComletTopology() : ComletBase("topology", "Topology of the device") {}
    virtual ~ComletTopology() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

  private:
    std::unique_ptr<ComletTopologyOptions> opts;
};