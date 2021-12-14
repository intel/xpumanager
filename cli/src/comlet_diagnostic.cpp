#include "comlet_diagnostic.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"
#include <sstream>

namespace xpum::cli {

void ComletDiagnostic::setupOptions() {
    this->opts = std::unique_ptr<ComletDiagnosticOptions>(new ComletDiagnosticOptions());
    addOption("-d,--device", this->opts->deviceId, "The device ID");
    addOption("-g,--group", this->opts->groupId, "The group ID");
    addOption("-l,--level", this->opts->level, "The diagnostics level to run. The valid options include\n\
      1. quick test\n\
      2. medium test\n\
      3. long test");
}

std::unique_ptr<nlohmann::json> ComletDiagnostic::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    if (this->opts->deviceId != INT_MIN && this->opts->deviceId < 0) {
        (*json)["error"] = "device not found";
        return json;
    }

    if (this->opts->groupId == 0) {
        (*json)["error"] = "group not found";
        return json;
    }

    if (this->opts->level != INT_MIN && (this->opts->level < 1 || this->opts->level > 3)) {
        (*json)["error"] = "invalid level";
        return json;
    }
    if (this->opts->level >= 1 && this->opts->level <= 3) {
        if (this->opts->deviceId >= 0) {
            json = this->coreStub->runDiagnostics(this->opts->deviceId, this->opts->level);
            // parsedProcessesList(*json);
            return json;
        } else if (this->opts->groupId > 0 && this->opts->groupId != UINT_MAX) {
            json = this->coreStub->runDiagnosticsByGroup(this->opts->groupId, this->opts->level);
            // parsedProcessesList(*json);
            return json;
        }
    }
    (*json)["error"] = "Wrong argument or unknown operation, run with --help for more information.";
    return json;
}

void ComletDiagnostic::parsedProcessesList(nlohmann::json& json) {
    if (json.contains("component_list")) {
        for (auto &item : json["component_list"]) {
            if (item["component_type"] == "XPUM_DIAG_SOFTWARE_EXCLUSIVE" && item["result"] == "Fail") {
                parsedProcessesListCore(item);
            }
        }
    }

    if (json.contains("device_list")) {
        for (auto &device : json["device_list"]) {
            for (auto &item : device["component_list"]) {
                if (item["component_type"] == "XPUM_DIAG_SOFTWARE_EXCLUSIVE" && item["result"] == "Fail") {
                    parsedProcessesListCore(item);
                }
            }
        }
    }
}

void ComletDiagnostic::parsedProcessesListCore(nlohmann::json& item) {
    std::string rawMessage = item["message"];
    size_t messagePosition = rawMessage.find(" Self-PID:");
    item["message"] = rawMessage.substr(0, messagePosition);
    std::string processList = rawMessage.substr(rawMessage.find("List:") + 5);
    std::stringstream ss(processList);
    std::string process;
    std::vector<nlohmann::json> processJsonList;
    while (getline(ss, process, '#')) {
        auto processJson = nlohmann::json();
        if (process.find("...") == process.size() - 3) {
            processJson["PID"] = "...";
            processJson["Command"] = "...";
        } else {
            size_t processPosition = process.find("-");
            processJson["PID"] = process.substr(1, processPosition - 1);
            processJson["Command"] = process.substr(processPosition + 1);
        }
        processJsonList.push_back(processJson);
    }
    item["process_list"] = processJsonList;
}
} // end namespace xpum::cli
