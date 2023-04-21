/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fwdata_mgmt.cpp
 */

#include "fwdata_mgmt.h"

#include <vector>
#include <fstream> 
#include <chrono>
#include <regex>
#include <igsc_lib.h>

#include "infrastructure/logger.h"
#include "system_cmd.h"
#include "firmware_manager.h"
#include "igsc_err_msg.h"

namespace xpum {

using namespace std::chrono_literals;

static std::string print_fwdata_version(const struct igsc_fwdata_version *fwdata_version);

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

static bool isFwDataImageAndDeviceCompatible(std::vector<char>& buffer, std::string devicePath) {
    struct igsc_fwdata_image* oimg = NULL;
    int ret;
    // image
    ret = igsc_image_fwdata_init(&oimg, (const uint8_t*)buffer.data(), buffer.size());
    if (ret != IGSC_SUCCESS) {
        igsc_image_fwdata_release(oimg);
        return false;
    }
    // device
    struct igsc_device_info dev_info;
    struct igsc_device_handle handle;
    ret = igsc_device_init_by_device(&handle, devicePath.c_str());
    if (ret != IGSC_SUCCESS) {
        igsc_image_fwdata_release(oimg);
        igsc_device_close(&handle);
        return false;
    }
    ret = igsc_device_get_device_info(&handle, &dev_info);
    if (ret != IGSC_SUCCESS) {
        igsc_image_fwdata_release(oimg);
        igsc_device_close(&handle);
        return false;
    }

    ret = igsc_image_fwdata_match_device(oimg, &dev_info);
    igsc_image_fwdata_release(oimg);
    igsc_device_close(&handle);
    return ret == IGSC_SUCCESS;
}

static void progress_percentage_func(uint32_t done, uint32_t total, void* ctx) {
    uint32_t percent = (done * 100) / total;

    // store percent 
    FwDataMgmt* p = (FwDataMgmt*) ctx;
    p->percent.store(percent);
}

xpum_result_t FwDataMgmt::flashFwData(FlashFwDataParam &param) {
    std::lock_guard<std::mutex> lck(mtx);
    std::string filePath = param.filePath;
    if (taskFwData.valid()) {
        // task already running
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        auto buffer = readImageContent(filePath.c_str());

        if (!validateImageFormat(buffer)) {
            return XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
        }

        if (!isFwDataImageAndDeviceCompatible(buffer, devicePath)) {
            return XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
        }

        // init fw-data update progress
        percent.store(0);

        taskFwData = std::async(std::launch::async, [this, buffer, filePath] {
            XPUM_LOG_INFO("Start update GSC FW-DATA on device {}", devicePath);

            struct igsc_device_handle handle;
            int ret;

            struct igsc_fwdata_image* oimg = NULL;
            struct igsc_fwdata_version dev_version;
            igsc_progress_func_t progress_func = progress_percentage_func;

            memset(&handle, 0, sizeof(handle));

            ret = igsc_device_init_by_device(&handle, devicePath.c_str());
            if (ret != IGSC_SUCCESS) {
                flashFwErrMsg = "Cannot initialize device: " + devicePath + ", error code: " + std::to_string(ret) + " error message: " + transIgscErrCodeToMsg(ret);
                XPUM_LOG_ERROR("Cannot initialize device: {}, error code: {}, error message: {}", devicePath, ret, transIgscErrCodeToMsg(ret));
                igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            ret = igsc_image_fwdata_init(&oimg, (const uint8_t*)buffer.data(), buffer.size());
            if (ret == IGSC_ERROR_BAD_IMAGE) {
                flashFwErrMsg = "Invalid image format: " + filePath;
                XPUM_LOG_ERROR("Invalid image format: {}", filePath);
                igsc_image_fwdata_release(oimg);
                igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            ret = igsc_device_fwdata_image_update(&handle, oimg, progress_func, this);

            if (ret) {
                flashFwErrMsg = "GFX_DATA update failed, error code: " + std::to_string(ret) + " error message: " + transIgscErrCodeToMsg(ret);
                XPUM_LOG_ERROR("GFX_DATA update failed on device {}, error code: {}, error message: {}", devicePath, ret, transIgscErrCodeToMsg(ret));
                igsc_image_fwdata_release(oimg);
                igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            ret = igsc_device_fwdata_version(&handle, &dev_version);
            if (ret != IGSC_SUCCESS) {
                XPUM_LOG_ERROR("Failed to get firmware version after update from device {}", devicePath);
            } else {
                std::string version = print_fwdata_version(&dev_version);
                pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION, version));
                XPUM_LOG_INFO("GSC FW-DATA on device {} is successfully flashed to {}", devicePath, version);
            }

            igsc_image_fwdata_release(oimg);
            igsc_device_close(&handle);
            pDevice->unlock();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
        });

        return xpum_result_t::XPUM_OK;
    }
}

static std::string print_fwdata_version(const struct igsc_fwdata_version *fwdata_version) {
    std::stringstream ss;
    ss << "0x" << std::hex << fwdata_version->oem_manuf_data_version;
    return ss.str();
}

std::string fwdata_device_version(const char *device_path)
{
    struct igsc_fwdata_version fwdata_version;
    struct igsc_device_handle handle;
    int ret;

    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, device_path);
    if (ret != IGSC_SUCCESS) {
        XPUM_LOG_ERROR("Failed to initialize device: {}", device_path);
        igsc_device_close(&handle);
        return "";
    }

    memset(&fwdata_version, 0, sizeof(fwdata_version));
    ret = igsc_device_fwdata_version(&handle, &fwdata_version);
    if (ret != IGSC_SUCCESS)
    {
        if (ret == IGSC_ERROR_PERMISSION_DENIED)
        {
            XPUM_LOG_ERROR("Permission denied: missing required credentials to access the device {}", device_path);
        }
        else
        {
            XPUM_LOG_ERROR("Fail to get fwdata version from device: {}", device_path);
            // print_device_fw_status(&handle);
        }
        igsc_device_close(&handle);
        return "";
    }

    auto version = print_fwdata_version(&fwdata_version);

    igsc_device_close(&handle);
    
    return version;

}

void FwDataMgmt::getFwDataVersion() {
    Property prop;
    if (pDevice->getProperty(
                XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_FUNCTION_TYPE, prop) 
            == true && prop.getValueInt() == DEVICE_FUNCTION_TYPE_VIRTUAL) {
        XPUM_LOG_DEBUG("Skip getting FW data version for VF");
        return;
    }
    auto version = fwdata_device_version(devicePath.c_str());
    pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION, version));
}

xpum_firmware_flash_result_t FwDataMgmt::getFlashFwDataResult(GetFlashFwDataResultParam &param){
    std::future<xpum_firmware_flash_result_t>* task=&taskFwData;
    param.errMsg = flashFwErrMsg;

    if (task->valid()) {
        auto status = task->wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            return task->get();
        } else {
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
}

bool FwDataMgmt::isUpgradingFw() {
    return taskFwData.valid();
}

bool FwDataMgmt::isReady() {
    if (!taskFwData.valid()) {
        return true;
    }
    auto status = taskFwData.wait_for(0ms);
    return status == std::future_status::ready;
}
}
