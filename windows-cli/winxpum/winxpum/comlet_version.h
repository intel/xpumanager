/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_version.h
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"


struct ComletVersionOptions {
    bool verbose = false;
    std::string argA = "";
    std::string argB = "";
    int times = 0;
};

class ComletVersion : public ComletBase {
public:
    ComletVersion() : ComletBase("version", "Print version information") {}
    virtual ~ComletVersion() {}

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;

    virtual void getTableResult(std::ostream& out) override;

private:
    std::unique_ptr<ComletVersionOptions> opts;
};
