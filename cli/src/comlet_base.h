#pragma once

#include "cli_wrapper.h"
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include <cassert>
#include <string>

class ComletBase {

    friend class CLIWrapper;

  public:
    ComletBase(std::string command) : command(command) {}
    virtual ~ComletBase() {}

    virtual void setupOptions() = 0;
    virtual std::unique_ptr<nlohmann::json> run() = 0;

  protected:
    template <typename T>
    void addOption(std::string optName, T &variable, std::string optDescription = "", bool required = false) {
        assert(subCLIApp);
        auto opt = this->subCLIApp->add_option(optName, variable, optDescription);
        if (required) {
            opt->required();
        }
    }

    template <typename T>
    void addFlag(std::string optName, T &variable, std::string optDescription = "") {
        assert(subCLIApp);
        this->subCLIApp->add_flag(optName, variable, optDescription);
    };

  private:
    const std::string command;
    CLI::App *subCLIApp;
};