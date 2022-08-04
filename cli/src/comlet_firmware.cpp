/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_firmware.cpp
 */

#include "comlet_firmware.h"

#include <chrono>
#include <nlohmann/json.hpp>
#include <regex>
#include <thread>
#include <igsc_lib.h>

#include "core_stub.h"
#include "xpum_structs.h"

namespace xpum::cli {

static const std::string igscPath{"igsc"};

ComletFirmware::ComletFirmware() : ComletBase("updatefw", "Update GPU firmware") {
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

    auto fwTypeOpt = addOption("-t, --type", opts->firmwareType, "The firmware name. Valid options: GSC, AMC, GSC_DATA. AMC firmware update just works for Intel Data Center GPU (AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later).");
    // fwTypeOpt->required();
    fwTypeOpt->check([](const std::string &str) {
        std::string errStr = "Invalid firmware type";
        if (str.compare("GSC") == 0 || str.compare("AMC") == 0 || str.compare("GSC_DATA") == 0) {
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
            char resolved_path[PATH_MAX];
            char *fullpath = realpath(str.c_str(), resolved_path);
            return std::string(fullpath);
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
    // GSC
    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GSC") == 0) {
        result["error"] = "Updating GSC firmware on all devices is not supported";
        return result;
    }

    if (opts->deviceId == XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("GSC_DATA") == 0) {
        result["error"] = "Updating GSC_DATA firmware on all devices is not supported";
        return result;
    }

    // AMC
    if (opts->deviceId != XPUM_DEVICE_ID_ALL_DEVICES && opts->firmwareType.compare("AMC") == 0) {
        result["error"] = "Updating AMC firmware on single device is not supported";
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
    if (firmwareType.compare("GSC") == 0)
        return XPUM_DEVICE_FIRMWARE_GSC;
    if (firmwareType.compare("AMC") == 0)
        return XPUM_DEVICE_FIRMWARE_AMC;
    if(firmwareType.compare("GSC_DATA") == 0)
        return XPUM_DEVICE_FIRMWARE_FW_DATA;
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
    if (type == XPUM_DEVICE_FIRMWARE_GSC) {
        if (!json.contains("firmware_version")) {
            return res;
        }
        return json["firmware_version"];
    } else {
        if (!json.contains("fw_data_firmware_version")) {
            return res;
        }
        return json["fw_data_firmware_version"];
    }
}

static std::string print_fw_version(const struct igsc_fw_version* fw_version) {
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
}

bool ComletFirmware::checkImageValid() {
    auto& buffer = imgBuffer;
    if (buffer.size() == 0) return false;
    uint8_t type;
    int ret;
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS)
    {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_GFX_FW;
}

bool ComletFirmware::validateFwDataImage(){
    auto& buffer = imgBuffer;
    if (buffer.size() == 0) return false;
    uint8_t type;
    int ret;
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS)
    {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_FW_DATA;
}

bool ComletFirmware::checkIgscExist() {
    std::string cmd = igscPath + " -V 2>&1";
    FILE *f = popen(cmd.c_str(), "r");
    char c_line[1024];
    while (fgets(c_line, 1024, f) != NULL) {
    }
    return pclose(f) == 0;
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
    if (type == 1) { // AMC caution
        std::cout << "CAUTION: it will update the AMC firmware of all cards and please make sure that you install the GPUs of the same model." << std::endl;
        std::cout << "Please comfirm to proceed ( Y/N ) ?" << std::endl;
        if (!opts->assumeyes) {
            std::string confirm;
            std::cin >> confirm;
            if (confirm != "Y" && confirm != "y") {
                out << "update aborted" << std::endl;
                return;
            }
        }
    } else { // GSC and FW-DATA caution
        // check igsc
        if (!checkIgscExist()) {
            out << "Error: Igsc tool doesn't exit." << std::endl;
            exit(1);
        }
        if (type == XPUM_DEVICE_FIRMWARE_GSC) {
            if (!checkImageValid()) {
                out << "Error: The image file is not a right SOC FW image file." << std::endl;
                exit(1);
            }
        } else {
            if (!validateFwDataImage()) {
                out << "Error: The image file is not a right FW-DATA image file." << std::endl;
                exit(1);
            }
        }
        // for ats-m3
        auto allGroups = coreStub->groupListAll();
        std::vector<int> deviceIdsToFlashFirmware;
        if (allGroups != nullptr && allGroups->contains("group_list")) {
            for (auto groupJson : (*allGroups)["group_list"]) {
                int groupId = groupJson["group_id"].get<int>();
                if (groupId & 0x80000000) {
                    auto deviceIdList = groupJson["device_id_list"];
                    for (auto deviceIdInGroup : deviceIdList) {
                        if (deviceIdInGroup.get<int>() == opts->deviceId) {
                            std::cout << "This GPU card has multiple cores. This operation will update all firmwares. Do you want to continue? (y/n) " << std::endl;
                            if (!opts->assumeyes) {
                                std::string confirm;
                                std::cin >> confirm;
                                if (confirm != "Y" && confirm != "y") {
                                    out << "update aborted" << std::endl;
                                    return;
                                }
                            }
                            for (auto tmpId : deviceIdList) {
                                deviceIdsToFlashFirmware.push_back(tmpId.get<int>());
                            }
                            break;
                        }
                    }
                    if (deviceIdsToFlashFirmware.size() > 0)
                        break;
                }
            }
        }
        if(deviceIdsToFlashFirmware.size()==0){
            deviceIdsToFlashFirmware.push_back(opts->deviceId);
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
        if (type == XPUM_DEVICE_FIRMWARE_GSC) {
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
        }
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
} // namespace xpum::cli