#pragma once

#include <comlet_base.h>
#include <string>

struct ComletVersionOption {
    std::string arg1;
    void debug();
};

class ComletVersion : public ComletBase<ComletVersionOption> {

  public:
    ComletVersion() : ComletBase("version") {}

    void setupCommand(std::map<std::string, std::string> &opt) override;
    void runCommand(ComletVersionOption const &opt) override;
};