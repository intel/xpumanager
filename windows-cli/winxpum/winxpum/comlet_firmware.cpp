/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_firmware.cpp
 */

#include "comlet_firmware.h"
#include <Windows.h>

#include <igsc_lib.h>

#include <chrono>
#include <nlohmann/json.hpp>
#include <regex>
#include <thread>

#include "core_stub.h"
#include "xpum_structs.h"
#include <excpt.h>


static const std::string igscPath{"igsc"};
static const std::string igscMissingErrorInfo{"This feature requires the igsc library. Please make sure it was installed correctly."};

ComletFirmware::ComletFirmware() : ComletBase("updatefw", "Update GPU firmware.") {
    this->printHelpWhenNoArgs = true;
}

ComletFirmware::~ComletFirmware() {
}

static bool isNumber(const std::string &str) {
    return str.find_first_not_of("0123456789") == std::string::npos;
}

void ComletFirmware::setupOptions() {
    opts = std::unique_ptr<FlashFirmwareOptions>(new FlashFirmwareOptions());

    auto deviceIdOpt = addOption("-d, --device", opts->deviceId, "The device ID");
    deviceIdOpt->check([](const std::string &str) {
        std::string errStr = "Device id should be integer larger than or equal to 0";
        if (!isNumber(str))
            return errStr;
        int value;
        try {
            value = std::stoi(str);
        } catch (const std::out_of_range &oor) {
            return errStr;
        }
        if (value < 0)
            return errStr;
        return std::string();
    });

    auto fwTypeOpt = addOption("-t, --type", opts->firmwareType, "The firmware name. Valid options: GFX, GFX_DATA.");
    // fwTypeOpt->required();
    fwTypeOpt->check([](const std::string &str) {
        std::string errStr = "Invalid firmware type";
        if (str.compare("GFX") == 0 || str.compare("GFX_DATA") == 0) {
            return std::string();
        } else {
            return errStr;
        }
    });

    auto fwPathOpt = addOption("-f, --file", opts->firmwarePath, "The firmware image file path on this server");
    // fwPathOpt->required();
    fwPathOpt->transform([](const std::string &str) {
        if (FILE *file = fopen(str.c_str(), "r")) {
            fclose(file);
            // get full path of firmware image path
            std::wstring wstr(str.begin(), str.end());
            wchar_t absolutePath[MAX_PATH] = {0};
            GetFullPathName(wstr.c_str(), MAX_PATH, absolutePath, NULL);
            wstr = absolutePath;
            return std::string(wstr.begin(), wstr.end());
        } else {
            throw CLI::ValidationError("Invalid file path.");
        }
    });

    fwPathOpt->needs(fwTypeOpt);
    fwTypeOpt->needs(fwPathOpt);

    deviceIdOpt->needs(fwTypeOpt);
    deviceIdOpt->needs(fwPathOpt);

    opts->deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
    addFlag("-y, --assumeyes", opts->assumeyes, "Assume that the answer to any question which would be asked is yes");
}

nlohmann::json ComletFirmware::validateArguments() {
    nlohmann::json result;
    // GFX
    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GFX") == 0) {
        result["error"] = "Updating GFX firmware on all devices is not supported";
        return result;
    }

    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GFX_DATA") == 0) {
        result["error"] = "Updating GFX_DATA firmware on all devices is not supported";
        return result;
    }

    return result;
}

std::unique_ptr<nlohmann::json> ComletFirmware::run() {
    std::unique_ptr<nlohmann::json> json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    return json;
}

static void printJson(std::shared_ptr<nlohmann::json> json, std::ostream &out, bool raw) {
    if (raw) {
        out << json->dump() << std::endl;
        return;
    } else {
        out << json->dump(4) << std::endl;
        return;
    }
}

static int getIntFirmwareType(std::string firmwareType) {
    if (firmwareType.compare("GFX") == 0)
        return XPUM_DEVICE_FIRMWARE_GFX;
    if (firmwareType.compare("AMC") == 0)
        return XPUM_DEVICE_FIRMWARE_AMC;
    if (firmwareType.compare("GFX_DATA") == 0)
        return XPUM_DEVICE_FIRMWARE_GFX_DATA;
    return -1;
}

