/*
 *  Copyright (C) 2021-2023 Intel Corporation
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
        int32_t tileId = -1;
        std::string scheduler;
        std::string performancefactor;
        std::string xelinkportEnable;
        std::string xelinkportBeaconing;
        std::string setecc = "";
        bool resetDevice = false;

        //std::string schedulerTimeslice ="";
        //std::string schedulerTimeout ="";
        //bool schedulerExclusive = false;
        std::string powerlimit = "";
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

        virtual void getTableResult(std::ostream& out) override;

        inline const bool isQuery() const {
            return this->opts->deviceId >= 0 && this->opts->scheduler.empty() && this->opts->performancefactor.empty() && this->opts->powerlimit.empty() && this->opts->standby.empty() && this->opts->frequencyrange.empty() && this->opts->xelinkportBeaconing.empty() && this->opts->xelinkportEnable.empty() && this->opts->performancefactor.empty() && this->opts->setecc.empty() && !this->opts->resetDevice;
        }

       private:
        std::unique_ptr<ComletConfigOptions> opts;
        std::vector<std::string> split(std::string str, std::string delimiter);
    };
} // namespace xpum::cli
