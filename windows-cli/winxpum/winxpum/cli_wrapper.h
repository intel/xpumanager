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



class ComletBase;
class CoreStub;

struct CLIWrapperOptions {
    bool raw;
    bool json;
    bool version;
};

class CLIWrapper {
public:
    CLIWrapper(CLI::App& cliApp);
    CLIWrapper& addComlet(const std::shared_ptr<ComletBase>& comlet);
    void printResult(std::ostream& out);

private:
    CLI::App& cliApp;
    std::unique_ptr<CLIWrapperOptions> opts;
    std::unique_ptr<nlohmann::json> jsonResult;
    std::shared_ptr<CoreStub> coreStub;
    std::vector<std::shared_ptr<ComletBase>> comlets;
};
