#include <CLI/CLI.hpp>

#include "comlet_version.h"

int main() {
    std::shared_ptr<ComletVersion> comlet = std::make_shared<ComletVersion>();
    auto map = std::map<std::string, std::string>();
    comlet->setupCommand(map);
    comlet->runCommand(ComletVersionOption());
    comlet->baseProcess();
}
