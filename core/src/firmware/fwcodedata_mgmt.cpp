
/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fwcodedata_mgmt.cpp
 */
#include "fwcodedata_mgmt.h"

#include <vector>
#include <fstream> 
#include <chrono>
#include <regex>
#include <igsc_lib.h>
#include <string>

#include "infrastructure/logger.h"
#include "system_cmd.h"
#include "firmware_manager.h"
#include "igsc_err_msg.h"
#include "core/core.h"
#include "fwdata_mgmt.h"

namespace xpum {

using namespace std::chrono_literals;

static bool findFileInDir(std::string dirPath, std::regex pattern, std::string &filePath){
    DIR* dir = opendir(dirPath.c_str());
    struct dirent* ent;
    if (nullptr != dir) {
        while ((ent = readdir(dir)) != nullptr) {
            if(std::regex_search(ent->d_name, pattern)){
                filePath = dirPath + "/" + ent->d_name;
                closedir(dir);
                return true;
            }
        }
        closedir(dir);
    }
    return false;
}

static std::string findSubDir(const char* dirPath, const char* sudDirName){
    std::string path;
    DIR* dir;
    struct dirent* ent;
    if ((dir = opendir(dirPath)) == nullptr)
        return "";
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.' || strcmp(ent->d_name, "..") == 0) {
                continue;
        }
        if (ent->d_type == DT_DIR) {
            std::string fullPath = std::string(dirPath) + "/" + std::string(ent->d_name);
            if (strcmp(ent->d_name, sudDirName) == 0){
                path = fullPath;
                break;
            } else {
                path = findSubDir(fullPath.c_str(), sudDirName);
                if (path != "")
                    break;
            }
        }
    }
    closedir(dir);
    return path;
}

bool unpackAndGetImagePath(const char* filePath, const char* dirName, int eccState, std::string &codeImagePath, std::string &dataImagePath){
    std::string unpack_cmd = "unzip -q -o " + std::string(filePath) + " -d " + std::string(dirName);
    int status = std::system(unpack_cmd.c_str());
    if (status != 0)
        return false;
    //check if follow the standard format
    std::string eccStateStr = (eccState == 1) ? "ECC_ON" : "ECC_OFF";
    std::string dirPath = findSubDir(dirName, eccStateStr.c_str());
    if (dirPath.empty())
        return false;
    
    std::regex codePattern(".*gfx_fwupdate.*\\.bin");
    if (!findFileInDir(dirPath, codePattern, codeImagePath)) {
        return false;
    }
    std::regex dataPattern(".*DataUpdate_"+ eccStateStr + ".*\\.bin");
    if (!findFileInDir(dirPath, dataPattern, dataImagePath)) {
        return false;
    }
    return true;
}

bool removeDir(const char* dirPath){
    DIR* dir;
    struct dirent* ent;
    bool success = true;
    if ((dir = opendir(dirPath)) == nullptr)
        return success;
    while ((ent = readdir(dir)) != nullptr) {
        if (ent->d_name[0] == '.' || strcmp(ent->d_name, "..") == 0)
            continue;
        if (ent->d_type == DT_DIR) {
            std::string subDir = std::string(dirPath) + "/" + std::string(ent->d_name);
            if (!removeDir(subDir.c_str())) {
                success = false;
            }
        } else if (ent->d_type == DT_REG) {
            std::string filePath = std::string(dirPath) + "/" + std::string(ent->d_name);
            if (unlink(filePath.c_str()) != 0) {
                success = false;
            }
        }
    }
    closedir(dir);
    if (rmdir(dirPath) != 0) {
        success = false;
    }
    return success;
}

static bool validateImageFormat(std::vector<char>& buffer){
    uint8_t type;
    int ret;
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS)
    {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_FW_DATA;
}

static bool isGscFwImage(std::vector<char>& buffer) {
    uint8_t type;
    int ret;
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS)
    {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_GFX_FW;
}

