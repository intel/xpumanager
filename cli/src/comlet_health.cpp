#include "comlet_health.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "cli_table.h"

namespace xpum::cli {

static CharTableConfig ComletConfigHealthDevice(R"({
    "showTitleRow": false,
    "columns": [{
        "title": "none"
    }, {
        "title": "none"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            { "rowTitle": "Device ID" },
            "device_id"
        ]
    }, {
        "instance": "core_temperature",
        "cells": [
            { "rowTitle": "GPU Core Temperature" }, [
            { "label": "Status", "value": "status" },
            { "label": "Description", "value": "description" },
            { "label": "Throttle Threshold", "suffix": " Celsius Degree", "value": "throttle_threshold", "fixer": "negint_novalue" },
            { "label": "Shutdown Threshold", "suffix": " Celsius Degree", "value": "shutdown_threshold", "fixer": "negint_novalue" },
            { "label": "Custom Threshold", "suffix": " Celsius Degree", "value": "custom_threshold", "fixer": "negint_novalue" }
        ]]
    }, {
        "instance": "memory_temperature",
        "cells": [
            { "rowTitle": "GPU Memory Temperature" }, [
            { "label": "Status", "value": "status" },
            { "label": "Description", "value": "description" },
            { "label": "Throttle Threshold", "suffix": " Celsius Degree", "value": "throttle_threshold", "fixer": "negint_novalue" },
            { "label": "Shutdown Threshold", "suffix": " Celsius Degree", "value": "shutdown_threshold", "fixer": "negint_novalue" },
            { "label": "Custom Threshold", "suffix": " Celsius Degree", "value": "custom_threshold", "fixer": "negint_novalue" }
        ]]
    }, {
        "instance": "power",
        "cells": [
            { "rowTitle": "GPU Power" }, [
            { "label": "Status", "value": "status" },
            { "label": "Description", "value": "description" },
            { "label": "Throttle Threshold", "suffix": " Celsius Degree", "value": "throttle_threshold", "fixer": "negint_novalue" },
            { "label": "Custom Threshold", "suffix": " Celsius Degree", "value": "custom_threshold", "fixer": "negint_novalue" }
        ]]
    }, {
        "instance": "memory",
        "cells": [
            { "rowTitle": "GPU Memory" }, [
            { "label": "Status", "value": "status" },
            { "label": "Description", "value": "description" }
        ]]
    }, {
        "instance": "fabric_port",
        "cells": [
            { "rowTitle": "GPU Fabric Port" }, [
            { "label": "Status", "value": "status" },
            { "label": "Description", "value": "description" }
        ]]
    }]
})"_json);

void ComletHealth::setupOptions() {
    this->opts = std::unique_ptr<ComletHealthOptions>(new ComletHealthOptions());
    addFlag("-l,--list", this->opts->listAll, "Display health info for all devices");
    addOption("-d,--device", this->opts->deviceId, "The device ID");
    addOption("-g,--group", this->opts->groupId, "The group ID");
    addOption("-c,--component", this->opts->componentType, "Commponent types\n\
      1. GPU Core Temperature\n\
      2. GPU Memory Temperature\n\
      3. GPU Power\n\
      4. GPU Memory");
    addOption("--threshold", this->opts->threshold, "Set custom threshold for device component");
}

std::unique_ptr<nlohmann::json> ComletHealth::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (this->opts->listAll) {
        json = this->coreStub->getAllHealth();
        return json;
    }

    if (this->opts->deviceId != INT_MIN && this->opts->deviceId < 0) {
        (*json)["error"] = "device not found";
        return json;
    }

    if (this->opts->groupId == 0) {
        (*json)["error"] = "group not found";
        return json;
    }
    
    if (this->opts->componentType != INT_MIN && (this->opts->componentType < 1 || this->opts->componentType > 5)) {
        (*json)["error"] = "invalid component";
        return json;
    }

    if ((this->opts->threshold != INT_MIN && this->opts->threshold < -1)
        || (this->opts->threshold == 0)) {
        (*json)["error"] = "invalid threshold";
        return json;
    }

    if (this->opts->deviceId >= 0) {
        if (this->opts->threshold >= -1) {
            if (this->opts->componentType == 1) {
                json = this->coreStub->setHealthConfig(this->opts->deviceId, HEALTH_CORE_THEARMAL_LIMIT, this->opts->threshold);
            } else if (this->opts->componentType == 2) {
                json = this->coreStub->setHealthConfig(this->opts->deviceId, HEALTH_MEMORY_THEARMAL_LIMIT, this->opts->threshold);
            } else if (this->opts->componentType == 3) {
                json = this->coreStub->setHealthConfig(this->opts->deviceId, HEALTH_POWER_LIMIT, this->opts->threshold);
            } else {
                (*json)["error"] = "threshold setting unsupported";
            }
            if ((*json).contains("error")) {
                return json;
            }
            json = this->coreStub->getHealth(this->opts->deviceId, this->opts->componentType);
            return json;
        } else {
           if (this->opts->componentType >= 1 && this->opts->componentType <= 5)
                json = this->coreStub->getHealth(this->opts->deviceId, this->opts->componentType);
            else
                json = this->coreStub->getHealth(this->opts->deviceId, -1);
            return json;
        }
    }

    if (this->opts->groupId > 0 && this->opts->groupId != UINT_MAX) {
        if (this->opts->threshold >= -1) {
            if (this->opts->componentType == 1) {
                json = this->coreStub->setHealthConfigByGroup(this->opts->groupId, HEALTH_CORE_THEARMAL_LIMIT, this->opts->threshold);
            } else if (this->opts->componentType == 2) {
                json = this->coreStub->setHealthConfigByGroup(this->opts->groupId, HEALTH_MEMORY_THEARMAL_LIMIT, this->opts->threshold);
            } else if (this->opts->componentType == 3) {
                json = this->coreStub->setHealthConfigByGroup(this->opts->groupId, HEALTH_POWER_LIMIT, this->opts->threshold);
            } else {
                (*json)["error"] = "threshold setting unsupported";
            }
            if ((*json).contains("error")) {
                return json;
            }
            json = this->coreStub->getHealthByGroup(this->opts->groupId, this->opts->componentType);
            return json;
        } else {
           if (this->opts->componentType >= 1 && this->opts->componentType <= 5)
                json = this->coreStub->getHealthByGroup(this->opts->groupId, this->opts->componentType);
            else
                json = this->coreStub->getHealthByGroup(this->opts->groupId, -1);
            return json;
        }
    }
    (*json)["error"] = "Wrong argument or unknown operation, run with --help for more information.";
    return json;
}

static void showHealth(std::ostream &out, std::shared_ptr<nlohmann::json> json, const bool cont = false) {
    CharTable table(ComletConfigHealthDevice, *json, cont);
    table.show(out);
}

void ComletHealth::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;
    if (this->opts->deviceId >= 0) {
        showHealth(out, json);
    } else {
        auto devices = (*json)["datas"].get<std::vector<nlohmann::json>>();
        bool cont = false;
        for (auto device : devices) {
            showHealth(out, std::make_shared<nlohmann::json>(device), cont);
            cont = true;
        }
    }

}
} // end namespace xpum::cli
