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
#include <igsc_lib.h>

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

static std::string print_fw_version(const struct igsc_fw_version* fw_version) {
    std::stringstream ss;
    ss << fw_version->project[0];
    ss << fw_version->project[1];
    ss << fw_version->project[2];
    ss << fw_version->project[3];
    ss << "_";
    ss << fw_version->hotfix;
    ss << ".";
    ss << fw_version->build;
    return ss.str();
}

static std::vector<GscFwVersion> getGscFwVersions() {
    std::vector<GscFwVersion> res;
    struct igsc_device_iterator* iter;
    struct igsc_device_info info;
    int ret;
    struct igsc_device_handle handle;
    struct igsc_fw_version fw_version;
    unsigned int ndevices = 0;

    // XPUM_LOG_INFO("Start iterate GSC fw");
    memset(&handle, 0, sizeof(handle));
    memset(&fw_version, 0, sizeof(fw_version));
    ret = igsc_device_iterator_create(&iter);
    if (ret != IGSC_SUCCESS) {
        XPUM_LOG_ERROR("Cannot create device iterator {}", ret);
        return res;
    }
    info.name[0] = '\0';
    while ((ret = igsc_device_iterator_next(iter, &info)) == IGSC_SUCCESS) {
        // XPUM_LOG_INFO("Find one");
        GscFwVersion fw;
        ret = igsc_device_init_by_device_info(&handle, &info);
        if (ret != IGSC_SUCCESS) {
            /* make sure we have a printable name */
            info.name[0] = '\0';
            continue;
        }
        // XPUM_LOG_INFO("Get info");

        // igsc_device_update_device_info(&handle, &info);

        // XPUM_LOG_INFO("Update info");

        ndevices++;

        fw.devicePath = info.name;
        fw.bdfAddr.domain = info.domain;
        fw.bdfAddr.bus = info.bus;
        fw.bdfAddr.device = info.dev;
        fw.bdfAddr.function = info.func;

        ret = igsc_device_fw_version(&handle, &fw_version);
        // XPUM_LOG_INFO("Get fw version");
        if (ret == IGSC_SUCCESS) {
            fw.fwVersion = print_fw_version(&fw_version);
        } else {
            XPUM_LOG_ERROR("Fail to get SoC fw version from device: {}", fw.devicePath);
            fw.fwVersion = "unknown";
        }
        /* make sure we have a printable name */
        info.name[0] = '\0';
        (void)igsc_device_close(&handle);

        res.push_back(fw);
    }
    igsc_device_iterator_destroy(iter);
    return res;
}

void FirmwareManager::detectGscFw() {
    std::vector<std::shared_ptr<Device>> devices;
    Core::instance().getDeviceManager()->getDeviceList(devices);
    auto fwList = getGscFwVersions();
    for (auto &pDevice : devices) {
        auto address = pDevice->getPciAddress();
        for (auto fw : fwList) {
            if (fw.bdfAddr == address) {
                pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION, fw.fwVersion));
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
    char* env = std::getenv("_XPUM_INIT_SKIP");
    std::string xpum_init_skip_module_list{env != NULL ? env : ""};
    if (xpum_init_skip_module_list.find("FIRMWARE") != xpum_init_skip_module_list.npos) {
        return;
    }
    // get gsc fw versions
    detectGscFw();
    // init fw-data management
    initFwDataMgmt();
    if (xpum_init_skip_module_list.find("AMC") == xpum_init_skip_module_list.npos) {
        // get amc fw versions
        amcFwList = cmd_get_amc_firmware_versions();
    }
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
    std::string cmd = igscPath + " -V 2>&1";
    SystemCommandResult sc_res = execCommand(cmd);
    if (sc_res.exitStatus() != 0)
        return false;
    return true;
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
    return sc_res.exitStatus() == 0;
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

    res = device->getFirmwareFlashResult(XPUM_DEVICE_FIRMWARE_GFX);

    result->deviceId = deviceId;
    result->type = XPUM_DEVICE_FIRMWARE_GFX;
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
    xpum_result_t res;
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
    result->type = XPUM_DEVICE_FIRMWARE_GFX_DATA;
    result->result = res;
}
} // namespace xpum
