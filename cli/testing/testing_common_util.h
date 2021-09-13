#pragma once

#include "comlet_base.h"
#include <CLI/CLI.hpp>
#include <gtest/gtest.h>
#include <string>

#define CHAR_PTR(str) ((std::string)(str)).c_str()
#define MAKE_COMLET_PTR(comlet_type) (std::static_pointer_cast<ComletBase>(std::make_shared<comlet_type>()))

std::string runComlet(std::shared_ptr<ComletBase> comlet, std::initializer_list<std::string> parameters) {
    const int argc = parameters.size() + 1;
    auto argvv = new const char *[argc] { "./xpumcli" };

    int i = 1;
    for (auto &parm : parameters) {
        argvv[i] = parm.c_str();
        i++;
    }

    CLI::App app{"Intel XPU Manager Command Line Interface"};
    CLIWrapper wrapper(app);
    wrapper.addComlet(comlet);
    app.require_subcommand(1);
    app.parse(argc, argvv);

    delete[] argvv;

    return wrapper.getResult();
}

void testComlet(std::shared_ptr<ComletBase> comlet, const char *expected, std::initializer_list<std::string> parameters) {
    auto actual = runComlet(comlet, parameters);
    EXPECT_STREQ(expected, actual.c_str());
}

void testComlet(std::shared_ptr<ComletBase> comlet, bool (*check)(std::string &), std::initializer_list<std::string> parameters) {
    auto actual = runComlet(comlet, parameters);
    EXPECT_TRUE(check(actual));
}