void ComletFirmware::getJsonResult(std::ostream &out, bool raw) {
    auto validateResultJson = validateArguments();
    if (validateResultJson.contains("error")) {
        printJson(std::make_shared<nlohmann::json>(validateResultJson), out, raw);
        return;
    }

    int type = getIntFirmwareType(opts->firmwareType);
    auto uniqueJson = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath);
    std::shared_ptr<nlohmann::json> json = std::move(uniqueJson);
    if (json->contains("error")) {
        printJson(json, out, raw);
        return;
    }
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        json = coreStub->getFirmwareFlashResult(opts->deviceId, type);
        if (json->contains("error")) {
            printJson(json, out, raw);
            return;
        }
        if (!json->contains("result")) {
            nlohmann::json tmp;
            tmp["error"] = "Failed to get firmware reuslt";
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            return;
        }

        std::string flashStatus = (*json)["result"].get<std::string>();

        if (flashStatus.compare("OK") == 0) {
            nlohmann::json tmp;
            tmp["result"] = "OK";
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            return;
        } else if (flashStatus.compare("FAILED") == 0) {
            nlohmann::json tmp;
            tmp["result"] = "FAILED";
            printJson(std::make_shared<nlohmann::json>(tmp), out, raw);
            return;
        } else {
            // do nothing
        }
    }
}

nlohmann::json ComletFirmware::getDeviceProperties(int deviceId) {
    auto json = coreStub->getDeviceProperties(deviceId);
    return *json;
}

std::string ComletFirmware::getCurrentFwVersion(nlohmann::json json) {
    std::string res = "unknown";
    int type = getIntFirmwareType(opts->firmwareType);
    if (type == XPUM_DEVICE_FIRMWARE_GFX) {
        if (!json.contains("gfx_firmware_version")) {
            return res;
        }
        return json["gfx_firmware_version"];
    } else {
        if (!json.contains("gfx_data_firmware_version")) {
            return res;
        }
        return json["gfx_data_firmware_version"];
    }
}

static std::string print_fw_version(const struct igsc_fw_version *fw_version) {
    std::stringstream ss;
    ss << fw_version->project[0];
    ss << fw_version->project[1];
    ss << fw_version->project[2];
    ss << fw_version->project[3];
    ss << "_";
    ss << fw_version->hotfix;
    ss << ".";
    ss << fw_version->build;
    return ss.str();
}

std::string ComletFirmware::getImageFwVersion() {
    __try {
        std::string version = "unknown";
        auto &buffer = imgBuffer;
        if (buffer.size() == 0) return version;

        struct igsc_fw_version fw_version;
        int ret;
        ret = igsc_image_fw_version((const uint8_t *)buffer.data(), buffer.size(), &fw_version);
        if (ret == IGSC_SUCCESS) {
            version = print_fw_version(&fw_version);
        }
        return version;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cout << igscMissingErrorInfo << std::endl;
        exit(-1);
    }
}

static std::string print_fwdata_version(const struct igsc_fwdata_version *fwdata_version) {
    std::stringstream ss;
    ss << fwdata_version->major_version;
    ss << ".";
    ss << fwdata_version->oem_manuf_data_version;
    ss << ".";
    ss << fwdata_version->major_vcn;
    return ss.str();
}

std::string ComletFirmware::getFwDataImageFwVersion() {
    __try {
        std::string version = "unknown";
        auto &buffer = imgBuffer;
        if (buffer.size() == 0) return version;

        struct igsc_fwdata_image *oimg = NULL;
        struct igsc_fwdata_version fwdata_version;
        int ret;

        ret = igsc_image_fwdata_init(&oimg, (const uint8_t *)buffer.data(), buffer.size());
        if (ret != IGSC_SUCCESS) {
            igsc_image_fwdata_release(oimg);
            return version;
        }

        ret = igsc_image_fwdata_version(oimg, &fwdata_version);
        if (ret == IGSC_SUCCESS) {
            version = print_fwdata_version(&fwdata_version);
        }
        return version;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cout << igscMissingErrorInfo << std::endl;
        exit(-1);
    }
}

bool ComletFirmware::checkImageValid() {
    __try {
        auto &buffer = imgBuffer;
        if (buffer.size() == 0) return false;
        uint8_t type;
        int ret;
        ret = igsc_image_get_type((const uint8_t *)buffer.data(), buffer.size(), &type);
        if (ret != IGSC_SUCCESS) {
            return false;
        }
        return type == IGSC_IMAGE_TYPE_GFX_FW;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cout << igscMissingErrorInfo << std::endl;
        exit(-1);
    }
}

