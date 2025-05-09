/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file latebinding_mgmt.cpp
 */

 #include "latebinding_mgmt.h"

 #include <igsc_lib.h>

 #include <chrono>
 #include <sstream>
 #include <iomanip>

 #include "api/device_model.h"
 #include "firmware_manager.h"
 #include "infrastructure/logger.h"
 #include "load_igsc.h"
 #include "igsc_err_msg.h"
 #include "handle_lock.h"

 namespace xpum {

 static LibIgsc libIgsc;

 xpum_result_t LateBindingMgmt::flashLateBindingFw(FlashLateBindingFwParam &param) {
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
            try {
            XPUM_LOG_INFO("Start update Late Binding on device {}", devicePath);

            struct igsc_device_handle handle {};
            int ret;

            ret = igsc_device_init_by_device(&handle, devicePath.c_str());
            if (ret != IGSC_SUCCESS) {
                flashFwErrMsg = "Cannot initialize device: " + devicePath;
                XPUM_LOG_ERROR("Cannot initialize device: {}", devicePath);
                igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            csc_late_binding_type late_binding_type;
            switch (type) {
                case XPUM_DEVICE_FIRMWARE_FAN_TABLE:
                    late_binding_type = CSC_LATE_BINDING_TYPE_FAN_TABLE;
                    break;
                case XPUM_DEVICE_FIRMWARE_VR_CONFIG:
                    late_binding_type = CSC_LATE_BINDING_TYPE_VR_CONFIG;
                    break;
                default:
                late_binding_type = CSC_LATE_BINDING_TYPE_INVALID;
            }

            uint32_t late_binding_status = {};
            ret = igsc_device_update_late_binding_config(&handle,
                late_binding_type, 0,
                (uint8_t*)buffer.data(), buffer.size(), &late_binding_status);
            if (ret != IGSC_SUCCESS) {
                flashFwErrMsg = "Late Binding update failed. " + print_device_fw_status(&handle);
                XPUM_LOG_ERROR("Late Binding update failed on device {}. {}", devicePath, print_device_fw_status(&handle));
                igsc_device_close(&handle);
                pDevice->unlock();
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }

            igsc_device_close(&handle);

            pDevice->unlock();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
        } catch (const std::exception &e) {
            flashFwErrMsg = "Late Binding update failed. " + std::string(e.what());
            XPUM_LOG_ERROR("Late Binding update failed on device {}. {}", devicePath, e.what());
            pDevice->unlock();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
        } catch (...) {
            flashFwErrMsg = "Late Binding update failed. Unknown error.";
            XPUM_LOG_ERROR("Late Binding update failed on device {}. Unknown error.", devicePath);
            pDevice->unlock();
            return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
        }
        });
        return xpum_result_t::XPUM_OK;
    }
 }

 xpum_firmware_flash_result_t LateBindingMgmt::getFlashLateBindingFwResult(GetFlashLateBindingFwResultParam &param) {
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