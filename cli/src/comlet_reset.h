/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_reset.h
 */

#pragma once

#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletResetOptions {
    std::string deviceId = "-1";
};

class ComletReset : public ComletBase {
   public:
    ComletReset() : ComletBase("reset", "Hard reset the GPU. All applications that are currently using this device will be forcibly killed.") {}
    virtual ~ComletReset() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

   private:
    std::unique_ptr<ComletResetOptions> opts;
};
} // end namespace xpum::cli
