#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

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
} // end namespace xpum::cli
