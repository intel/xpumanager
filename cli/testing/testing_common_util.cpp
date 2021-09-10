#include "testing_common_util.h"

#include <CLI/CLI.hpp>
#include <gtest/gtest.h>

void testComlet(std::shared_ptr<ComletBase> comlet, const char *expected, std::initializer_list<std::string> parameters) {

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

    auto actual = wrapper.getResult();

    EXPECT_STREQ(expected, actual.c_str());
}