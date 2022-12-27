/* 
 *  Copyright (C) 2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_ps.h
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletPsOptions {
    std::string deviceId = "-1";
};

class ComletPs: public ComletBase {
    public:
        ComletPs();
        virtual ~ComletPs() {}

        virtual void setupOptions() override;
        virtual std::unique_ptr<nlohmann::json> run() override;
        virtual void getTableResult(std::ostream &out) override;
       
    private:
        std::unique_ptr<ComletPsOptions> opts;
        inline static double rnd_2(double val) {
            return round(val * 100) / 100;
        }
};

} // end namespace xpum::cli
