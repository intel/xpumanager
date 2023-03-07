/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_agentset.h
 */

#pragma once

#include "comlet_base.h"

namespace xpum::cli {

struct ComletAgentSetOptions {
    bool list;
    int samplingInterval = -1;
};

class ComletAgentSet : public ComletBase {
   private:
    std::unique_ptr<ComletAgentSetOptions> opts;

   public:
    ComletAgentSet();

    virtual void setupOptions() override;

    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

    inline const bool isListOperation() const {
        return opts->list;
    }

    inline const int getSamplingInterval() const {
        return opts->samplingInterval;
    }
};
} // namespace xpum::cli
