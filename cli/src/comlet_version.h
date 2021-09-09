#pragma once

#include "comlet_base.h"

#include <string>

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