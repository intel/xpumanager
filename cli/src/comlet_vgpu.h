/* 
 *  Copyright (C) 2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_vgpu.h
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {

struct ComletVgpuOptions {
    std::string deviceId = "";
    bool precheck = false;
    bool create = false;
    int32_t numVfs = 0;
    std::string lmemPerVf = "";
};

class ComletVgpu: public ComletBase {
    public:
        ComletVgpu();
        virtual ~ComletVgpu() {}

        virtual void setupOptions() override;
        virtual std::unique_ptr<nlohmann::json> run() override;
        virtual void getTableResult(std::ostream &out) override;
       
    private:
        std::unique_ptr<ComletVgpuOptions> opts;
        int parsedDeviceId;
        bool precheckPassFlag = false;
};

} // end namespace xpum::cli
