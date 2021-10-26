#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletDiagnosticOptions {
    int deviceId = -1;
    uint32_t groupId = -1;
    int level = -1;
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
