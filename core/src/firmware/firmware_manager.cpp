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
#include "api/device_model.h"
#include "amc/ipmi_amc_manager.h"
#include "amc/redfish_amc_manager.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <regex>
#include <igsc_lib.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace xpum {


static std::vector<std::shared_ptr<Device>> getSiblingDevices(std::shared_ptr<Device> pDevice);

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
    for (auto& pDevice : devices) {
        auto address = pDevice->getPciAddress();
        for (auto fw : fwList) {
            if (fw.bdfAddr == address) {
                pDevice->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION, fw.fwVersion));
                pDevice->setMeiDevicePath(fw.devicePath);
            }
        }
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
        // init amc manager
        preInitAmcManager();
    }
};

void FirmwareManager::preInitAmcManager() {
    p_amc_manager = std::make_shared<IpmiAmcManager>();
    auto ipmi_enabled = p_amc_manager->preInit();
    if (!ipmi_enabled) {
        p_amc_manager = std::make_shared<RedfishAmcManager>();
        p_amc_manager->preInit();
    }
}

bool FirmwareManager::initAmcManager() {
    InitParam param;
    if (p_amc_manager->init(param))
        return true;
    getAmcFwErrMsg = flashFwErrMsg = param.errMsg;
    return false;
}

xpum_result_t FirmwareManager::getAMCFirmwareVersions(std::vector<std::string>& versions, AmcCredential credential) {
    getAmcFwErrMsg.clear();
    if (!initAmcManager()) {
        // getAmcFwErrMsg = "Fail to get AMC firmware versions";
        return XPUM_GENERIC_ERROR;
        // return XPUM_OK;
    }
    GetAmcFirmwareVersionsParam param;
    param.username = credential.username;
    param.password = credential.password;
    
    p_amc_manager->getAmcFirmwareVersions(param);
    getAmcFwErrMsg = param.errMsg;
    if (param.errCode != xpum_result_t::XPUM_OK) {
        return param.errCode;
    }
    for (auto version : param.versions) {
        versions.push_back(version);
    }
    return param.errCode;
}

xpum_result_t FirmwareManager::runAMCFirmwareFlash(const char* filePath, AmcCredential credential) {
    flashFwErrMsg.clear();
    if (!initAmcManager()) {
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC;
    }

    std::vector<std::shared_ptr<Device>> allDevices;
    Core::instance().getDeviceManager()->getDeviceList(allDevices);
    // lock all devices
    bool locked = Core::instance().getDeviceManager()->tryLockDevices(allDevices);
    if (!locked) {
        flashFwErrMsg = "Device is bussy";
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    }

    FlashAmcFirmwareParam param;
    param.file = std::string(filePath);
    param.username = credential.username;
    param.password = credential.password;
    param.callback = [this]() {
        // unlock all device when update finish
        std::vector<std::shared_ptr<Device>> allDevices;
        Core::instance().getDeviceManager()->getDeviceList(allDevices);
        Core::instance().getDeviceManager()->unlockDevices(allDevices);
    };

    p_amc_manager->flashAMCFirmware(param);
    flashFwErrMsg = param.errMsg;
    return (xpum_result_t)param.errCode;
}

xpum_result_t FirmwareManager::getAMCFirmwareFlashResult(xpum_firmware_flash_task_result_t* result, AmcCredential credential) {
    if (!initAmcManager()) {
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC;
    }
    GetAmcFirmwareFlashResultParam param;
    param.username = credential.username;
    param.password = credential.password;
    p_amc_manager->getAMCFirmwareFlashResult(param);

    flashFwErrMsg = param.errMsg;

    if (param.errCode != XPUM_OK) {
        return param.errCode;
    }
    *result = param.result;
    return XPUM_OK;
}

std::string FirmwareManager::getAmcWarnMsg() {
    if (p_amc_manager)
        return "";
    return getRedfishAmcWarn();
}

static bool isGscFwImage(std::vector<char>& buffer) {
    uint8_t type;
    int ret;
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS)
    {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_GFX_FW;
}

