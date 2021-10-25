#pragma once

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace CLI
{
   class App;
}

namespace xpum::cli {

class ComletBase;
class CoreStub;

struct CLIWrapperOptions {
    bool pretty;
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
} // end namespace xpum::cli
