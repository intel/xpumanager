/* 
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_config.cpp
 */

#include <nlohmann/json.hpp>
#include <future>
#include <unordered_map>

#include "level_zero/zes_api.h"
#include "comlet_config.h"
#include "cli_table.h"
#include "core_stub.h"
#include "utility.h"
#include "exit_code.h"
#include "local_functions.h"

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
                {"rowTitle": " " },
                { "rowTitle": "Memory ECC:" },
                { "label": "  Current", "value": "memory_ecc_current_state" },
                { "label": "  Pending", "value": "memory_ecc_pending_state" },
                {"rowTitle": " " },
                { "rowTitle": "PCIe Gen4 Downgrade:" },
                { "label": "  Current", "value": "pcie_downgrade_current_state" }
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
/*
                { "label": "  Available", "value": "memory_ecc_available" },
                { "label": "  Configurable", "value": "memory_ecc_configurable" },
                { "label": "  Action", "value": "memory_ecc_pending_action" },
*/

static void printProgress(std::future<std::unique_ptr<nlohmann::json>>& future) {
    int i = 0;
    while(future.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        std::cout << ".";
        std::cout.flush();
        if(++i % 80 == 0){
            std::cout << std::endl;
        }
    }
    if(i % 80 != 0){
        std::cout << std::endl;
    }
}

