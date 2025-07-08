/* 
 *  Copyright (C) 2021-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_firmware.h
 */

#pragma once

#include <climits>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

namespace xpum::cli {
struct FlashFirmwareOptions {
    std::string deviceId = "";
    std::string firmwareType;
    std::string firmwarePath;

    std::string username = "";
    std::string password = "";

    bool assumeyes;

    bool forceUpdate = false;

    bool recovery = false;
};

class ComletFirmware : public ComletBase {
   public:
    ComletFirmware();
    virtual ~ComletFirmware();

    virtual void setupOptions() override;
    virtual std::unique_ptr<nlohmann::json> run() override;
    virtual void getJsonResult(std::ostream &out, bool raw = false) override;
    virtual void getTableResult(std::ostream &out) override;

    nlohmann::json validateArguments();

    inline std::string getFirmwareType(){
        return opts->firmwareType;
    }

    bool isRecovery();

    bool assumeYes() {
        return opts->assumeyes;
    }

   private:
    std::unique_ptr<FlashFirmwareOptions> opts;
    std::vector<char> imgBuffer;

    std::string getCurrentFwVersion(nlohmann::json json);
    std::string getImageFwVersion();
    bool checkIgscExist();
    bool checkImageValid();
    bool validateFwDataImage();
    std::string getFwDataImageFwVersion();
    std::string getPSCImageFwVersion();
    nlohmann::json getDeviceProperties(int deviceId);
    int deviceIdStrToInt(std::string deviceId);

    void readImageContent(const char* filePath);
};
} // namespace xpum::cli