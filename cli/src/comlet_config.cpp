#include "comlet_config.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"

namespace xpum::cli {


void ComletConfig::setupOptions() {
    this->opts = std::unique_ptr<ComletConfigOptions>(new ComletConfigOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
    addOption("-t,--tile", this->opts->tileId, "tile id");
    addOption("--scheduler", this->opts->scheduler, "set scheduler mode. Valid options: \"timeout\",timeoutValue(us); \"timeslice\",interval(us),yieldtimeout(us);\"exclusive\".");
    //addOption("--timeslice", this->opts->schedulerTimeslice, "set scheduler timeslice mode");
    //addOption("--timeout", this->opts->schedulerTimeout, "set scheduler timeout mode");
    //addFlag("--exclusive", this->opts->schedulerExclusive, "set scheduler exclusive mode");
    addOption("--powerlimit", this->opts->powerlimit, "set powerlimit");// --
    addOption("--performancefactor", this->opts->performancefactor, "Set the performance factor.\
    Valid options: \"compute/media\",factorValue. The factor value should be \
    between 0 to 100. 100 means that the workload is completely compute bounded and requires very little support from the memory support.\
    0 means that the workload is completely memory bouded and the performance of the memory controller needs to be increased.");
    addOption("--standby", this->opts->standby, "set standby mode. Valid options: \"default\", \"never\"");
    addOption("--frequencyrange", this->opts->frequencyrange, "set core frequencyrange");
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
    if (this->opts->deviceId >= 0 
        && this->opts->scheduler.empty()
        && this->opts->performancefactor.empty()
        && this->opts->powerlimit.empty()
        && this->opts->standby.empty()
        && this->opts->frequencyrange.empty()
        ) {
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
            }
            if (command.compare("timeslice") == 0) {
                if (paralist.size() != 3 || paralist.at(1).empty() || paralist.at(2).empty()) {
                   (*json)["return"]="invalid parameter";
                   return json; 
                }
                val1 = std::stoi(paralist.at(1));
                val2 = std::stoi(paralist.at(2));
                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, SCHEDULER_TIMESLICE, val1, val2);
            }
            if (command.compare("exclusive") == 0) {
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
                return json;
                if((*json)["status"] == "OK") {
                     (*json)["return"] = "Succeed to set the power limit on GPU " + std::to_string(this->opts->deviceId) +
                    " tile " + std::to_string(this->opts->tileId) + ".";
                }
            }else {
                (*json)["return"]="invalid parameter";
                return json;
            }
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
        }
        (*json)["return"]="unknonw or invalid command, parameter or device/tile Id";
        return json;
    }
    (*json)["return"]="invalid device Id";
    return json;
}
} // end namespace xpum::cli
