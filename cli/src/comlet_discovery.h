#pragma once

#include "comlet_base.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

struct ComletDiscoveryOptions {
    int deviceId = -1;
    int a = 0;
};

class ComletDiscovery : public ComletBase {

  public:
    ComletDiscovery() : ComletBase("discovery", "Discover devices on the system") {}
    virtual ~ComletDiscovery() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

  private:
    std::unique_ptr<ComletDiscoveryOptions> opts;
};