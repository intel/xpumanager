#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <climits>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletDiagnosticOptions {
    int deviceId = INT_MIN;
    uint32_t groupId = UINT_MAX;
    int level = INT_MIN;
};

class ComletDiagnostic : public ComletBase {
   public:
    ComletDiagnostic() : ComletBase("diag", "System validation/diagnostic") {}
    virtual ~ComletDiagnostic() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

   private:
    std::unique_ptr<ComletDiagnosticOptions> opts;
};
} // end namespace xpum::cli
