/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_base.h
 */

#pragma once

#include <CLI/CLI.hpp>
#include <cassert>
#include <nlohmann/json.hpp>
#include <string>

#include "cli_wrapper.h"

class CoreStub;

class ComletBase {
    friend class CLIWrapper;

public:
    ComletBase(std::string command, std::string description) : command(command), description(description) {}
    virtual ~ComletBase() {}

    virtual void setupOptions() = 0;
    virtual std::unique_ptr<nlohmann::json> run() = 0;

    bool parsed() {
        return this->subCLIApp->parsed();
    }

    virtual void getJsonResult(std::ostream& out, bool raw = false) {
        if (raw) {
            out << this->run()->dump() << std::endl;
            return;
        }
        else {
            out << this->run()->dump(4) << std::endl;
            return;
        }
    }

    virtual void getTableResult(std::ostream& out) {
        out << "Only -j/--json option supported for this command" << std::endl;
    }

    bool isEmpty() {
        bool empty = true;
        auto options = subCLIApp->get_options();
        for (auto option : options) {
            empty = empty && option->empty();
        }
        return empty;
    }

protected:
    template <typename T>
    CLI::Option* addOption(std::string optName, T& variable, std::string optDescription = "", bool required = false) {
        assert(subCLIApp != nullptr);
        auto opt = this->subCLIApp->add_option(optName, variable, optDescription);
        if (required) {
            opt->required();
        }
        return opt;
    }

    template <typename T>
    CLI::Option* addFlag(std::string optName, T& variable, std::string optDescription = "") {
        assert(subCLIApp != nullptr);
        return this->subCLIApp->add_flag(optName, variable, optDescription);
    };

    std::shared_ptr<CoreStub> coreStub;

private:
    const std::string command;
    const std::string description;
    CLI::App* subCLIApp;

public:
    bool printHelpWhenNoArgs = 0;
};
