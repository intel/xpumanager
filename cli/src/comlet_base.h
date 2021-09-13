#pragma once

#include "cli_wrapper.h"
#include <CLI/CLI.hpp>

#include <cassert>
#include <nlohmann/json.hpp>
#include <string>

class CoreStub;

class ComletBase {

    friend class CLIWrapper;

  public:
    ComletBase(std::string command, std::string description) : command(command), description(description) {}
    virtual ~ComletBase() {}

    virtual void setupOptions() = 0;
    virtual std::unique_ptr<nlohmann::json> run() = 0;

  protected:
    template <typename T>
    void addOption(std::string optName, T &variable, std::string optDescription = "", bool required = false) {
        assert(subCLIApp != nullptr);
        auto opt = this->subCLIApp->add_option(optName, variable, optDescription);
        if (required) {
            opt->required();
        }
    }

    template <typename T>
    void addFlag(std::string optName, T &variable, std::string optDescription = "") {
        assert(subCLIApp != nullptr);
        this->subCLIApp->add_flag(optName, variable, optDescription);
    };

    std::shared_ptr<CoreStub> coreStub;

  private:
    const std::string command;
    const std::string description;
    CLI::App *subCLIApp;
};