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

            ok = sc_res.exitStatus() == 0;

            XPUM_LOG_INFO("Command success: {}", ok);

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

void FwDataMgmt::getFwDataVersion() {
    std::string command = igscPath + " fw-data version -d " + devicePath + " 2>&1";
    SystemCommandResult sc_res = execCommand(command);

    if (sc_res.exitStatus() != 0) return;

    // Device: Fw Data Version: 101->0->0
    auto output = sc_res.output();
    std::regex regexp(R"(Fw Data Version: (.*)\n)");

    std::smatch m;
    if (regex_search(output, m, regexp)) {
        pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_FWDATA_FIRMWARE_NAME, m[1]));
    }
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