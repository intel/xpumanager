/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware_manager.h
 */

#pragma once

#include <set>
#include <unordered_map>
#include <mutex>
#include <string>
#include <future>

#include "igsc/igsc_lib.h"
#include "firmware/amc/ipmi_amc_manager.h"
#include "firmware/amc/amc_manager.h"

#include "xpum_structs.h"

namespace xpum {

    namespace gfx_fw_status {
        enum GfxFwStatus {
            RESET,
            INIT,
            RECOVERY,
            TEST,
            FW_DISABLED,
            NORMAL,
            DISABLE_WAIT,
            OP_STATE_TRANS,
            INVALID_CPU_PLUGGED_IN,
            UNKNOWN
        };
    }

	class FirmwareManager {
	public:
		void init();
        bool initIgsc();
        std::vector<int> getSiblingDevices(int deviceId);
        xpum_result_t runGSCFirmwareFlash(xpum_device_id_t deviceId, const char* filePath, bool force = false);
        xpum_result_t runFwDataFlash(xpum_device_id_t deviceId, const char* filePath);
        xpum_result_t runAMCFlash(const char* filePath, const char* user, const char * password);
        xpum_result_t getAmcFwVersions(std::vector<std::string> *versions, const char* user, const char* password);
        void getFlashResult(xpum_device_id_t deviceId, xpum_firmware_flash_task_result_t* result);
        bool isFwImageAndDeviceCompatible(std::string bdf, std::string image_file);
        bool isFwDataImageAndDeviceCompatible(std::string bdf, std::string image_file, std::string& error_message);
        bool updateFWVersionProps();
        xpum_result_t getSimpleEccState(xpum_device_id_t deviceId, uint8_t& current, uint8_t& pending);
        xpum_result_t setSimpleEccState(xpum_device_id_t deviceId, uint8_t req, uint8_t& current, uint8_t& pending);


        std::string getFlashFwErrMsg() {
            return flashFwErrMsg;
        }
        std::string getDeviceGSCVersion(std::string bdf);
        std::string getDeviceGSCDataVersion(std::string bdf);

        gfx_fw_status::GfxFwStatus getGfxFwStatus(xpum_device_id_t deviceId);
        static std::string transGfxFwStatusToString(gfx_fw_status::GfxFwStatus status);

    private:
        bool loadSibingDevices();
        
        bool isSiblingDeviceReady = false;
        bool isIgscInit = false;
        bool isFWVerPropUpdated = false;
        std::shared_ptr<AmcManager> p_amc_manager;

        std::mutex m_sblg;
        std::mutex m_igsc;
        std::mutex m_prop;
        std::mutex m_results;

        std::unordered_map<int, std::set<int>> sibling_devices;
        std::unordered_map<std::string, std::string> bdf_to_devicepath;
        std::vector<std::future<xpum_firmware_flash_result_t>> flash_results;
        std::string flashFwErrMsg;
	};
} // end namespace xpum