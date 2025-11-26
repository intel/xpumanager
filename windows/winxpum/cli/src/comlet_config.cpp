/*
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_config.cpp
 */

#include "comlet_config.h"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include "level_zero/zes_api.h"

#include "core_stub.h"
#include "cli_table.h"
#include "exit_code.h"

namespace xpum::cli {

    static CharTableConfig ComletDeviceConfiguration(R"({
        "indentation": 4,
        "columns": [{
            "title": "Device Type"
        }, {
            "title": "Device ID/Tile ID"
        }, {
            "title": "Configuration"
        }],
        "rows": [{
            "instance": "",
            "cells": [
                { "rowTitle": "GPU" },
                "device_id", [
                    { "rowTitle": "Power domain card:" },
                    { "label": "  sustain(w) ", "value": "pl_card_sustain" },
                    { "label": "  burst(w) ", "value": "pl_card_burst" },
                    { "label": "  peak(w) ", "value": "pl_card_peak" },
                    { "rowTitle": "Power domain package:" },
                    { "label": "  sustain(w) ", "value": "pl_package_sustain" },
                    { "label": "  burst(w) ", "value": "pl_package_burst" },
                    { "label": "  peak(w) ", "value": "pl_package_peak" },
                    { "label": "  Valid Range", "value": "power_valid_range" },
                    {"rowTitle": " " },
                    { "rowTitle": "Memory ECC:" },
                    { "label": "  Current", "value": "memory_ecc_current_state" },
                    { "label": "  Pending", "value": "memory_ecc_pending_state" }
                ]
            ]
        }]
    })"_json);

    static CharTableConfig ComletTileConfiguration(R"({
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
                    { "label": "GPU Min Frequency (MHz) ", "value": "min_frequency" },
                    { "label": "GPU Max Frequency (MHz) ", "value": "max_frequency" },
                    { "label": "  Valid Options", "value": "gpu_frequency_valid_options" },
                    {"rowTitle": " " },
                    { "label": "Standby Mode", "value": "standby_mode" },
                    { "label": "  Valid Options", "value": "standby_mode_valid_options" },
                    {"rowTitle": " " },
                    { "label": "Scheduler Mode", "value": "scheduler_mode" },
                    { "label": "  Timeout (us) ", "value": "scheduler_watchdog_timeout" },
                    { "label": "  Interval (us) ", "value": "scheduler_timeslice_interval" },
                    { "label": "  Yield Timeout (us) ", "value": "scheduler_timeslice_yield_timeout" },
                    {"rowTitle": " " },
                    { "label": "Engine Type", "value": "compute_engine" },
                    { "label": "  Performance Factor", "value": "compute_performance_factor" },
                    { "label": "Engine Type", "value": "media_engine" },
                    { "label": "  Performance Factor", "value": "media_performance_factor" },
                    {"rowTitle": " " },
                    { "rowTitle": "Xe Link ports:" },
                    { "label": "  Up", "value": "port_up" },
                    { "label": "  Down", "value": "port_down" },
                    { "label": "  Beaconing On", "value": "beaconing_on" },
                    { "label": "  Beaconing Off", "value": "beaconing_off" }
                ]
            ]
        }]
    })"_json);

    void ComletConfig::setupOptions() {
        this->opts = std::unique_ptr<ComletConfigOptions>(new ComletConfigOptions());
        addOption("-d,--device", this->opts->deviceId, "device id");
        addOption("-t,--tile", this->opts->tileId, "tile id");
        addOption("--frequencyrange", this->opts->frequencyrange, "GPU tile-level core frequency range.");
        addOption("--powerlimit", this->opts->powerlimit, "Device-level power limit.");
	addOption("--powertype", this->opts->powertype, "Device-level power limit type. Valid options: \"sustain\"; \"peak\"; \"burst\"");	
        addOption("--memoryecc", this->opts->setecc, "Enable/disable memory ECC setting. 0:disable; 1:enable");
        addOption("--standby", this->opts->standby, "Tile-level standby mode. Valid options: \"default\"; \"never\".");
        addOption("--scheduler", this->opts->scheduler, "Tile-level scheduler mode. Value options: \"timeout\",timeoutValue (us); \"timeslice\",interval (us),yieldtimeout (us);\"exclusive\".The valid range of all time values (us) is from 5000 to 100,000,000.");
        //addFlag("--reset", this->opts->resetDevice, "Hard reset the GPU. All applications that are currently using this device will be forcibly killed.");
        //addOption("--timeslice", this->opts->schedulerTimeslice, "set scheduler timeslice mode");
        //addOption("--timeout", this->opts->schedulerTimeout, "set scheduler timeout mode");
        //addFlag("--exclusive", this->opts->schedulerExclusive, "set scheduler exclusive mode");

        addOption("--performancefactor", this->opts->performancefactor,
                  "Set the tile-level performance factor. Valid options: \"compute/media\";factorValue. The factor value should be\n\
    between 0 to 100. 100 means that the workload is completely compute bounded and requires very little support from the memory support. 0 means that the workload is completely memory bounded and the performance of the memory controller needs to be increased.");
        addOption("--xelinkport", this->opts->xelinkportEnable, "Change the Xe Link port status. The value 0 means down and 1 means up.");
        addOption("--xelinkportbeaconing", this->opts->xelinkportBeaconing, "Change the Xe Link port beaconing status. The value 0 means off and 1 means on.");
    }
    std::vector<std::string> ComletConfig::split(std::string str, std::string delimiter) {
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
        (*json)["return"] = "error";
        if (isQuery()) {
            json = this->coreStub->getDeviceConfig(this->opts->deviceId, this->opts->tileId);
            return json;
        }
        if (this->opts->deviceId >= 0) {
            if (this->opts->tileId >= 0 && !this->opts->scheduler.empty()) {
                (*json)["return"] = "unsupported feature";
                return json;
            } else if (/*this->opts->tileId >= 0 &&*/ !this->opts->powerlimit.empty()) {
                int power_limit = 0;
                int32_t power_level;
                std::unordered_map<std::string, int32_t> power_type = {
                    {"sustain", static_cast<int32_t>(ZES_POWER_LEVEL_SUSTAINED)},
                    {"peak", static_cast<int32_t>(ZES_POWER_LEVEL_PEAK)},
                    {"burst", static_cast<int32_t>(ZES_POWER_LEVEL_BURST)}
                };
                if (this->opts->powertype.empty()) {
                    power_level = static_cast<int32_t>(ZES_POWER_LEVEL_SUSTAINED);
                }else if (power_type.find(this->opts->powertype) != power_type.end()) {
                    power_level = power_type[this->opts->powertype];
                } else {
                    (*json)["return"] = "Invalid powertype value: " + this->opts->powertype;
                    return json;
                }
                try {
                    power_limit = std::stoi(this->opts->powerlimit);
                } catch (std::exception &e) {
                    (*json)["return"] = "invalid parameter: powerlimit";
                    return json;
                }
                if (power_limit <= 0) {
                    (*json)["return"] = "invalid parameter: power limit should greater than 0.";
                    return json;
                }
                this->opts->tileId = -1;
                xpum_power_limit_ext_t power_limit_ext = {power_limit, power_level};
                json = this->coreStub->setDevicePowerlimitExt(this->opts->deviceId, this->opts->tileId, power_limit_ext);
                if ((*json).contains("errno")) {
                    (*json)["error"] = getErrorString((*json)["errno"]);
                }else if ((*json)["status"] == "OK") {
                    (*json)["return"] = "Succeed to set the power limit on GPU " + std::to_string(this->opts->deviceId);
                }
                return json;
            } else if (this->opts->tileId >= 0 && !this->opts->standby.empty()) {
                (*json)["return"] = "unsupported feature";
                return json;
            } else if (this->opts->tileId >= 0 && !this->opts->frequencyrange.empty()) {
                std::vector<std::string> paralist = split(this->opts->frequencyrange, ",");
                if (paralist.size() == 2 && !paralist.at(0).empty() && !paralist.at(1).empty()) {
                    int val1;
                    int val2;
                    try {
                        val1 = std::stoi(paralist.at(0));
                        val2 = std::stoi(paralist.at(1));
                    } catch (std::invalid_argument const& e) {
                        (*json)["return"] = "invalid parameter: frequencyrange";
                        return json;
                    } catch (std::out_of_range const& e) {
                        (*json)["return"] = "invalid parameter: frequencyrange";
                        return json;
                    }
                    if (val1 > val2) {
                        (*json)["return"] = "invalid parameter: frequencyrange";
                        return json;
                    }
                    json = this->coreStub->setDeviceFrequencyRange(this->opts->deviceId, this->opts->tileId, val1, val2);
                    if ((*json)["status"] == "OK") {
                        (*json)["return"] = "Succeed to change the core frequency range on GPU " + std::to_string(this->opts->deviceId) +
                                            " tile " + std::to_string(this->opts->tileId) + ".";
                    }
                    return json;
                } else {
                    (*json)["return"] = "invalid parameter: please check help information";
                    return json;
                }
            } else if (this->opts->tileId >= 0 && !this->opts->performancefactor.empty()) {
                (*json)["return"] = "unsupported feature";
                return json;
            } else if (this->opts->tileId >= 0 && !this->opts->xelinkportEnable.empty()) {
                (*json)["return"] = "unsupported feature";
                return json;
            } else if (this->opts->tileId >= 0 && !this->opts->xelinkportBeaconing.empty()) {
                (*json)["return"] = "unsupported feature";
                return json;
            } else if (!this->opts->setecc.empty()) {
                bool enabled = false;
                int eccVal;
                try {
                    eccVal = std::stoi(this->opts->setecc);
                } catch (std::invalid_argument const& e) {
                    (*json)["return"] = "invalid parameter: memoryecc";
                    return json;
                } catch (std::out_of_range const& e) {
                    (*json)["return"] = "invalid parameter: memoryecc";
                    return json;
                }
                if (eccVal == 1) {
                    enabled = true;
                } else if (eccVal == 0) {
                    enabled = false;
                } else {
                    (*json)["return"] = "invalid parameter: memoryecc";
                    return json;
                }
                json = this->coreStub->setMemoryEccState(this->opts->deviceId, enabled);
                if ((*json)["status"] == "OK") {
                    std::string current = (*json)["memory_ecc_current_state"];
                    std::string pending = (*json)["memory_ecc_pending_state"];

                    /* (*json)["return"] = "Succeed to set memory Ecc state: available: " + available +
                    " configurable: " + configurable +
                    " current: " + current +
                    " pending: " + pending + 
                    " action: " +  pendingAction;*/
                    (*json)["return"] = "Successfully " + (enabled ? std::string("enable") : std::string("disable")) + " ECC memory on GPU " + std::to_string(this->opts->deviceId) + ". Please reset the GPU or reboot the OS for the change to take effect.";
                } else {
                    (*json)["return"] = "Failed to " + (enabled ? std::string("enable") : std::string("disable")) + " ECC memory on GPU " + std::to_string(this->opts->deviceId) + ".";
                }
                return json;
            }
            /*else if (this->opts->tileId == -1 && this->opts->resetDevice) {
                char confirmed;
                if (this->opts->deviceId >= 0) {
                    json = this->coreStub->getDeviceProcessState(this->opts->deviceId);
                    std::cout << "The process(es) below are using this device." << "\n";

                    for (auto it = (*json)["device_process_list"].begin(); it != (*json)["device_process_list"].end();++it) {
                        std::cout << "PID: " << (*it)["process_id"] << " ,";
                        std::cout << " Command: " << (*it)["process_name"];
                        std::cout << "\n";
                    }
                    //std::cout << json->dump(4) <<"\n";
                    std::cout << "All process(es) above will be forcibly killed if you reset it. Do you want to continue? (Y/N):";
                    std::cin >> confirmed;
                    if (std::tolower(confirmed) == 'y') {
                        json = this->coreStub->resetDevice(this->opts->deviceId, true);
                    }
                    else {
                        json->clear();
                        (*json)["status"] = "CANCEL";
                        (*json)["return"] = "Reset is cancelled";
                    }
                }
                if ((*json)["status"] == "OK") {
                    //json->clear();
                    (*json)["return"] = "Succeed to reset the GPU " + std::to_string(this->opts->deviceId);
                }
                return json;
            }*/
            (*json)["return"] = "unknown or invalid command, parameter or device/tile Id";
            return json;
        }
        (*json)["return"] = "invalid device Id";
        return json;
    }

    static void showConfigurations(std::ostream& out, std::shared_ptr<nlohmann::json> json) {
        CharTable table1(ComletDeviceConfiguration, *json);
        CharTable table2(ComletTileConfiguration, *json, true);
        table1.show(out);
        table2.show(out);
    }

    static void showPureCommandOutput(std::ostream& out, std::shared_ptr<nlohmann::json> json) {
    }

    void ComletConfig::getTableResult(std::ostream& out) {
        auto res = run();
        if (res->contains("return")) {
            out << "Return: " << (*res)["return"].get<std::string>() << std::endl;
            return;
        } else if (res->contains("error")) {
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
} // namespace xpum::cli
