/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core.cpp
 */

#include "pch.h"
#include "core.h"

#include "infrastructure/logger.h"
#include "infrastructure/configuration.h"
#include "control/device_manager.h"

namespace xpum {

    Core::Core()
        : p_device_manager(nullptr),
        initialized(false),
        ze_initialized(false) {
        XPUM_LOG_TRACE("core()");
    }

    Core::~Core() {
        XPUM_LOG_TRACE("~core()");
        close();
    }

    void Core::init() {
        std::unique_lock<std::mutex> lock(mutex);
        if (initialized) {
            return;
        }

        XPUM_LOG_INFO("xpumd core starts to initialize");

        XPUM_LOG_INFO("initialize configuration");
        Configuration::init();

        XPUM_LOG_INFO("initialize device manager");
        p_device_manager = std::make_shared<DeviceManager>();
        p_device_manager->init();


        XPUM_LOG_INFO("initialize firmware manager");
        p_firmware_manager = std::make_shared<FirmwareManager>();
        p_firmware_manager->init();

        XPUM_LOG_INFO("xpumd core initialization completed");
        initialized = true;
    }

    void Core::close() {
        std::unique_lock<std::mutex> lock(mutex);
        if (!initialized) {
            return;
        }

        p_firmware_manager = nullptr;

        close(std::dynamic_pointer_cast<InitCloseInterface>(p_device_manager),
            "Failed to close device manager");
    }

    void Core::close(const std::shared_ptr<InitCloseInterface>& p_init_close_interface,
        const std::string& p_msgPrix) {
        if (p_init_close_interface == nullptr) {
            return;
        }

        try {
            p_init_close_interface->close();
        }
        catch (std::exception& e) {
            XPUM_LOG_WARN("{}: {}", p_msgPrix, e.what());
        }
        catch (...) {
            XPUM_LOG_WARN("{}: unexpected exception", p_msgPrix);
        }
    }

    Core& Core::instance() {
        static Core instance;
        return instance;
    }

    std::shared_ptr<DeviceManagerInterface> Core::getDeviceManager() {
        return p_device_manager;
    }

    std::shared_ptr<FirmwareManager> Core::getFirmwareManager() {
        return p_firmware_manager;
    }

    bool Core::isInitialized() {
        std::unique_lock<std::mutex> lock(mutex);
        return this->initialized;
    }

    bool Core::isZeInitialized() {
        std::unique_lock<std::mutex> lock(mutex);
        return this->ze_initialized;
    }

    void Core::setZeInitialized(bool val) {
        std::unique_lock<std::mutex> lock(mutex);
        this->ze_initialized = val;
    }

    xpum_result_t Core::apiAccessPreCheck() {
        if (!this->ze_initialized) {
            return XPUM_LEVEL_ZERO_INITIALIZATION_ERROR;
        }
        return XPUM_OK;
    }
} // end namespace xpum