bool ComletFirmware::validateFwDataImage() {
    __try {    
        auto &buffer = imgBuffer;
        if (buffer.size() == 0) return false;
        uint8_t type;
        int ret;
        ret = igsc_image_get_type((const uint8_t *)buffer.data(), buffer.size(), &type);
        if (ret != IGSC_SUCCESS) {
            return false;
        }
        return type == IGSC_IMAGE_TYPE_FW_DATA;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cout << igscMissingErrorInfo << std::endl;
        exit(-1);
    }
}

void ComletFirmware::getTableResult(std::ostream &out) {
    auto validateResultJson = validateArguments();
    if (validateResultJson.contains("error")) {
        out << "Error: " << validateResultJson["error"].get<std::string>() << std::endl;
        return;
    }

    // read file
    readImageContent(opts->firmwarePath.c_str());
    // warn user
    int type = getIntFirmwareType(opts->firmwareType);
        // GFX and GFX_DATA caution
    if (type == XPUM_DEVICE_FIRMWARE_GFX) {
        if (!checkImageValid()) {
            out << "Error: The image file is not a right GFX firmware image file." << std::endl;
            exit(1);
        }
    } else {
        if (!validateFwDataImage()) {
            out << "Error: The image file is not a right GFX_DATA firmware image file." << std::endl;
            exit(1);
        }
    }
    std::vector<int> deviceIdsToFlashFirmware;
    // for ats-m3
    deviceIdsToFlashFirmware = coreStub->getSiblingDevices(opts->deviceId);
    if (deviceIdsToFlashFirmware.size() == 0) {
        deviceIdsToFlashFirmware.push_back(opts->deviceId);
    } else {
        std::cout << "This GPU card has multiple cores. This operation will update all firmwares. Do you want to continue? (y/n) " << std::endl;
        if (!opts->assumeyes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                out << "update aborted" << std::endl;
                return;
            }
        }
        else {
            out << std::endl;
        }
    }
    // version confirmation
    for (int deviceId : deviceIdsToFlashFirmware) {
        auto json = getDeviceProperties(deviceId);
        if (json.contains("error")) {
            out << "Error: " << json["error"].get<std::string>() << std::endl;
            exit(1);
        }
        out << "Device " << deviceId << " FW version: " << getCurrentFwVersion(json) << std::endl;
    }
    if (type == XPUM_DEVICE_FIRMWARE_GFX) {
        out << "Image FW version: " << getImageFwVersion() << std::endl;
    } else {
        out << "Image FW version: " << getFwDataImageFwVersion() << std::endl;
    }
    out << "Do you want to continue? (y/n) " << std::endl;
    if (!opts->assumeyes) {
        std::string confirm;
        std::cin >> confirm;
        if (confirm != "Y" && confirm != "y") {
            out << "update aborted" << std::endl;
            return;
        }
    } else {
        out << std::endl;
    }
    // start run
    auto json = coreStub->runFirmwareFlash(opts->deviceId, type, opts->firmwarePath);

    auto status = (*json)["error"];
    if (!status.is_null()) {
        out << "Error: " << status.get<std::string>() << std::endl;
        return;
    }
    out << "Start to update firmware" << std::endl;
    out << "Firmware Name: " << opts->firmwareType << std::endl;
    out << "Image path: " << opts->firmwarePath << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        out << "." << std::flush;

        json = coreStub->getFirmwareFlashResult(opts->deviceId, type);
        if (json->contains("error")) {
            out << std::endl;
            out << "Error: " << (*json)["error"] << std::endl;
            return;
        }
        if (!json->contains("result")) {
            out << std::endl;
            out << "Error: Failed to get firmware reuslt" << std::endl;
            return;
        }

        std::string flashStatus = (*json)["result"].get<std::string>();

        if (flashStatus.compare("OK") == 0) {
            out << std::endl;
            out << "Update firmware successfully." << std::endl;
            return;
        } else if (flashStatus.compare("FAILED") == 0) {
            out << std::endl;
            out << "Update firmware failed" << std::endl;
            return;
        } else {
            // do nothing
        }
    }

    out << "unknown error" << std::endl;
}

void ComletFirmware::readImageContent(const char *filePath) {
    struct stat s;
    if (stat(filePath, &s) != 0 || !(s.st_mode & S_IFREG))
        return;
    std::ifstream is(std::string(filePath), std::ifstream::binary);
    if (!is) {
        return;
    }
    // get length of file:
    is.seekg(0, is.end);
    int length = is.tellg();
    is.seekg(0, is.beg);

    std::vector<char> buffer(length);

    is.read(buffer.data(), length);
    is.close();
    imgBuffer = buffer;
}