#pragma once

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

class ComletBase;
class CoreStub;
namespace CLI {
class App;
} // namespace CLI

struct CLIWrapperOptions {
    bool raw;
};

class CLIWrapper {

  public:
    CLIWrapper(CLI::App &cliApp);
    CLIWrapper &addComlet(const std::shared_ptr<ComletBase> &comlet);
    std::string getResult();

  private:
    CLI::App &cliApp;
    std::unique_ptr<CLIWrapperOptions> opts;
    std::unique_ptr<nlohmann::json> jsonResult;
    std::shared_ptr<CoreStub> coreStub;
};