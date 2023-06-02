/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file cli_wrapper.h
 */

#pragma once

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace CLI {
    class App;
}

namespace xpum::cli {

    class ComletBase;
    class CoreStub;

    struct CLIWrapperOptions {
        bool raw;
        bool json;
        bool version;
    };

    class CLIWrapper {
    public:
        CLIWrapper(CLI::App& cliApp, bool privilege);
        CLIWrapper& addComlet(const std::shared_ptr<ComletBase>& comlet);
        int printResult(std::ostream& out);
        std::shared_ptr<CoreStub> getCoreStub();

    private:
        CLI::App& cliApp;
        std::unique_ptr<CLIWrapperOptions> opts;
        std::unique_ptr<nlohmann::json> jsonResult;
        std::shared_ptr<CoreStub> coreStub;
        std::vector<std::shared_ptr<ComletBase>> comlets;
    };
} // end namespace xpum::cli