/* 
 *  Copyright (C) 2022-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_log.h
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletLogOptions {
    std::string fileName = "";
};

class ComletLog: public ComletBase {
    public:
        ComletLog();
        virtual ~ComletLog() {}

        virtual void setupOptions() override;
        virtual std::unique_ptr<nlohmann::json> run() override;
        virtual void getTableResult(std::ostream &out) override;
       
    private:
        std::unique_ptr<ComletLogOptions> opts;
};

} // end namespace xpum::cli
