#include "comlet_config.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"
#include "cli_table.h"

namespace xpum::cli {

static CharTableConfig ComletConfigShowConfiguration(R"({
    "indentation": 4,
    "columns": [{
        "title": "Device Type"
    }, {
        "title": "Device ID/Tile ID"
    }, {
        "title": "Configuration"
    }],
    "rows": [{
        "instance": "tile_config_data[]",
        "cells": [
            { "rowTitle": "GPU" },
            "tile_id", [
                { "label": "Power Limit (w) ", "value": "power_limit" },
                { "label": "  Valid Range", "value": "power_vaild_range" },
                { "label": "Power Average Window (ms) ", "value": "power_average_window" },
                { "label": "  Valid Range", "value": "power_average_window_vaild_range" },
                { "rowTitle": " " },
                { "label": "GPU Min Frequency (MHz) ", "value": "min_frequency" },
                { "label": "GPU Max Frequency (MHz) ", "value": "max_frequency" },
                { "label": "  Valid Options", "value": "gpu_frequency_valid_options" },
                {"rowTitle": " " },
                { "label": "Standby Mode", "value": "standby_mode" },
                { "label": "  Valid Options", "value": "standby_mode_valid_options" },
                {"rowTitle": " " },
                { "label": "Scheduler Mode", "value": "scheduler_mode" },
                { "label": "  scheduler watchdog timeout (us) ", "value": "scheduler_watchdog_timeout" },
                { "label": "  scheduler timeslice interval (us) ", "value": "scheduler_timeslice_interval" },
                { "label": "  scheduler timeslice yield timeout (us) ", "value": "scheduler_timeslice_yield_timeout" }
            ]
        ]
    }]
})"_json);

