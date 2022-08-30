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

static bool validateImageFormat(std::string filePath){
    std::string cmd = igscPath + " image-type -i " + std::string(filePath) +" 2>&1";
    SystemCommandResult sc_res = execCommand(cmd);
    if (sc_res.exitStatus() != 0)
        return false;
    auto output = sc_res.output();

    std::string flag = "Firmware Data update image";

    return output.find(flag) != output.npos;

}

xpum_result_t FwDataMgmt::flashFwData(std::string filePath) {

    std::lock_guard<std::mutex> lck(mtx);
    if (taskFwData.valid()) {
        // task already running
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {

        if(!validateImageFormat(filePath)){
            return XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
        }

        std::string command = igscPath + " fw-data update -a -d " + devicePath + " -i " + filePath +" 2>&1";

        taskFwData = std::async(std::launch::async, [&, command] {
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
    pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION, version));
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
}