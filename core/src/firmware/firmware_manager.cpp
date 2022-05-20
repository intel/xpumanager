/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file dump_manager.h
 */
#include "firmware_manager.h"
#include "core/core.h"
#include "system_cmd.h"
#include "fwdata_mgmt.h"
#include "group/group_manager.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <regex>

namespace xpum {

extern int cmd_firmware(const char* file, unsigned int versions[4]);

extern std::vector<std::string> cmd_get_amc_firmware_versions();

SystemCommandResult execCommand(const std::string& command) {
    int exitcode = 0;
    std::array<char, 1048576> buffer{};
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    if (pipe != nullptr) {
        try {
            std::size_t bytesread;
            while ((bytesread = std::fread(buffer.data(), sizeof(buffer.at(0)), sizeof(buffer), pipe)) != 0) {
                result += std::string(buffer.data(), bytesread);
            }
        } catch (...) {
            pclose(pipe);
        }
        int ret = pclose(pipe);
        exitcode = WEXITSTATUS(ret);
    }

    return SystemCommandResult(result, exitcode);
}

struct GscFwVersion {
    std::string devicePath;
    pci_address_t bdfAddr;
    std::string fwVersion;
};

static std::vector<GscFwVersion> getGscFwVersions() {
    std::vector<GscFwVersion> res;
    std::string cmd = igscPath + " list-devices --info 2>&1";
    SystemCommandResult sc_res = execCommand(cmd);
    if (sc_res.exitStatus() != 0)
        return res;
    auto output = sc_res.output();
    std::regex regexp(R"(Device \[\d+\] '(/dev/mei\d+)':.*([0-9a-f]{4}):([0-9a-f]{2}):([0-9a-f]{2})\.([0-9a-f]{2})\nFW Version: (.*)\n)");

    std::smatch m;
    while (regex_search(output, m, regexp)) {
        GscFwVersion fw;
        fw.devicePath = m[1];
        fw.bdfAddr.domain = stoi(m[2], 0, 16);
        fw.bdfAddr.bus = stoi(m[3], 0, 16);
        fw.bdfAddr.device = stoi(m[4], 0, 16);
        fw.bdfAddr.function = stoi(m[5], 0, 16);
        fw.fwVersion = m[6];
        output = m.suffix();
        res.push_back(fw);
    }
    return res;
}

void FirmwareManager::detectGscFw() {
    std::vector<std::shared_ptr<Device>> devices;
    Core::instance().getDeviceManager()->getDeviceList(devices);
    auto fwList = getGscFwVersions();
    for (auto& pDevice : devices) {
        auto address = pDevice->getPciAddress();
        for (auto fw : fwList) {
            if (fw.bdfAddr == address) {
                pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_FIRMWARE_VERSION, fw.fwVersion));
                pDevice->setMeiDevicePath(fw.devicePath);
            }
        }
    }
}

void FirmwareManager::getAMCFwVersions() {
    if(amcUpdated){
        // firmware updated, need to re get version info
        amcFwList = cmd_get_amc_firmware_versions();
        amcUpdated = false;
    }
}

void FirmwareManager::initFwDataMgmt(){
    std::vector<std::shared_ptr<Device>> devices;
    Core::instance().getDeviceManager()->getDeviceList(devices);
    for (auto pDevice : devices) {
        pDevice->setFwDataMgmt(std::make_shared<FwDataMgmt>(pDevice->getMeiDevicePath(), pDevice));
        pDevice->getFwDataMgmt()->getFwDataVersion();
    }
}

void FirmwareManager::init() {
    // get amc fw versions
    amcFwList = cmd_get_amc_firmware_versions();
    // get gsc fw versions
    detectGscFw();
    // init fw-data management
    initFwDataMgmt();
};

std::vector<std::string> FirmwareManager::getAMCFirmwareVersions() {
    getAMCFwVersions();
    return amcFwList;
}