void ComletConfig::setupOptions() {
    this->opts = std::unique_ptr<ComletConfigOptions>(new ComletConfigOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
    addOption("-t,--tile", this->opts->tileId, "tile id");
    addOption("--frequencyrange", this->opts->frequencyrange, "GPU tile-level core frequency range.");
    addOption("--powerlimit", this->opts->powerlimit, "Tile-level power limit.");
    addOption("--standby", this->opts->standby, "Tile-level standby mode. Valid options: \"default\"; \"never\".");
    addOption("--scheduler", this->opts->scheduler, "Tile-level scheduler mode. Value options: \"timeoutc\",timeoutValue (us); \"timeslice\",interval (us),yieldtimeout (us);\"exclusive\".");
    addFlag("--reset", this->opts->resetDevice, "Hard reset the GPU. All applications that are currently using this device will be forcibly killed.");
    
    //addOption("--timeslice", this->opts->schedulerTimeslice, "set scheduler timeslice mode");
    //addOption("--timeout", this->opts->schedulerTimeout, "set scheduler timeout mode");
    //addFlag("--exclusive", this->opts->schedulerExclusive, "set scheduler exclusive mode");
    
    /*addOption("--performancefactor", this->opts->performancefactor, "Set the performance factor.\
Valid options: \"compute/media\",factorValue. The factor value should be \
between 0 to 100. 100 means that the workload is completely compute bounded and requires very little support from the memory support.\
0 means that the workload is completely memory bouded and the performance of the memory controller needs to be increased.");*/
    
    
    
}
std::vector<std::string> ComletConfig::split(std::string str, std::string delimiter){
    size_t pos = 0;
    std::string token;
    std::string str1 = str;
    std::vector<std::string> paraList;
    while ((pos = str1.find(delimiter)) != std::string::npos) {
        token = str1.substr(0, pos);
        paraList.push_back(token);
        str1.erase(0, pos + delimiter.length());
    }
    paraList.push_back(str1);
    return paraList;
}

std::unique_ptr<nlohmann::json> ComletConfig::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["return"]="error";
    if (isQuery()) {
        json = this->coreStub->getDeviceConfig(this->opts->deviceId, this->opts->tileId);
        return json;
    }
    if (this->opts->deviceId >= 0) {
        if (this->opts->tileId >= 0 && !this->opts->scheduler.empty()) {
            std::vector<std::string> paralist = split(this->opts->scheduler, ",");
            int val1,val2;
            std::string command = paralist.at(0);
            std::for_each(command.begin(), command.end(), [](char & c) {
                c = ::tolower(c);
            });
            if (command.compare("timeout") == 0) {
                if (paralist.size() != 2 || paralist.at(1).empty()) {
                   (*json)["return"]="invalid parameter";
                   return json; 
                }
                val1 = std::stoi(paralist.at(1));
                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, SCHEDULER_TIMEOUT,
                    val1,0);
            } else if (command.compare("timeslice") == 0) {
                if (paralist.size() != 3 || paralist.at(1).empty() || paralist.at(2).empty()) {
                   (*json)["return"]="invalid parameter";
                   return json; 
                }
                val1 = std::stoi(paralist.at(1));
                val2 = std::stoi(paralist.at(2));
                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, SCHEDULER_TIMESLICE, val1, val2);
            } else if (command.compare("exclusive") == 0) {
                if (paralist.size() != 1) {
                   (*json)["return"]="invalid parameter";
                   return json; 
                }
                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, SCHEDULER_EXCLUSIVE, 0, 0);
            } else {
                (*json)["return"]="invalid scheduler mode";
                return json; 
            }
            if((*json)["status"] == "OK") {
                (*json)["return"] = "Succeed to change the scheduler mode on GPU " + std::to_string(this->opts->deviceId) +
                " tile " + std::to_string(this->opts->tileId) + ".";
            }
            return json;
        } else if (this->opts->tileId >= 0 && !this->opts->powerlimit.empty()) {
            std::vector<std::string> paralist = split(this->opts->powerlimit, ",");
            if (paralist.size() ==2 && !paralist.at(0).empty() && !paralist.at(1).empty()) {
                int val1 = std::stoi(paralist.at(0));
                int val2 = std::stoi(paralist.at(1));
                json = this->coreStub->setDevicePowerlimit(this->opts->deviceId, this->opts->tileId, val1, val2);
                if((*json)["status"] == "OK") {
                     (*json)["return"] = "Succeed to set the power limit on GPU " + std::to_string(this->opts->deviceId) +
                    " tile " + std::to_string(this->opts->tileId) + ".";
                }
            }else {
                (*json)["return"]="invalid parameter";
                return json;
            }
            return json;
        } else if (this->opts->tileId >= 0 && !this->opts->standby.empty()) {
            XpumStandbyMode mode;
            std::for_each(this->opts->standby.begin(), this->opts->standby.end(), [](char & c) {
                c = ::tolower(c);
            });
            if (this->opts->standby.compare("never") == 0) {
                mode = STANDBY_NEVER;
            } else if (this->opts->standby.compare("default") == 0) {
                mode = STANDBY_DEFAULT;
            } else {
                (*json)["return"]="invalid parameter";
                return json;    
            }
            json = this->coreStub->setDeviceStandby(this->opts->deviceId, this->opts->tileId, mode);
            if((*json)["status"] == "OK") {
                     (*json)["return"] = "Succeed to change the standby mode on GPU " + std::to_string(this->opts->deviceId) +
                    " tile " + std::to_string(this->opts->tileId) + ".";
            }
            return json;
        } else if (this->opts->tileId >= 0 && !this->opts->frequencyrange.empty()) {
            std::vector<std::string> paralist = split(this->opts->frequencyrange, ",");
            if (paralist.size() == 2 && !paralist.at(0).empty() && !paralist.at(1).empty()) {
                int val1 = std::stoi(paralist.at(0));
                int val2 = std::stoi(paralist.at(1));
                json = this->coreStub->setDeviceFrequencyRange(this->opts->deviceId, this->opts->tileId, val1, val2);
                if((*json)["status"] == "OK") {
                    (*json)["return"] = "Succeed to change the core frequency range on GPU " + std::to_string(this->opts->deviceId) +
                    " tile " + std::to_string(this->opts->tileId) + ".";
                }
                return json;
            }else {
                (*json)["return"]="invalid parameter";
                return json;
            }
        } else if (this->opts->tileId >= 0 && !this->opts->performancefactor.empty()) {
            std::vector<std::string> paralist = split(this->opts->performancefactor, ",");
            if (paralist.size() != 2 || paralist.at(1).empty()) {
                (*json)["return"]="invalid parameter";
                return json;
            }
            std::string engine = paralist.at(0);
            std::for_each(engine.begin(), engine.end(), [](char & c) {
                c = ::tolower(c);});
            xpum_engine_type_flags_t engineType;

            if (engine.compare("compute") == 0) {
                engineType = XPUM_COMPUTE;
            } else if (engine.compare("media") == 0) {
                engineType = XPUM_MEDIA;
            } else {
                (*json)["return"]="invalid engine";
                return json;
            }
            double val1 = std::stod(paralist.at(1));
            if (val1 < 0.0 || val1 > 100.0) {
                (*json)["return"]="invalid factor";
                return json;
            }
            json = this->coreStub->setPerformanceFactor(this->opts->deviceId, this->opts->tileId, engineType, val1);
            if((*json)["status"] == "OK") {
                (*json)["return"] = "Succeed to change the performance factor to " + paralist.at(1) +
                " on GPU " + std::to_string(this->opts->deviceId) +
                " tile " + std::to_string(this->opts->tileId) + ".";
            }
            return json;
        } else if (this->opts->tileId == -1 && this->opts->resetDevice) {
            char confirmed;
            if (this->opts->deviceId >= 0) {
                json = this->coreStub->getDeviceProcessState(this->opts->deviceId);
                std::cout <<"The process(es) below are using this device."<<"\n";

                for(auto it= (*json)["device_process_list"].begin(); it!=(*json)["device_process_list"].end();++it) {
                    std::cout <<"PID: "<<(*it)["process_id"] <<" ,";
                    std::cout <<" Command: "<<(*it)["process_name"];
                    std::cout<<"\n";
                }
                //std::cout << json->dump(4) <<"\n";
                std::cout <<"All process(es) above will be forcibly killed if you reset it. Do you want to continue? (Y/N):";
                std::cin>>confirmed;
                if (std::tolower(confirmed) == 'y') {
                    json = this->coreStub->resetDevice(this->opts->deviceId, true); 
                } else {
                    json->clear();
                    (*json)["status"] = "CANCEL";
                    (*json)["return"] = "Reset is cancelled";
                }
            }
            if((*json)["status"] == "OK") {
                //json->clear();
                (*json)["return"] = "Succeed to reset the GPU "+ std::to_string(this->opts->deviceId);
            }
            return json;
        }
        (*json)["return"]="unknonw or invalid command, parameter or device/tile Id";
        return json;
    }
    (*json)["return"]="invalid device Id";
    return json;
}

static void showConfigurations(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    CharTable table(ComletConfigShowConfiguration, *json);
    table.show(out);
}

static void showPureCommandOutput(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
}

void ComletConfig::getTableResult(std::ostream &out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    if (isQuery()) {
        showConfigurations(out, json);
    } else {
        showPureCommandOutput(out, json);
    }
}

} // end namespace xpum::cli
