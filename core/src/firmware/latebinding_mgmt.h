/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file latebinding_mgmt.h
 */

 #pragma once

 #include <atomic>
 #include <future>
 #include <mutex>
 #include <string>

 #include "device/device.h"

 namespace xpum {

 struct FlashLateBindingFwParam {
     std::string filePath;
     xpum_firmware_type_t type;
     std::string errMsg;
 };

 struct GetFlashLateBindingFwResultParam{
     std::string errMsg;
 };

 class LateBindingMgmt {
    public:
     LateBindingMgmt(std::string devicePath, std::shared_ptr<Device> pDevice) : devicePath(devicePath), pDevice(pDevice){};

     xpum_result_t flashLateBindingFw(FlashLateBindingFwParam &param);

     xpum_firmware_flash_result_t getFlashLateBindingFwResult(GetFlashLateBindingFwResultParam &param);

     std::atomic<int> percent;

    private:
     std::string devicePath;

     std::mutex mtx; // lock for task
     std::future<xpum_firmware_flash_result_t> task;

     std::shared_ptr<Device> pDevice;

     std::string flashFwErrMsg;
 };

 } // namespace xpum