xpum_result_t FirmwareManager::runAMCFirmwareFlash(const char* filePath) {
    if (amcFwList.size() <= 0) {
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC;
    }

    std::lock_guard<std::mutex> lck(mtx);
    if (taskAMC.valid()) {
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    } else {
        std::string dupPath(filePath);
        taskAMC = std::async(std::launch::async, [dupPath, this] {
            int rc = cmd_firmware(dupPath.c_str(), nullptr);
            this->amcUpdated = true;
            if (rc == 0) {
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
            } else {
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
        });

        return xpum_result_t::XPUM_OK;
    }
}

void FirmwareManager::getAMCFirmwareFlashResult(xpum_firmware_flash_task_result_t* result) {
    std::future<xpum_firmware_flash_result_t>* task = &taskAMC;

    xpum_firmware_flash_result_t res;

    if (task->valid()) {
        using namespace std::chrono_literals;
        auto status = task->wait_for(0ms);
        if (status == std::future_status::ready) {
            std::lock_guard<std::mutex> lck(mtx);
            res = task->get();
        } else {
            res = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
        }
    } else {
        res = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
    }
    result->deviceId = XPUM_DEVICE_ID_ALL_DEVICES;
    result->type = XPUM_DEVICE_FIRMWARE_AMC;
    result->result = res;
}


static bool detectGfxTool() {
    if (FILE *file = fopen(igscPath.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }
}

static bool isGscFwImage(const char* filePath) {
    std::string cmd = igscPath + " image-type -i " + std::string(filePath) +" 2>&1";
    SystemCommandResult sc_res = execCommand(cmd);
    if (sc_res.exitStatus() != 0)
        return false;
    auto output = sc_res.output();

    std::string flag = "GFX FW Update image";

    return output.find(flag) != output.npos;
}

static bool isFwImageAndDeviceCompatible(std::string meiPath, const char* filePath) {
    std::string cmd = igscPath + " fw hwconfig -d " + meiPath + " -i " + std::string(filePath) + " 2>&1";
    SystemCommandResult sc_res = execCommand(cmd);
    if (sc_res.exitStatus() != 0) {
        return false;
    }
    auto output = sc_res.output();
    XPUM_LOG_INFO("image and device compatible check: {}", output);
    std::smatch m;
    std::regex imageHwSkuRegexp(R"(image:\s+hw sku:\s+\[\s+(.*)\s+\]\s+hw step:\s+\[.*\n)");
    auto matched = regex_search(output, m, imageHwSkuRegexp);
    if (!matched)
        return false;
    std::string imageHwSku = m[1];
    XPUM_LOG_INFO("image hw sku: {}", imageHwSku);
    std::regex deviceHwSkuRegexp(R"(device:\s+hw sku:\s+\[\s+(.*)\s+\]\s+hw step:\s+\[.*\n)");
    matched = regex_search(output, m, deviceHwSkuRegexp);
    if (!matched)
        return false;
    std::string deviceHwSku = m[1];
    XPUM_LOG_INFO("device hw sku: {}", deviceHwSku);
    return imageHwSku.compare(deviceHwSku) == 0;
}

xpum_result_t FirmwareManager::runGSCFirmwareFlash(xpum_device_id_t deviceId, const char* filePath) {
    // check tool if tool exists
    if (!detectGfxTool()) {
        XPUM_LOG_INFO("flash tool not exists");
        return XPUM_UPDATE_FIRMWARE_IGSC_NOT_FOUND;
    }

    // validate the image file
    if (!isGscFwImage(filePath)) {
        return XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
    }

    // check device exists
    std::shared_ptr<Device> pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr) {
        return XPUM_GENERIC_ERROR;
    }

    // validate the image is compatible with the device
    if (!isFwImageAndDeviceCompatible(pDevice->getMeiDevicePath(), filePath)) {
        return XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
    }

    return pDevice->runFirmwareFlash(filePath, igscPath);
}

void FirmwareManager::getGSCFirmwareFlashResult(xpum_device_id_t deviceId, xpum_firmware_flash_task_result_t *result) {
    xpum_firmware_flash_result_t res;
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));

    res = device->getFirmwareFlashResult(XPUM_DEVICE_FIRMWARE_GSC);

    result->deviceId = deviceId;
    result->type = XPUM_DEVICE_FIRMWARE_GSC;
    result->result = res;
}

bool FirmwareManager::isUpgradingFw(void) {
    return taskAMC.valid();
}

static std::vector<std::shared_ptr<Device>> getSiblingDevices(std::shared_ptr<Device> pDevice) {
    xpum::Core& core = xpum::Core::instance();
    auto groupManager = core.getGroupManager();
    std::vector<std::shared_ptr<Device>> result;

    int count = 0;
    groupManager->getAllGroupIds(nullptr, &count);
    std::vector<xpum_group_id_t> groupIds(count);
    groupManager->getAllGroupIds(groupIds.data(), &count);

    xpum_device_id_t deviceId = std::stoi(pDevice->getId());
    for (int i = 0; i < count; i++) {
        auto groupId = groupIds[i];
        if (groupId & BUILD_IN_GROUP_MASK) {
            xpum_group_info_t groupInfo;
            groupManager->getGroupInfo(groupId, &groupInfo);

            for (int j = 0; j < groupInfo.count; j++) {
                // device in build in group
                if (groupInfo.deviceList[j] == deviceId) {
                    auto deviceManager = core.getDeviceManager();
                    for (int k = 0; k < groupInfo.count; k++) {
                        auto siblingId = groupInfo.deviceList[k];
                        std::string siblingIdStr = std::to_string(siblingId);
                        auto pSiblingDevice = deviceManager->getDevice(siblingIdStr);
                        result.push_back(pSiblingDevice);
                    }
                    return result;
                }
            }
        }
    }
    result.push_back(pDevice);
    return result;
}

xpum_result_t FirmwareManager::runFwDataFlash(xpum_device_id_t deviceId, const char* filePath) {
    std::shared_ptr<Device> pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr) {
        return XPUM_GENERIC_ERROR;
    }
    xpum_result_t res = XPUM_GENERIC_ERROR;
    // check for ats-m3
    auto deviceList = getSiblingDevices(pDevice);
    for (auto pd : deviceList) {
        res = pd->getFwDataMgmt()->flashFwData(filePath);
        if (res != XPUM_OK)
            break;
    }
    return res;
}

void FirmwareManager::getFwDataFlashResult(xpum_device_id_t deviceId, xpum_firmware_flash_task_result_t* result) {
    xpum_firmware_flash_result_t res;
    std::shared_ptr<Device> pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));

    res = pDevice->getFwDataMgmt()->getFlashFwDataResult();

    result->deviceId = deviceId;
    result->type = XPUM_DEVICE_FIRMWARE_FW_DATA;
    result->result = res;
}
} // namespace xpum