/*
 *  Copyright (C) 2021-2022 Intel Corporation
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

namespace xpum {

using namespace std::chrono_literals;

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

xpum_result_t FwDataMgmt::flashFwData(std::string filePath) {

    std::lock_guard<std::mutex> lck(mtx);
    if (taskFwData.valid()) {
        // task already running
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        auto buffer = readImageContent(filePath.c_str());

        if(!validateImageFormat(buffer)){
            return XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
        }

        if (!isFwDataImageAndDeviceCompatible(buffer, devicePath)) {
            return XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
        }

        std::string command = igscPath + " fw-data update -a -d " + devicePath + " -i " + filePath +" 2>&1";

        taskFwData = std::async(std::launch::async, [&, command, this] {
            // XPUM_LOG_INFO("Start update GSC fw, total {} commands", commands.size());
            bool ok = true;

            XPUM_LOG_INFO("Execute command: {}", command);

            SystemCommandResult sc_res = execCommand(command);

            std::string output = sc_res.output();

            if (sc_res.exitStatus() != 0 || output.find("Error") != output.npos) {
                XPUM_LOG_ERROR("Error occurs: {}", output);
                ok = false;
            } else {
                XPUM_LOG_INFO("Command success");
            }

            // refresh fw-data version
            getFwDataVersion();

            xpum_firmware_flash_result_t rc;
            if (ok) {
                rc = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
            } else {
                rc = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            pDevice->unlock();
            return rc;
        });

        return xpum_result_t::XPUM_OK;
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
        return "unknown";
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
        return "unknown";
    }

    auto version = print_fwdata_version(&fwdata_version);

    igsc_device_close(&handle);
    
    return version;

}

void FwDataMgmt::getFwDataVersion() {
    auto version = fwdata_device_version(devicePath.c_str());
    pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_FWDATA_FIRMWARE_VERSION, version));
}

xpum_firmware_flash_result_t FwDataMgmt::getFlashFwDataResult(){
    std::future<xpum_firmware_flash_result_t>* task=&taskFwData;

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