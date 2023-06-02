/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core.h
 */

#pragma once

#include <memory>
#include <mutex>

#include "infrastructure/init_close_interface.h"
#include "control/device_manager_interface.h"
#include "firmware/firmware_manager.h"
#include "xpum_structs.h"

namespace xpum {

    /*
        The top controller of xpum
    */
    class Core : public InitCloseInterface {
        public:
            static Core& instance();

            void init() override;

            void close() override;

            bool isInitialized();

            bool isZeInitialized();

            void setZeInitialized(bool val);

            xpum_result_t apiAccessPreCheck();

            std::shared_ptr<DeviceManagerInterface> getDeviceManager();

            std::shared_ptr<FirmwareManager> getFirmwareManager();

        private:
            Core();

            Core& operator=(const Core&) = delete;

            Core(const Core& other) = delete;

            ~Core();

            void close(const std::shared_ptr<InitCloseInterface>& p_init_close_interface, const std::string& p_msgPrix);

            std::shared_ptr<DeviceManagerInterface> p_device_manager;

            std::shared_ptr<FirmwareManager> p_firmware_manager;

            bool initialized;

            bool ze_initialized;

            std::mutex mutex;
    };

} // end namespace xpum
