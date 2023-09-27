/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware_stub.cpp
 */

#include "lib_core_stub.h"
#include "xpum_api.h"
#include "exit_code.h"
#include "logger.h"
#include "redfish_amc_manager.h"
#include <fstream>

namespace xpum::cli {

static std::string getFirmwareName(unsigned int firmwareType) {
    switch(firmwareType){
        case XPUM_DEVICE_FIRMWARE_GFX:
            return "GFX";
        case XPUM_DEVICE_FIRMWARE_AMC:
            return "AMC";
        case XPUM_DEVICE_FIRMWARE_GFX_DATA:
            return "GFX_DATA";
        case XPUM_DEVICE_FIRMWARE_GFX_PSCBIN:
            return "GFX_PSCBIN";
        case XPUM_DEVICE_FIRMWARE_GFX_CODE_DATA:
            return "GFX_CODE_DATA";
        default:
            return "UNKOWN";
    }
}

static std::string getFlashFwErrMsg() {
    // get error message
    int count;
    xpumGetFirmwareFlashErrorMsg(nullptr, &count);
    char buffer[count];
    xpumGetFirmwareFlashErrorMsg(buffer, &count);
    return std::string(buffer);
}

std::unique_ptr<nlohmann::json> LibCoreStub::runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath, std::string username, std::string password, bool force) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    xpum_firmware_flash_job job;
    job.type = (xpum_firmware_type_t)type;
    job.filePath = filePath.c_str();

    std::string fwName = getFirmwareName(type);
    XPUM_LOG_AUDIT("Try to update %s FW on device %d with image %s", fwName.c_str(), deviceId, filePath.c_str());

    auto res = xpumRunFirmwareFlashEx(deviceId, &job, username.c_str(), password.c_str(), force);

    std::string errorMsg = getFlashFwErrMsg();

    if (errorMsg.size()) {
        (*json)["error"] = errorMsg;
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }

    if (res == xpum_result_t::XPUM_OK) {
        (*json)["result"] = "OK";
    } else {
        switch (res) {
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC:
                (*json)["error"] = "Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_MODEL_INCONSISTENCE:
                (*json)["error"] = "Device models are inconsistent, failed to upgrade all.";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_IMAGE_FILE_NOT_FOUND:
                (*json)["error"] = "Firmware image not found.";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND:
                (*json)["error"] = "Igsc tool doesn't exit";
                break;
            case xpum_result_t::XPUM_RESULT_DEVICE_NOT_FOUND:
                (*json)["error"] = "Device not found.";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_ALL:
                if (type == XPUM_DEVICE_FIRMWARE_GFX)
                    (*json)["error"] = "Updating GFX firmware on all devices is not supported";
                else if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA)
                    (*json)["error"] = "Updating GFX_DATA firmware on all devices is not supported";
                else if (type == XPUM_DEVICE_FIRMWARE_GFX_CODE_DATA)
                    (*json)["error"] = "Updating GFX_CODE_DATA firmware on all devices is not supported";
                else
                    (*json)["error"] = "Updating GFX_PSCBIN firmware on all devices is not supported";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC_SINGLE:
                (*json)["error"] = "Updating AMC firmware on single device is not supported";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING:
                (*json)["error"] = "Firmware update task already running.";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE:
                (*json)["error"] = "The image file is not a right FW image file.";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE:
                (*json)["error"] = "The image file is a right FW image file, but not proper for the target GPU.";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_DATA:
                (*json)["error"] = "The device doesn't support GFX_DATA firmware update";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC:
                (*json)["error"] = "The device doesn't support PSCBIN firmware update";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC_IGSC:
                (*json)["error"] = "Installed igsc doesn't support PSCBIN firmware update";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_GFX_CODE_DATA:
                (*json)["error"] = "The device doesn't support GFX_CODE_DATA firmware update";
                break;
            case xpum_result_t::XPUM_UPDATE_FIRMWARE_GFX_DATA_IMAGE_VERSION_LOWER_OR_EQUAL_TO_DEVICE:
                (*json)["error"] = "The GFX_DATA version of the image is less than or equal to the device";
                break;
            default:
                (*json)["error"] = "Unknown error.";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
    }

    return json;
}

std::unique_ptr<nlohmann::json> LibCoreStub::getFirmwareFlashResult(int deviceId, unsigned int type) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    xpum_firmware_flash_task_result_t result;

    xpum_result_t res = xpumGetFirmwareFlashResult(deviceId, (xpum_firmware_type_t)type, &result);

    std::string errorMsg = getFlashFwErrMsg();

    if (res != XPUM_OK) {
        switch (res) {
            case XPUM_LEVEL_ZERO_INITIALIZATION_ERROR:
                (*json)["error"] = "Level Zero Initialization Error";
                break;
            case XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC:
                (*json)["error"] = "Can't find the AMC device. AMC firmware update just works for ATS-P or ATS-M card (ATS-P AMC firmware version is 3.3.0 or later. ATS-M AMC firmware version is 3.6.3 or later) on Intel M50CYP server (BMC firmware version is 2.82 or later) so far.";
                break;
            default:
                (*json)["error"] = "Fail to get firmware flash result.";
                break;
        }
        (*json)["errno"] = errorNumTranslate(res);
        return json;
    }

    if (errorMsg.size()) {
        (*json)["error"] = errorMsg;
        (*json)["errno"] = XPUM_CLI_ERROR_UPDATE_FIRMWARE_FAIL;
        return json;
    }

    (*json)["percentage"] = result.percentage;

    switch (result.result) {
        case XPUM_DEVICE_FIRMWARE_FLASH_OK:
            (*json)["result"] = "OK";
            break;
        case XPUM_DEVICE_FIRMWARE_FLASH_ERROR:
            (*json)["result"] = "FAILED";
            break;
        case XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED:
            (*json)["result"] = "UNSUPPORTED";
            break;
        default:
            (*json)["result"] = "ONGOING";
            break;
    }

    return json;
}

static bool hasCdcEem() {
    std::string cmd = "modinfo cdc_eem 2>&1";
    FILE* f = popen(cmd.c_str(), "r");
    if (f)
        return pclose(f) == 0;
    return false;
}

static bool isCentOS() {
    std::string line;
    std::ifstream f("/etc/centos-release");
    if (f.is_open()) {
        getline(f, line);
        f.close();
        if (line.find("CentOS Stream release 9") != line.npos || line.find("CentOS Stream release 8") != line.npos)
            return true;
    }
    return false;
}

std::string LibCoreStub::getRedfishAmcWarnMsg() {
    if (isCentOS() && !hasCdcEem())
        return "";
    return getRedfishAmcWarn();
}

std::unique_ptr<nlohmann::json> LibCoreStub::getSensorReading() {
    return std::unique_ptr<nlohmann::json>(new nlohmann::json());
}

} // namespace xpum::cli