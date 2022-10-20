/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_firmware.h
 */

#pragma once

#include <climits>
#include <nlohmann/json.hpp>
#include <string>

#include "comlet_base.h"

struct FlashFirmwareOptions {
    int deviceId;
    std::string firmwareType;
    std::string firmwarePath;

    /*
        FlashFirmwareOptions( unsigned int id, const std::string& type, const std::string& path )
            : deviceId( id ), firmwarePath( path ) {
            if ( type == "GSC" ) {
                firmwareType = 0;
            }
            else {
                firmwareType = 0;
            }
        }
        */
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

   private:
    std::unique_ptr<FlashFirmwareOptions> opts;
    std::vector<char> imgBuffer;

    std::string getCurrentFwVersion(nlohmann::json json);
    std::string getImageFwVersion();
    bool checkImageValid();
    bool validateFwDataImage();
    std::string getFwDataImageFwVersion();
    nlohmann::json getDeviceProperties(int deviceId);

    void readImageContent(const char *filePath);
};