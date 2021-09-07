#pragma once

#include "comlet_base.h"

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
    virtual void run() override;

  private:
    std::shared_ptr<ComletVersionOptions> opts;
};