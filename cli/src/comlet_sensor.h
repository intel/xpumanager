/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_sensor.h
 */

#pragma once

#include "comlet_base.h"
#include "core_stub.h"

namespace xpum::cli {

class ComletSensor : public ComletBase {
   public:
    ComletSensor() : ComletBase("amcsensor", "List the AMC real-time sensor readings.") {
        // printHelpWhenNoArgs = true;
    }
    virtual ~ComletSensor() {}

    virtual void setupOptions() override{};

    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream &out) override;

};
}