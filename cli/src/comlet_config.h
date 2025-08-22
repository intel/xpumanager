/* 
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_config.h
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletConfigOptions {
    int deviceId = -1;
    std::string device = "";
    int32_t tileId = -1;
    std::string scheduler;
    std::string performancefactor;
    std::string xelinkportEnable;
    std::string xelinkportBeaconing;
    std::string setecc = "";
    bool resetDevice = false;
    bool applyPPR = false;
    bool forcePPR = false;

    //std::string schedulerTimeslice ="";
    //std::string schedulerTimeout ="";
    //bool schedulerExclusive = false;
    std::string powerlimit = "";
    std::string powertype = "";
    std::string standby = "";
    std::string frequencyrange = "";
};

class ComletConfig : public ComletBase {
   public:
    ComletConfig() : ComletBase("config", "Get and change the GPU settings.") {
        printHelpWhenNoArgs = true;
    }
    virtual ~ComletConfig() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

    inline const bool isQuery() const {
        return this->opts->deviceId >= 0 && this->opts->scheduler.empty() && this->opts->performancefactor.empty() && this->opts->powerlimit.empty() && this->opts->standby.empty() && this->opts->frequencyrange.empty() && this->opts->xelinkportBeaconing.empty() && this->opts->xelinkportEnable.empty() && this->opts->performancefactor.empty() && this->opts->setecc.empty()
        && !this->opts->resetDevice && !this->opts->applyPPR;
    }

   private:
    std::unique_ptr<ComletConfigOptions> opts;
    std::vector<std::string> split(std::string str, std::string delimiter);
};
} // end namespace xpum::cli
