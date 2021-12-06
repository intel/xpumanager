#include "comlet_reset.h"
#include "cli_wrapper.h"
#include <iostream>
#include <nlohmann/json.hpp>

#include "core_stub.h"

namespace xpum::cli {


void ComletReset::setupOptions() {
    this->opts = std::unique_ptr<ComletResetOptions>(new ComletResetOptions());
    addOption("-d,--device", this->opts->deviceId, "device id");
}

std::unique_ptr<nlohmann::json> ComletReset::run() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    char confirmed;
    (*json)["return"]="error";
    if (this->opts->deviceId >= 0) {
        json = this->coreStub->getDeviceProcessState(this->opts->deviceId);
        std::cout <<"Current client process(es): "<<"\n";
        std::cout << json->dump(4) <<"\n";
        std::cout <<"Please confirm whether to reset GPU? y or n :";
        std::cin>>confirmed;
        if (std::tolower(confirmed) == 'y') {
            json = this->coreStub->resetDevice(this->opts->deviceId, true); 
        } else {
            json->clear();
            (*json)["status"] = "OK";
            (*json)["return"] = "Reset is cancelled";
        }
    }
    return json;
}
} // end namespace xpum::cli
