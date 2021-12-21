#include "comlet_firmware.h"
#include <nlohmann/json.hpp>
#include "core_stub.h"
#include <thread>
#include <chrono>

namespace xpum::cli {
ComletFirmware::ComletFirmware() : ComletBase("updatefw", "Update GPU firmware") {
}

ComletFirmware::~ComletFirmware() {
}

void ComletFirmware::setupOptions() {
    opts = std::unique_ptr<FlashFirmwareOptions>( new FlashFirmwareOptions() );
    addOption( "-d, --device", opts->deviceId, "device ID" );
    addOption( "-t, --type", opts->firmwareType, "The firmware name. Valid options: GSC, AMC. AMC firmware update just works for ATS-P card so far" );
    addOption( "-f, --file", opts->firmwarePath, "The firmware image file path on this server" );
}

std::unique_ptr<nlohmann::json> ComletFirmware::run() {
    std::unique_ptr<nlohmann::json> json = std::unique_ptr<nlohmann::json>( new nlohmann::json() );

    if ( opts->deviceId < 0 ) {
        (*json)["error"] = "invalid device id";
        return json;
    }

    if (opts->firmwareType != "GSC" && opts->firmwareType != "AMC") {
        (*json)["error"] = "invalid firmware type";
        return json;
    }

    int type = opts->firmwareType == "GSC" ? 0 : 1;
    if ( type == 1 ) {
        std::cout << "CAUTION: update AMC may cause OS reboot" << std::endl;
        std::cout << "Please comfirm to proceed ( Y/N ) ?" << std::endl;
        std::string confirm;
        std::cin >> confirm;
        if (confirm != "Y" && confirm != "y") {
            (*json)["error"] = "update aborted";
            return json;
        }
    }
    std::cout << "Start to update firmware" << std::endl;
    std::cout << "Firmware Type: " << opts->firmwareType << std::endl;
    std::cout << "Image path: " << opts->firmwarePath << std::endl;

    json = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath);
    return json;
}

void ComletFirmware::getTableResult(std::ostream &out) {
    /*
    std::string json = this->run()->dump();
    if (json.find("OK") != std::string::npos) {
        out << "Update firmware successfully" << std::endl;
    } else if (json.find("update aborted") != std::string::npos) {
        out << "Update firmware aborted" << std::endl;
    } else {
        out << "Update firmware failed" << std::endl;
    }
    */

    auto json = this->run();
    auto status = (*json)["error"];
    if (!status.is_null()) {
        out << status.get<std::string>() << std::endl;
        return;
    }

    status = (*json)["firmware_flash_result"];
    if (!status.is_null()) {
        if (status.get<std::string>().find("OK") != std::string::npos) {
            out << "Update firmware successfully" << std::endl;
        } else {
            out << "Update firmware failed" << std::endl;
        }
        return;
    }

    out << "unknown error" << std::endl;
}
} // namespace xpum::cli