static bool atsmHwConfigCompatibleCheck(std::string meiPath, std::vector<char>& buffer) {
    struct igsc_hw_config img_hw_config, dev_hw_config;
    int ret;
    // image hw config
    ret = igsc_image_hw_config((const uint8_t*)buffer.data(), buffer.size(), &img_hw_config);
    if (ret != IGSC_SUCCESS) {
        return false;
    }

    // device hw config
    struct igsc_device_handle handle;

    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, meiPath.c_str());
    if (ret != IGSC_SUCCESS) {
        (void)igsc_device_close(&handle);
        return false;
    }

    ret = igsc_device_hw_config(&handle, &dev_hw_config);
    if (ret != IGSC_SUCCESS) {
        (void)igsc_device_close(&handle);
        return false;
    }

    (void)igsc_device_close(&handle);

    ret = igsc_hw_config_compatible(&img_hw_config, &dev_hw_config);
    if (ret != IGSC_SUCCESS) {
        return false;
    }
    return true;
}

static bool isPVCFwImageAndDeviceCompatible(std::string meiPath, std::vector<char>& buffer) {
    struct igsc_fw_version img_fw_version, dev_fw_version;
    int ret;
    // image fw version
    ret = igsc_image_fw_version((const uint8_t*)buffer.data(), buffer.size(), &img_fw_version);
    if (ret != IGSC_SUCCESS)
    {
        return false;
    }
    // device fw version
    struct igsc_device_handle handle;

    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, meiPath.c_str());
    if (ret != IGSC_SUCCESS) {
        return false;
    }

    memset(&dev_fw_version, 0, sizeof(dev_fw_version));
    ret = igsc_device_fw_version(&handle, &dev_fw_version);
    if (ret != IGSC_SUCCESS)
    {
        (void)igsc_device_close(&handle);
        return false;
    }

    (void)igsc_device_close(&handle);

    return std::equal(std::begin(dev_fw_version.project), std::end(dev_fw_version.project), std::begin(img_fw_version.project));
}

std::vector<char> readImageContent(const char* filePath) {
    struct stat s;
    if (stat(filePath, &s) != 0 || !(s.st_mode & S_IFREG))
        return std::vector<char>();
    std::ifstream is(std::string(filePath), std::ifstream::binary);
    if (!is) {
        return std::vector<char>();
    }
    // get length of file:
    is.seekg(0, is.end);
    int length = is.tellg();
    is.seekg(0, is.beg);

    std::vector<char> buffer(length);

    is.read(buffer.data(), length);
    is.close();
    return buffer;
}

xpum_result_t FirmwareManager::runGSCFirmwareFlash(xpum_device_id_t deviceId, const char* filePath) {
    // read image file
    auto buffer = readImageContent(filePath);

    // validate the image file
    if (!isGscFwImage(buffer)) {
        return XPUM_UPDATE_FIRMWARE_INVALID_FW_IMAGE;
    }

    // check device exists
    std::shared_ptr<Device> pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));
    if (pDevice == nullptr) {
        return XPUM_GENERIC_ERROR;
    }

    // validate the image is compatible with the device
    if (pDevice->getDeviceModel() == XPUM_DEVICE_MODEL_ATS_M_1 || pDevice->getDeviceModel() == XPUM_DEVICE_MODEL_ATS_M_3) {
        if (!atsmHwConfigCompatibleCheck(pDevice->getMeiDevicePath(), buffer)) {
            return XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
        }
    } else {
        if (!isPVCFwImageAndDeviceCompatible(pDevice->getMeiDevicePath(), buffer)) {
            return XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
        }
    }

    xpum_result_t res = XPUM_GENERIC_ERROR;

    // check for ats-m3
    auto deviceList = getSiblingDevices(pDevice);
    bool locked = Core::instance().getDeviceManager()->tryLockDevices(deviceList);
    if (!locked)
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    // check is updating fw
    for (auto pd : deviceList) {
        if (pd->isUpgradingFw()) {
            Core::instance().getDeviceManager()->unlockDevices(deviceList);
            return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        }
    }
    // try to update
    bool stop = false;
    std::vector<std::shared_ptr<Device>> toUnlock;
    for (auto pd : deviceList) {
        if (!stop) {
            res = pd->runFirmwareFlash(buffer);
            if (res != XPUM_OK) {
                stop = true;
                toUnlock.push_back(pd);
            }
        } else {
            toUnlock.push_back(pd);
        }
    }
    if (toUnlock.size() > 0) {
        // some device fail to start, remember to unlock
        Core::instance().getDeviceManager()->unlockDevices(toUnlock);
    }
    return res;
}

