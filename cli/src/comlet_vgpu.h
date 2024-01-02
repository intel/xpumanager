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
    bool remove = false;
    bool list = false;
    bool kern = false;
    bool assumeYes = false;
    int32_t numVfs = 0;
    std::string lmemPerVf = "";
    bool stats = false;
};

class ComletVgpu: public ComletBase {
    public:
        ComletVgpu();
        virtual ~ComletVgpu() {}

        virtual void setupOptions() override;
        virtual std::unique_ptr<nlohmann::json> run() override;
        virtual void getTableResult(std::ostream &out) override;
        bool isAddKernelParam();
       
    private:

        std::unique_ptr<ComletVgpuOptions> opts;
        bool precheckPassFlag = false;
};

} // end namespace xpum::cli
