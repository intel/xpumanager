/* 
 *  Copyright (C) 2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_top.h
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletTopOptions {
    std::string deviceId = "-1";
};

class ComletTop: public ComletBase {
    public:
        ComletTop();
        virtual ~ComletTop() {}

        virtual void setupOptions() override;
        virtual std::unique_ptr<nlohmann::json> run() override;
        virtual void getTableResult(std::ostream &out) override;
       
    private:
        std::unique_ptr<ComletTopOptions> opts;
        inline static double rnd_2(double val) {
            return round(val * 100) / 100;
        }
};

} // end namespace xpum::cli