void FirmwareManager::getGSCFirmwareFlashResult(xpum_device_id_t deviceId, xpum_firmware_flash_task_result_t *result) {
    xpum_firmware_flash_result_t res;
    std::shared_ptr<Device> device = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));

    // res = device->getFirmwareFlashResult(XPUM_DEVICE_FIRMWARE_GSC);

    result->deviceId = deviceId;
    result->type = XPUM_DEVICE_FIRMWARE_GFX;
    // result->result = res;
    auto deviceList = getSiblingDevices(device);

    int totalPercent = 0;
    bool ongoing = false;
    for (auto pd : deviceList) {
        totalPercent += pd->gscFwFlashPercent.load();
        // if sibling device is upgrading, and dont get the result until all device is ready
        if (pd->isUpgradingFw() && !pd->isUpgradingFwResultReady()) {
            result->result = XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
            ongoing = true;
        }
    }
    result->percentage = totalPercent / deviceList.size();
    if (ongoing) {
        return;
    }

    result->result = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;

    for (auto pd : deviceList) {
        res = pd->getFirmwareFlashResult(XPUM_DEVICE_FIRMWARE_GFX);
        if (res != xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK) {
            result->result = res;
        }
    }
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
    // check device is busy or not
    auto deviceList = getSiblingDevices(pDevice);
    bool locked = Core::instance().getDeviceManager()->tryLockDevices(deviceList);
    if (!locked)
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
    // check is updating fw
    for (auto pd : deviceList) {
        if (pd->getFwDataMgmt()->isUpgradingFw()) {
            Core::instance().getDeviceManager()->unlockDevices(deviceList);
            return xpum_result_t::XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
        }
    }
    // try to update
    bool stop = false;
    std::vector<std::shared_ptr<Device>> toUnlock;
    for (auto pd : deviceList) {
        if (!stop) {
            res = pd->getFwDataMgmt()->flashFwData(filePath);
            if (res != XPUM_OK) {
                stop = true;
                toUnlock.push_back(pd);
            }
        } else {
            toUnlock.push_back(pd);
        }
    }
    if (toUnlock.size() > 0) {
        // some device fail to start, remember to unlock
        Core::instance().getDeviceManager()->unlockDevices(toUnlock);
    }
    return res;
}

void FirmwareManager::getFwDataFlashResult(xpum_device_id_t deviceId, xpum_firmware_flash_task_result_t* result) {
    std::lock_guard<std::mutex> lck(mtx);
    xpum_firmware_flash_result_t res;
    std::shared_ptr<Device> pDevice = Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId));

    result->deviceId = deviceId;
    result->type = XPUM_DEVICE_FIRMWARE_GFX_DATA;
    auto deviceList = getSiblingDevices(pDevice);

    bool ongoing = false;
    int totalPercent = 0;
    for (auto pd : deviceList) {
        // if sibling device is upgrading, and dont get the result until all device is ready
        auto fwDataMgmt = pd->getFwDataMgmt();
        totalPercent += fwDataMgmt->percent.load();
        if (fwDataMgmt->isUpgradingFw() && !fwDataMgmt->isReady()) {
            result->result = XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
            ongoing = true;
        }
    }
    result->percentage = totalPercent / deviceList.size();
    if (ongoing) {
        return;
    }

    result->result = xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;

    for (auto pd : deviceList) {
        res = pd->getFwDataMgmt()->getFlashFwDataResult();
        if (res != xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK) {
            result->result = res;
        }
    }
}

xpum_result_t FirmwareManager::getAMCSensorReading(xpum_sensor_reading_t data[], int* count) {
    if (!initAmcManager()) {
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC;
    }
    GetAmcSensorReadingParam param;
    p_amc_manager->getAMCSensorReading(param);
    if (param.errCode != XPUM_OK) {
        return param.errCode;
    }
    auto& readingDataList = param.dataList;
    if (data == nullptr) {
        *count = readingDataList.size();
        return XPUM_OK;
    }
    if (*count < (int)readingDataList.size()) {
        return XPUM_BUFFER_TOO_SMALL;
    }
    for (std::size_t i = 0; i < readingDataList.size(); i++) {
        data[i] = readingDataList.at(i);
    }
    return XPUM_OK;
}

xpum_result_t FirmwareManager::getAMCSlotSerialNumbers(AmcCredential credential, std::vector<SlotSerialNumberAndFwVersion>& serialNumberList){
    if (!initAmcManager()) {
        return xpum_result_t::XPUM_UPDATE_FIRMWARE_UNSUPPORTED_AMC;
    }
    GetAmcSlotSerialNumbersParam param;
    param.username = credential.username;
    param.password = credential.password;
    p_amc_manager->getAMCSlotSerialNumbers(param);
    serialNumberList = param.serialNumberList;
    return XPUM_OK;
}
} // namespace xpum