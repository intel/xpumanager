#pragma once

#include "comlet_base.h"

struct ComletGroupOptions {
    int groupId = 0;
    int deviceId = -1;
    std::string name = "";
    bool create = false;
    bool del = false;
    bool list = false;
    bool add = false;
    bool remove = false;
};

class ComletGroup : public ComletBase {

  public:
    ComletGroup() : ComletBase("group", "Group management") {}
    virtual ~ComletGroup() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

  private:
    std::unique_ptr<ComletGroupOptions> opts;
};