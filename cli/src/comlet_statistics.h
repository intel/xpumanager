#pragma once

#include "comlet_base.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

struct ComletStatisticsOptions {
    int deviceId = -1;
    int groupId = -1;
};

class ComletStatistics : public ComletBase {

  public:
    ComletStatistics() : ComletBase("stats", "Display device statistics") {}
    virtual ~ComletStatistics() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

  private:
    std::unique_ptr<ComletStatisticsOptions> opts;
};