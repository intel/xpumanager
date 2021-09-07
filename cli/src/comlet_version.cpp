#include "comlet_version.h"
#include "config.h"

#include <iostream>

void ComletVersion::setupOptions() {
    this->opts = std::make_shared<ComletVersionOptions>();
    addFlag("-v,--verbose", this->opts->verbose, "flag for verbose output");
    addOption("-a, --argA", this->opts->argA, "one string arg", true);
    addOption("-b, --argB", this->opts->argB, "one more string arg", false);
    addOption("-n, --times", this->opts->times, "integer arg", false);
}

void ComletVersion::run() {
    std::cout << "CLI Version: " << XPUM_VERSION << std::endl;
    std::cout << "verbose:" << this->opts->verbose << std::endl;
    std::cout << "argA:" << this->opts->argA << std::endl;
    std::cout << "argB:" << this->opts->argB << std::endl;
    std::cout << "times:" << this->opts->times << std::endl;
}