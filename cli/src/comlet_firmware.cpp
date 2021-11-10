#include "comlet_firmware.h"
#include <nlohmann/json.hpp>
#include "core_stub.h"
#include <thread>
#include <chrono>

namespace xpum::cli {
ComletFirmware::ComletFirmware() : ComletBase("fwflash", "Flash device firmware") {
}

ComletFirmware::~ComletFirmware() {
}

void ComletFirmware::setupOptions() {
    opts = std::unique_ptr<FlashFirmwareOptions>( new FlashFirmwareOptions() );
    addOption( "-d, --device", opts->deviceId, "device id" );
    addOption( "-t, --type", opts->firmwareType, "firmware type name" );
    addOption( "-f, --file", opts->firmwarePath, "firmware file location on server" );
}

std::unique_ptr<nlohmann::json> ComletFirmware::run() {
    std::unique_ptr<nlohmann::json> json = std::unique_ptr<nlohmann::json>( new nlohmann::json() );

    if ( opts->deviceId < 0 ) {
        (*json)["error"] = "invalid device id";
        return json;
    }

    if ( opts->firmwareType != "GSC" ) {
        (*json)["error"] = "invalid firmware type";
        return json;
    }

    json = coreStub->runFirmwareFlash( opts->deviceId, 0, opts->firmwarePath );
    return json;
}
} // namespace xpum::cli