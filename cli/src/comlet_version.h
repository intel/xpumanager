#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletVersionOptions {
    bool verbose = false;
    std::string argA = "";
    std::string argB = "";
    int times = 0;
};

class ComletVersion : public ComletBase {
   public:
    ComletVersion() : ComletBase("version", "Print version information") {}
    virtual ~ComletVersion() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

   private:
    std::unique_ptr<ComletVersionOptions> opts;
};
} // end namespace xpum::cli
