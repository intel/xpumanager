#pragma once

#include "comlet_base.h"
#include "config.h"

#include <string>

struct ComletVersionOptions {
    bool verbose;
    std::string argA;
    std::string argB;
    int times;
};

class ComletVersion : public ComletBase {

  public:
    ComletVersion() : ComletBase("version") {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

  private:
    std::shared_ptr<ComletVersionOptions> opts;
};