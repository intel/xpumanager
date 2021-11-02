#pragma once

#include <stdint.h>

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletGroupOptions {
    uint32_t groupId = 0;
    std::vector<int> deviceList;
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
    std::unique_ptr<nlohmann::json> destroyGroup();
    std::unique_ptr<nlohmann::json> listGroup();
    std::unique_ptr<nlohmann::json> addDeviceToGroup();
    std::unique_ptr<nlohmann::json> removeDeviceFromGroup();

   private:
    std::unique_ptr<ComletGroupOptions> opts;
};
} // end namespace xpum::cli
