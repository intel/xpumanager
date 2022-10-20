/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_base.h
 */

#pragma once

#include <CLI/CLI.hpp>
#include <cassert>
#include <nlohmann/json.hpp>
#include <string>

#include "cli_wrapper.h"

namespace xpum::cli {

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

    virtual void getJsonResult(std::ostream &out, bool raw = false) {
        auto pJson = this->run();
        setExitCodeByJson(*pJson);
        if (raw) {
            out << pJson->dump() << std::endl;
            return;
        } else {
            out << pJson->dump(4) << std::endl;
            return;
        }
    }

    virtual void getTableResult(std::ostream &out) {
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
    CLI::Option *addOption(std::string optName, T &variable, std::string optDescription = "", bool required = false) {
        assert(subCLIApp != nullptr);
        auto opt = this->subCLIApp->add_option(optName, variable, optDescription);
        if (required) {
            opt->required();
        }
        return opt;
    }

    template <typename T>
    CLI::Option *addFlag(std::string optName, T &variable, std::string optDescription = "") {
        assert(subCLIApp != nullptr);
        return this->subCLIApp->add_flag(optName, variable, optDescription);
    };

    std::shared_ptr<CoreStub> coreStub;

    std::string getCommand() {
        return command;
    }

    int setExitCodeByJson(nlohmann::json json) {
        if (!json.contains("errno")) {
            return 0;
        }
        exit_code = json["errno"];
        return exit_code;
    }

    int exit_code = 0;
   private:
    const std::string command;
    const std::string description;
    CLI::App *subCLIApp;

   public:
    bool printHelpWhenNoArgs = 0;
};
} // end namespace xpum::cli