xpum_result_t FwCodeDataMgmt::flashFwCodeData(FlashFwCodeDataParam &param) {
    std::lock_guard<std::mutex> lck(mtx);
    auto& deviceId = param.deviceId;
    auto& codeImagePath = param.codeImagePath;
    auto& dataImagePath = param.dataImagePath;
    if (taskFwCodeData.valid()) {
        // task already running
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        // check GFX fw_status
        auto fw_status = Core::instance().getFirmwareManager()->getGfxFwStatus(deviceId);
        if (fw_status != gfx_fw_status::GfxFwStatus::NORMAL) {
            flashFwErrMsg = "Fail to flash, GFX firmware status is " + Core::instance().getFirmwareManager()->transGfxFwStatusToString(fw_status);
            return XPUM_GENERIC_ERROR;
        }
        // read code image file
        auto codeImageBuffer = readImageContent(codeImagePath.c_str());
        // validate the code image file
        if (!isGscFwImage(codeImageBuffer)) {
            return XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
        }

        // read data image file
        auto dataImg = readImageContent(dataImagePath.c_str());
        if (!validateImageFormat(dataImg)) {
            return XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
        }

        auto res = isFwDataImageAndDeviceCompatible(dataImg, devicePath);
        if (res == XPUM_OK) {
            XPUM_LOG_DEBUG("isNeedUpdateData");
            isNeedUpdateData = true;
        } else {
            XPUM_LOG_DEBUG("not NeedUpdateData");
        }

        percent.store(0);

        taskFwCodeData = std::async(std::launch::async, [this, deviceId, codeImagePath, dataImagePath] {
            XPUM_LOG_INFO("Start update GSC FW-CODE-DATA on device {}", devicePath);
            xpum_result_t res;
            res = Core::instance().getFirmwareManager()->runGSCFirmwareFlash(deviceId, codeImagePath.c_str());
            if (res != XPUM_OK){
                flashFwErrMsg = Core::instance().getFirmwareManager()->getFlashFwErrMsg();
                removeDir(tmpUnpackPath.c_str());
                return XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            xpum_firmware_flash_task_result_t result;
            Core::instance().getFirmwareManager()->getGSCFirmwareFlashResult(deviceId, &result);
            while (result.result == XPUM_DEVICE_FIRMWARE_FLASH_ONGOING) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                Core::instance().getFirmwareManager()->getGSCFirmwareFlashResult(deviceId, &result);
                if (!isNeedUpdateData){
                    if (result.percentage > this->percent.load())
                        this->percent.store(result.percentage);
                } else {
                    if (result.percentage/2 > this->percent.load())
                        this->percent.store(result.percentage/2);
                }
            }
            if (result.result != XPUM_DEVICE_FIRMWARE_FLASH_OK || !isNeedUpdateData) {
                flashFwErrMsg = Core::instance().getFirmwareManager()->getFlashFwErrMsg();
                removeDir(tmpUnpackPath.c_str());
                return result.result;
            }

            res = Core::instance().getFirmwareManager()->runFwDataFlash(deviceId, dataImagePath.c_str());
            if (res != XPUM_OK){
                if (!Core::instance().getFirmwareManager()->getFlashFwErrMsg().empty()) {
                    flashFwErrMsg = "Update GFX_CODE succeed. " + Core::instance().getFirmwareManager()->getFlashFwErrMsg();
                }
                removeDir(tmpUnpackPath.c_str());
                return XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            Core::instance().getFirmwareManager()->getFwDataFlashResult(deviceId, &result);
            while (result.result == XPUM_DEVICE_FIRMWARE_FLASH_ONGOING) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                Core::instance().getFirmwareManager()->getFwDataFlashResult(deviceId, &result);
                if ((result.percentage/2 + 50) > this->percent.load())
                    this->percent.store(result.percentage/2 + 50);
            }
            if (!Core::instance().getFirmwareManager()->getFlashFwErrMsg().empty()) {
                flashFwErrMsg = "Update GFX_CODE succeed. " + Core::instance().getFirmwareManager()->getFlashFwErrMsg();
            }
            removeDir(tmpUnpackPath.c_str());
            return result.result;
        });

        return xpum_result_t::XPUM_OK;
    }
}

bool FwCodeDataMgmt::isUpgradingFw() {
    return taskFwCodeData.valid();
}

bool FwCodeDataMgmt::isReady() {
    if (!taskFwCodeData.valid()) {
        return true;
    }
    auto status = taskFwCodeData.wait_for(0ms);
    return status == std::future_status::ready;
}

xpum_firmware_flash_result_t FwCodeDataMgmt::getFlashFwCodeDataResult(GetFlashFwCodeDataResultParam &param){
    std::future<xpum_firmware_flash_result_t>* task=&taskFwCodeData;
    param.errMsg = flashFwErrMsg;

    if (task->valid()) {
        auto status = task->wait_for(0ms);
        if (status == std::future_status::ready) {
            return task->get();
        } else {
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
}
}