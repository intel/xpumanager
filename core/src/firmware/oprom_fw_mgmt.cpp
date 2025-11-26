/*
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file latebinding_mgmt.cpp
 */

#include "oprom_fw_mgmt.h"

#include <igsc_lib.h>

#include <chrono>
#include <iomanip>
#include <sstream>

#include "api/device_model.h"
#include "firmware_manager.h"
#include "handle_lock.h"
#include "igsc_err_msg.h"
#include "infrastructure/logger.h"
#include "load_igsc.h"

namespace xpum {

static LibIgsc libIgsc;

static void progress_percentage_func(uint32_t done, uint32_t total, void *ctx) {
    uint32_t percent = (done * 100) / total;

    // store percent
    OpromFwMgmt *p = (OpromFwMgmt *)ctx;
    p->percent.store(percent);
}

xpum_result_t OpromFwMgmt::flashOpromFw(FlashOpromFwParam &param) {
    auto deviceModel = pDevice->getDeviceModel();
    if (deviceModel != XPUM_DEVICE_MODEL_BMG) {
        pDevice->unlock();
        return XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC;
    }
    if (!libIgsc.ok()) {
        pDevice->unlock();
        return XPUM_UPDATE_FIRMWARE_UNSUPPORTED_PSC_IGSC;
    }
    std::lock_guard<std::mutex> lck(mtx);
    std::string filePath = param.filePath;
    if (task.valid()) {
        // task already running
        pDevice->unlock();
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        auto buffer = readImageContent(filePath.c_str());

        // init fw-data update progress
        percent.store(0);

        xpum_firmware_type_t type = param.type;

        task = std::async(std::launch::async, [this, buffer, filePath, type] {
            XPUM_LOG_INFO("Start update GSC fw on device {}", devicePath);

            struct igsc_oprom_image *oprom_img = NULL;
            struct igsc_device_handle handle;
            struct igsc_oprom_version oprom_version;
            igsc_progress_func_t progress_func = progress_percentage_func;
            int ret;

            memset(&handle, 0, sizeof(handle));

            ret = igsc_device_init_by_device(&handle, devicePath.c_str());
            if (ret != IGSC_SUCCESS) {
                flashFwErrMsg = "Cannot initialize device: " + devicePath;
                XPUM_LOG_ERROR("Cannot initialize device: {}. {}", devicePath);
                (void)igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            ret = igsc_image_oprom_init(&oprom_img, (const uint8_t *)buffer.data(), buffer.size());
            if (ret != IGSC_SUCCESS) {
                flashFwErrMsg = "Cannot initialize oprom image: " + filePath;
                XPUM_LOG_ERROR("Cannot initialize oprom image: {}", filePath);
                igsc_image_oprom_release(oprom_img);
                (void)igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            enum igsc_oprom_type oprom_type;
            switch (type) {
                case XPUM_DEVICE_FIRMWARE_OPROM_DATA:
                    oprom_type = IGSC_OPROM_DATA;
                    break;
                case XPUM_DEVICE_FIRMWARE_OPROM_CODE:
                    oprom_type = IGSC_OPROM_CODE;
                    break;
                default:
                    oprom_type = IGSC_OPROM_NONE;
            }

            struct igsc_device_info device_info;
            memset(&device_info, 0, sizeof(struct igsc_device_info));

            ret = igsc_device_get_device_info(&handle, &device_info);
            if (ret) {
                flashFwErrMsg = "Unable to get device info. Update process failed. FW status: " + print_device_fw_status(&handle);
                XPUM_LOG_ERROR("Unable to get device info. Update process failed. FW status: {}", print_device_fw_status(&handle));
                igsc_image_oprom_release(oprom_img);
                (void)igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            ret = igsc_image_oprom_match_device(oprom_img, oprom_type, &device_info);
            if (ret) {
                flashFwErrMsg = "Image is not compatible with the device. FW status: " + print_device_fw_status(&handle);
                XPUM_LOG_ERROR("Image is not compatible with the device. FW status: {}", print_device_fw_status(&handle));
                igsc_image_oprom_release(oprom_img);
                (void)igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            ret = igsc_device_oprom_update(&handle, oprom_type, oprom_img, progress_func, this);
            if (ret) {
                flashFwErrMsg = "Update process failed. " + print_device_fw_status(&handle);
                XPUM_LOG_ERROR("Update process failed. {}", print_device_fw_status(&handle));
                igsc_image_oprom_release(oprom_img);
                (void)igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            // Get the updated version of the device
            int retries = 0;
            while ((ret = igsc_device_oprom_version(&handle, oprom_type, &oprom_version)) == IGSC_ERROR_BUSY) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                if (++retries >= MAX_CONNECT_RETRIES)
                    break;
            }
            if (ret != IGSC_SUCCESS) {
                XPUM_LOG_ERROR("Cannot retrieve firmware version from device: {}", devicePath);
            } else {
                std::stringstream ss;
                for (int i = 0; i < IGSC_OPROM_VER_SIZE; i++) {
                    ss << std::hex << (int)oprom_version.version[i];
		    ss << " ";
                }
                std::string version = ss.str();
                if (oprom_type == IGSC_OPROM_CODE) {
                    pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_OPROM_CODE_FIRMWARE_VERSION, version));
                } else if(oprom_type == IGSC_OPROM_DATA) {
                    pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_OPROM_DATA_FIRMWARE_VERSION, version));
                }
                XPUM_LOG_INFO("Device {} GSC OPROM fw flashed successfully to {}", devicePath, version);
            }

            igsc_image_oprom_release(oprom_img);
            (void)igsc_device_close(&handle);
            pDevice->unlock();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
        });
        return xpum_result_t::XPUM_OK;
    }
}

xpum_firmware_flash_result_t OpromFwMgmt::getFlashOpromFwResult(GetFlashOpromFwResultParam &param) {
    auto deviceModel = pDevice->getDeviceModel();
    if (deviceModel != XPUM_DEVICE_MODEL_BMG) {
        return XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED;
    }
    if (!libIgsc.ok()) {
        return XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED;
    }
    using namespace std::chrono_literals;
    param.errMsg = flashFwErrMsg;
    if (task.valid()) {
        auto status = task.wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            return task.get();
        } else {
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
}

} // namespace xpum
