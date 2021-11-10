#pragma once

#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace CLI {
class App;
}

namespace xpum::cli {

class ComletBase;
class CoreStub;

struct CLIWrapperOptions {
    bool raw;
    bool table;
};

class CLIWrapper {
   public:
    CLIWrapper(CLI::App &cliApp);
    CLIWrapper &addComlet(const std::shared_ptr<ComletBase> &comlet);
    void printResult(std::ostream &out);

   private:
    CLI::App &cliApp;
    std::unique_ptr<CLIWrapperOptions> opts;
    std::unique_ptr<nlohmann::json> jsonResult;
    std::shared_ptr<CoreStub> coreStub;
    std::vector<std::shared_ptr<ComletBase>> comlets;
};
} // end namespace xpum::cli
