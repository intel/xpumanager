#include "comlet_config.h"

#include <nlohmann/json.hpp>

#include "core_stub.h"

namespace xpum::cli {


void ComletConfig::setupOptions() {
    this->opts = std::unique_ptr<ComletConfigOptions>(new ComletConfigOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
    addOption("-t,--tile", this->opts->tileId, "tile id");
    addOption("--timeslice", this->opts->schedulerTimeslice, "set scheduler timeslice mode");
    addOption("--timeout", this->opts->schedulerTimeout, "set scheduler timeout mode");
    addFlag("--exclusive", this->opts->schedulerExclusive, "set scheduler exclusive mode");
    addOption("-p,--powerlimit", this->opts->powerlimit, "set powerlimit");
    addOption("-b,--standby", this->opts->standby, "set standby mode");
    addOption("-f,--frequencyrange", this->opts->frequencyrange, "set frequencyrange");
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
    if (this->opts->deviceId >= 0 
        && this->opts->schedulerTimeout.empty()
        && this->opts->schedulerTimeslice.empty()
        && !this->opts->schedulerExclusive
        && this->opts->powerlimit.empty()
        && this->opts->standby.empty()
        && this->opts->frequencyrange.empty()
        ) {
        json = this->coreStub->getDeviceConfig(this->opts->deviceId, this->opts->tileId);
        return json;
    }
    if (this->opts->deviceId >= 0) {
        if (this->opts->tileId >= 0 && !this->opts->schedulerTimeout.empty()) {            
            json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, SCHEDULER_TIMEOUT,
            std::stoi(this->opts->schedulerTimeout ),0);
            return json;
        } else if (this->opts->tileId >= 0 && !this->opts->schedulerTimeslice.empty()) {
            std::vector<std::string> paralist = split(this->opts->schedulerTimeslice, ",");
            if (paralist.size() >=2 && !paralist.at(0).empty() && !paralist.at(1).empty()) {
                int val1 = std::stoi(paralist.at(0));
                int val2 = std::stoi(paralist.at(1));
                json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, SCHEDULER_TIMESLICE, val1, val2);
                return json;
            }else {
                return json;
            }
        } else if (this->opts->tileId >= 0 && this->opts->schedulerExclusive) {
            json = this->coreStub->setDeviceSchedulerMode(this->opts->deviceId, this->opts->tileId, SCHEDULER_EXCLUSIVE, 0, 0);
            return json;
        } else if (!this->opts->powerlimit.empty()) {
            std::vector<std::string> paralist = split(this->opts->powerlimit, ",");
            if (paralist.size() >=2 && !paralist.at(0).empty() && !paralist.at(1).empty()) {
                int val1 = std::stoi(paralist.at(0));
                int val2 = std::stoi(paralist.at(1));
                json = this->coreStub->setDevicePowerlimit(this->opts->deviceId, val1, val2);
                return json;
            }else {
                return json;
            }
        } else if (this->opts->tileId >= 0 && !this->opts->standby.empty()) {
            XpumStandbyMode mode;
            if (this->opts->standby.compare("never") == 0) {
                mode = STANDBY_NEVER;
            } else if (this->opts->standby.compare("default") == 0) {
                mode = STANDBY_DEFAULT;
            } else {
                return json;    
            }
            json = this->coreStub->setDeviceStandby(this->opts->deviceId, this->opts->tileId, mode);
            return json;
        } else if (this->opts->tileId >= 0 && !this->opts->frequencyrange.empty()) {
            std::vector<std::string> paralist = split(this->opts->frequencyrange, ",");
            if (paralist.size() >= 2 && !paralist.at(0).empty() && !paralist.at(1).empty()) {
                int val1 = std::stoi(paralist.at(0));
                int val2 = std::stoi(paralist.at(1));
                json = this->coreStub->setDeviceFrequencyRange(this->opts->deviceId, this->opts->tileId, val1, val2);
                return json;
            }else {
                return json;
            }
        }
        return json;
    }
    return json;
}
} // end namespace xpum::cli