void ComletConfig::setupOptions() {
    this->opts = std::unique_ptr<ComletConfigOptions>(new ComletConfigOptions());
    auto deviceIdOpt = addOption("-d,--device", this->opts->device, "The device ID or PCI BDF address to query");
    deviceIdOpt->check([this](const std::string &str) {
        std::string errStr = "Device id should be a non-negative integer or a BDF string";
        if (isValidDeviceId(str)) {
            return std::string();
        } else if (isBDF(str)) {
            return std::string();
        }
        return errStr;
    });
    addOption("-t,--tile", this->opts->tileId, "The tile ID");
    addOption("--frequencyrange", this->opts->frequencyrange, "GPU tile-level core frequency range.");
    addOption("--powerlimit", this->opts->powerlimit, "Device-level power limit.");
    addOption("--powertype", this->opts->powertype, "Device-level power limit type. Valid options: \"sustain\"; \"peak\"; \"burst\"");
    addOption("--standby", this->opts->standby, "Tile-level standby mode. Valid options: \"default\"; \"never\".");
    addOption("--scheduler", this->opts->scheduler, "Tile-level scheduler mode. Value options: \"timeout\",timeoutValue (us); \"timeslice\",interval (us),yieldtimeout (us);\"exclusive\". The valid range of all time values (us) is from 5000 to 100,000,000.");
    addFlag("--reset", this->opts->resetDevice, "Reset device by SBR (Secondary Bus Reset). For Intel(R) Max Series GPU, when SR-IOV is enabled, please add \"pci=realloc=off\" into Linux kernel command line parameters. When SR-IOV is disabled, please add \"pci=realloc=on\" into Linux kernel command line parameters.");
    auto ppr = addFlag("--ppr", this->opts->applyPPR, "Apply ppr to the device.");
    auto forceFlag = addFlag("--force", opts->forcePPR, "Force PPR to run.");
    forceFlag->needs(ppr);


    //addOption("--timeslice", this->opts->schedulerTimeslice, "set scheduler timeslice mode");
    //addOption("--timeout", this->opts->schedulerTimeout, "set scheduler timeout mode");
    //addFlag("--exclusive", this->opts->schedulerExclusive, "set scheduler exclusive mode");

    addOption("--performancefactor", this->opts->performancefactor,
              "Set the tile-level performance factor. Valid options: \"compute/media\";factorValue. The factor value should be\n\
between 0 to 100. 100 means that the workload is completely compute bounded and requires very little support from the memory support. 0 means that the workload is completely memory bounded and the performance of the memory controller needs to be increased.");
    addOption("--xelinkport", this->opts->xelinkportEnable, "Change the Xe Link port status. The value 0 means down and 1 means up.");
    addOption("--xelinkportbeaconing", this->opts->xelinkportBeaconing, "Change the Xe Link port beaconing status. The value 0 means off and 1 means on.");
    addOption("--memoryecc", this->opts->setecc,"Enable/disable memory ECC setting. 0:disable; 1:enable");
    addOption("--pciedowngrade", this->opts->setpciedown,"Enable/disable PCIe Gen4 Downgrade setting. 0:disable; 1:enable");
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
    if (this->opts->device != "") {
        if (isNumber(this->opts->device)) {
            this->opts->deviceId = std::stoi(this->opts->device);
        } else {
            auto json = this->coreStub->getDeivceIdByBDF(
                this->opts->device.c_str(), &this->opts->deviceId);
            if (json->contains("error")) {
                return json;
            }
        }
    }
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["return"] = "error";
    if (isQuery()) {
        json = this->coreStub->getDeviceConfig(this->opts->deviceId, this->opts->tileId);
        return json;
    }
    if (this->opts->deviceId >= 0) {
        if (this->opts->tileId >= 0 && !this->opts->scheduler.empty()) {
            std::vector<std::string> paralist = split(this->opts->scheduler, ",");
            int val1, val2;
            std::string command = paralist.at(0);
            std::for_each(command.begin(), command.end(), [](char &c) {
                c = ::tolower(c);
            });
            if (command.compare("timeout") == 0) {
                if (paralist.size() != 2 || paralist.at(1).empty()) {
                    (*json)["return"] = "invalid parameter: timeout";
                    return json;
                }

                try {
                    val1 = std::stoi(paralist.at(1));
                } catch (std::exception &e) {
                    (*json)["return"] = "invalid parameter: timeout";
                    return json;
                }

                if (val1 <= 0) {
                    (*json)["return"] = "invalid parameter: timeout should bigger than 0.";
                    return json;
                }

                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, 0,
                                                              val1, 0);
            } else if (command.compare("timeslice") == 0) {
                if (paralist.size() != 3 || paralist.at(1).empty() || paralist.at(2).empty()) {
                    (*json)["return"] = "invalid parameter: timeslice";
                    return json;
                }
                try {
                    val1 = std::stoi(paralist.at(1));
                    val2 = std::stoi(paralist.at(2));
                } catch (std::exception &e) {
                    (*json)["return"] = "invalid parameter: timeslice";
                    return json;
                }

                if (val1 <= 0 || val2 <= 0) {
                    (*json)["return"] = "invalid parameter: time slice should bigger than 0.";
                    return json;
                }
                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, XPUM_TIMESLICE, val1, val2);
            } else if (command.compare("exclusive") == 0) {
                if (paralist.size() != 1) {
                    (*json)["return"] = "invalid parameter: exclusive";
                    return json;
                }
                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, XPUM_EXCLUSIVE, 0, 0);
            } else if (command.compare("debug") == 0) {
                if (paralist.size() != 1) {
                    (*json)["return"] = "invalid parameter: debug";
                    return json;
                }
                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, XPUM_COMPUTE_UNIT_DEBUG, 0, 0);
            } else {
                (*json)["return"] = "invalid scheduler mode";
                return json;
            }
            if ((*json)["status"] == "OK") {
                (*json)["return"] = "Succeed to change the scheduler mode on GPU " + std::to_string(this->opts->deviceId) +
                                    " tile " + std::to_string(this->opts->tileId) + ".";
            }
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
            int mode;
            std::for_each(this->opts->standby.begin(), this->opts->standby.end(), [](char &c) {
                c = ::tolower(c);
            });
            if (this->opts->standby.compare("never") == 0) {
                mode = 1;
            } else if (this->opts->standby.compare("default") == 0) {
                mode = 0;
            } else {
                (*json)["return"] = "invalid parameter: standby mode";
                return json;
            }
            json = this->coreStub->setDeviceStandby(this->opts->deviceId, this->opts->tileId, mode);
            if ((*json)["status"] == "OK") {
                (*json)["return"] = "Succeed to change the standby mode on GPU " + std::to_string(this->opts->deviceId) +
                                    " tile " + std::to_string(this->opts->tileId) + ".";
            }
            return json;
        } else if (this->opts->tileId >= 0 && !this->opts->frequencyrange.empty()) {
            std::vector<std::string> paralist = split(this->opts->frequencyrange, ",");
            if (paralist.size() == 2 && !paralist.at(0).empty() && !paralist.at(1).empty()) {
                int val1,val2;
                try {
                    val1 = std::stoi(paralist.at(0));
                    val2 = std::stoi(paralist.at(1));
                } catch (std::exception &e) {
                    (*json)["return"] = "invalid parameter: frequency range";
                    return json;
                }

                if (val1 <= 0 || val2 <= 0) {
                    (*json)["return"] = "invalid parameter: min/max frequency should bigger than 0.";
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
            std::vector<std::string> paralist = split(this->opts->performancefactor, ",");
            if (paralist.size() != 2 || paralist.at(1).empty()) {
                (*json)["return"] = "invalid parameter: please check help information";
                return json;
            }
            std::string engine = paralist.at(0);
            std::for_each(engine.begin(), engine.end(), [](char &c) { c = ::tolower(c); });
            xpum_engine_type_flags_t engineType;

            if (engine.compare("compute") == 0) {
                engineType = XPUM_COMPUTE;
            } else if (engine.compare("media") == 0) {
                engineType = XPUM_MEDIA;
            } else {
                (*json)["return"] = "invalid engine";
                return json;
            }
            double val1 = std::stod(paralist.at(1));
            if (val1 < 0.0 || val1 > 100.0) {
                (*json)["return"] = "invalid factor";
                return json;
            }
            json = this->coreStub->setPerformanceFactor(this->opts->deviceId, this->opts->tileId, engineType, val1);
            if ((*json)["status"] == "OK") {
                (*json)["return"] = "Succeed to change the " + engine + " performance factor to " + paralist.at(1) +
                                    " on GPU " + std::to_string(this->opts->deviceId) +
                                    " tile " + std::to_string(this->opts->tileId) + ".";
            }
            return json;
        } else if (this->opts->tileId >= 0 && !this->opts->xelinkportEnable.empty()) {
            std::vector<std::string> paralist = split(this->opts->xelinkportEnable, ",");
            if (paralist.size() != 2 || paralist.at(1).empty()) {
                (*json)["return"] = "invalid parameter: please check help information";
                return json;
            }

            int port;
            int enabled;
            try {
                port = std::stoi(paralist.at(0));
                enabled = std::stoi(paralist.at(1));
            } catch (std::exception &e) {
                    (*json)["return"] = "invalid parameter: xeLink port";
                    return json;
            }

            if ((enabled != 0 && enabled != 1) || port < 0) {
                (*json)["return"] = "invalid parameter enabled";
                return json;
            }
            json = this->coreStub->setFabricPortEnabled(this->opts->deviceId, this->opts->tileId, port, enabled);
            if ((*json)["status"] == "OK") {
                (*json)["return"] = "Succeed to change Xe Link port " + paralist.at(0) + " to " + (enabled == 1 ? "up" : "down") + " .";
            }
            return json;
        } else if (this->opts->tileId >= 0 && !this->opts->xelinkportBeaconing.empty()) {
            std::vector<std::string> paralist = split(this->opts->xelinkportBeaconing, ",");
            if (paralist.size() != 2 || paralist.at(1).empty()) {
                (*json)["return"] = "invalid parameter: please check help information";
                return json;
            }

            int port;
            int beaconing;
            try {
                port = std::stoi(paralist.at(0));
                beaconing = std::stoi(paralist.at(1));
            } catch (std::exception &e) {
                    (*json)["return"] = "invalid parameter: xeLink beaconing";
                    return json;
            }

            if (beaconing != 0 && beaconing != 1) {
                (*json)["return"] = "invalid parameter value: beaconing";
                return json;
            }
            json = this->coreStub->setFabricPortBeaconing(this->opts->deviceId, this->opts->tileId, port, beaconing);
            if ((*json)["status"] == "OK") {
                (*json)["return"] = "Succeed to change Xe Link port " + paralist.at(0) + " beaconing to " + (beaconing == 1 ? "on" : "off") + " .";
            }
            return json;
        }
        else if (!this->opts->setecc.empty()) {
            bool enabled = false;
            int eccVal;
            if (this->opts->setecc.length() == 1 &&
                (this->opts->setecc[0] == '0' || this->opts->setecc[0] == '1')) {
                eccVal = std::stoi(this->opts->setecc);
            } else {
                (*json)["return"]="invalid parameter value";
                return json;
            }
            if (eccVal == 1) {
                enabled = true;
            } else if (eccVal == 0) {
                enabled = false;
            } else {
                (*json)["return"]="invalid parameter value";
                return json;    
            }
            json = this->coreStub->setMemoryEccState(this->opts->deviceId, enabled);
            if((*json)["status"] == "OK") {
                (*json)["return"] = "Successfully " + (enabled?std::string("enable"): std::string("disable")) + " ECC memory on GPU " + std::to_string(this->opts->deviceId)+". Please reset the GPU or reboot the OS for the change to take effect.";
            }
            return json;  
        }
#ifdef DAEMONLESS
        else if (!this->opts->setpciedown.empty()) {
            bool enabled = false;
            int pciedownVal;
            if (this->opts->setpciedown.length() == 1 &&
                (this->opts->setpciedown[0] == '0' || this->opts->setpciedown[0] == '1')) {
                pciedownVal = std::stoi(this->opts->setpciedown);
            } else {
                (*json)["return"]="invalid parameter value";
                return json;
            }
            if (pciedownVal == 1) {
                enabled = true;
            } else if (pciedownVal == 0) {
                enabled = false;
            } else {
                (*json)["return"]="invalid parameter value";
                return json;
            }
            json = this->coreStub->setPCIeDowngradeState(this->opts->deviceId, enabled);
            if((*json)["status"] == "OK") {
                std::string pendingAction = (*json)["pcie_downgrade_pending_action"];
                std::string ret = "Successfully " + (enabled?std::string("enable"): std::string("disable")) + " PCIe Gen4 Downgrade on GPU " + std::to_string(this->opts->deviceId);
                if (pendingAction.compare("none") == 0) {
                    ret += ".";
                } else {
                    ret += ". Please hard reset or power on/off the machine for the change to take effect!";
                }
		(*json)["return"] = ret;
            }
            return json;
        }
#endif
        else if (this->opts->tileId == -1 && this->opts->resetDevice) {
            
            if (this->opts->deviceId >= 0) {
                json = this->coreStub->resetDevice(this->opts->deviceId, true); 
#if 0
                char confirmed;
                json = this->coreStub->getDeviceProcessState(this->opts->deviceId);
                std::cout <<"The process(es) below are using this device."<<"\n";

                for(auto it= (*json)["device_process_list"].begin(); it!=(*json)["device_process_list"].end();++it) {
                    std::cout <<"PID: "<<(*it)["process_id"] <<" ,";
                    std::cout <<" Command: "<<(*it)["process_name"];
                    std::cout <<"\n";
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
#endif
            }
            if((*json)["status"] == "OK") {
                //json->clear();
                (*json)["return"] = "Succeed to reset the GPU "+ std::to_string(this->opts->deviceId);
            }
            return json;
        } else if (this->opts->tileId == -1 && this->opts->applyPPR) {

            auto futureRes = std::async(std::launch::async, &CoreStub::applyPPR, this->coreStub, this->opts->deviceId, this->opts->forcePPR);
            printProgress(futureRes);
            json = futureRes.get();

            if ((*json)["status"] == "OK") {
                if (json->contains("ppr_diag_result")) {
                    (*json)["return"] = "PPR has been successfully applied to the GPU "+ std::to_string(this->opts->deviceId) + "." +
                    "\nPPR diag result: " + (*json)["ppr_diag_result"].get<std::string>() +
                    "\nPPR diag result description: " + (*json)["ppr_diag_result_string"].get<std::string>() +
                    "\nGPU " + std::to_string(this->opts->deviceId) + " memory status: " + (*json)["memory_health_result"].get<std::string>() +
                    "\nDescription: " + (*json)["memory_health_result_string"].get<std::string>();
                } else {
                    std::string memoryState = (*json)["memory_health_result"].get<std::string>();
                    std::string explainStr = "";
                    if(memoryState.find("OK") != std::string::npos) {
                        explainStr = " PPR doesn't need to be run. If you must run PPR, please add the parameter --force.";
                    } else if(memoryState.find("Critical") != std::string::npos) {
                        explainStr = " PPR can't be Applied to this device. The card should be replaced. If you must run PPR, please add the parameter --force.";
                    } else if(memoryState.find("Unknown") != std::string::npos) {
                        explainStr = " Not sure if PPR can be applied. If you must run PPR, please add the parameter --force.";
                    }

                    (*json)["return"] = "The memory status of GPU " + std::to_string(this->opts->deviceId) + " is " + memoryState + "." + explainStr;
                }
            }
            return json;
        }
        (*json)["return"] = "unknown or invalid command, parameter or device/tile Id";
        return json;
    }
    (*json)["return"] = "invalid device Id";
    return json;
}

static void showConfigurations(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
    CharTable table1(ComletDeviceConfiguration, *json);
    CharTable table2(ComletTileConfiguration, *json, true);
    table1.show(out);
    table2.show(out);
}

static void showPureCommandOutput(std::ostream &out, std::shared_ptr<nlohmann::json> json) {
}

void ComletConfig::getTableResult(std::ostream &out) {
    if (this->opts->resetDevice || this->opts->applyPPR) {
        if (this->opts->device != "") {
            if (isNumber(this->opts->device)) {
                this->opts->deviceId = std::stoi(this->opts->device);
            } else {
                auto json = this->coreStub->getDeivceIdByBDF(
                    this->opts->device.c_str(), &this->opts->deviceId);
                if (json->contains("error")) {
                    out << "Error: " << (*json)["error"].get<std::string>() << std::endl;
                    setExitCodeByJson(*json);
                    return;
                }
            }
        }
        if (this->opts->resetDevice && this->opts->deviceId >= 0 && this->opts->tileId == -1) {
            out << "It may take one minute to reset GPU " << this->opts->deviceId << ". Please wait ..." << std::endl;
        } else if (this->opts->applyPPR && this->opts->deviceId >= 0 && this->opts->tileId == -1){
            out << "It may take ten minutes to apply PPR to GPU " << this->opts->deviceId << ". Please wait ..." << std::endl;
        }
    }
    auto res = run();
#ifndef DAEMONLESS
    if (this->opts->deviceId >= 0 && this->opts->tileId == -1 && this->opts->resetDevice) {
        out << "Resetting GPU will make XPUM daemon not work." << std::endl;
        out << "Please restart XPU Manager daemon: sudo systemctl restart xpum." << std::endl;
    }
    if (this->opts->deviceId >= 0 && this->opts->tileId == -1 && this->opts->applyPPR && res->contains("ppr_diag_result")) {
        out << "Apply PPR will make XPUM daemon not work." << std::endl;
        out << "Please restart XPU Manager daemon: sudo systemctl restart xpum." << std::endl;
    }

#endif
    if (res->contains("return")) {
        out << "Return: " << (*res)["return"].get<std::string>() << std::endl;
        if ((res->contains("status") == false) || 
            ((*res)["status"] != "OK" && (*res)["status"] != "CANCEL")) {
            exit_code = XPUM_CLI_ERROR_BAD_ARGUMENT;
        }
        return;
    } else if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;

    if (isQuery()) {
        // Hard code default power limit (0) for ATS-M1 and ATS-M3 per
        // a customer request
        if ((*json)["power_limit"] == 0) {
            auto props = this->coreStub->getDeviceProperties(
                this->opts->deviceId);
            if (props->contains("error") == false) {
                std::string pciId = (*props)["pci_device_id"];
                if (isATSM1(pciId) == true) {
                    (*json)["power_limit"] = 120;
                }
                if (isATSM3(pciId) == true) {
                    (*json)["power_limit"] = 25;
                }
                if (isSG1(pciId) == true) {
                    (*json)["power_limit"] = 25;
                }
            }
        }
        showConfigurations(out, json);
    } else {
        showPureCommandOutput(out, json);
    }
}

bool ComletConfig::getResetOption(){
    return this->opts->resetDevice;
}
} // end namespace xpum::cli
