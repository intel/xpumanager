/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device_stub.cpp
 */

#include "device/gpu/gpu_device_stub.h"

#include <unistd.h>

#include <algorithm>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <thread>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <regex>
#include <stdio.h>
#include <dirent.h>
#include <climits>
#include <sys/stat.h>
#include <string>

#include "api/api_types.h"
#include "device/frequency.h"
#include "device/memoryEcc.h"
#include "device/performancefactor.h"
#include "device/scheduler.h"
#include "device/standby.h"
#include "gpu_device.h"
#include "infrastructure/configuration.h"
#include "infrastructure/device_property.h"
#include "infrastructure/device_type.h"
#include "infrastructure/exception/level_zero_initialization_exception.h"
#include "infrastructure/handle_lock.h"
#include "infrastructure/logger.h"
#include "infrastructure/measurement_data.h"
#include "firmware/system_cmd.h"

#define MAX_SUB_DEVICE 256

namespace xpum {

std::map<ze_device_handle_t, std::shared_ptr<std::vector<std::shared_ptr<DeviceMetricGroups_t>>>> GPUDeviceStub::device_perf_groups;
const char* GPU_TIME_NAME = "GpuTime";

namespace {

template <class F, class... Args>
void invokeTask(Callback_t callback, F&& f, Args&&... args) {
    auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    auto taskInfo = std::make_tuple(task, callback);

    try {
        auto ret = (std::get<0>(taskInfo))();
        (std::get<1>(taskInfo))(std::static_pointer_cast<void>(ret), nullptr);
    } catch (std::exception& e) {
        std::string error = "Failed to execute task in thread pool:";
        error += e.what();
        XPUM_LOG_DEBUG(error);
        (std::get<1>(taskInfo))(nullptr, std::make_shared<BaseException>(e.what()));
    } catch (...) {
        std::string error =
            "Failed to execute task in thread pool: unexpected exception";
        XPUM_LOG_DEBUG(error);
        (std::get<1>(taskInfo))(nullptr, std::make_shared<BaseException>(error));
    }
}

// used for exclusively runing the RAS APIs to avoid two issues: 1) invalid read/write memory in zesRasGetState; 2) kernel error msg "mei-gsc mei-gscfi.3.auto: id exceeded 256"
std::mutex ras_m;

template <class F, class... Args>
bool checkCapability(const char* device_name, const std::string& bdf_address, const char* capability_name, F&& f, Args&&... args) {
    auto detect_func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    try {
        detect_func();
        return true;
    } catch (BaseException& e) {
        XPUM_LOG_WARN("Device {}{} has no {} capability.", device_name, bdf_address, capability_name);
        XPUM_LOG_WARN("Capability {} detection returned: {}", capability_name, e.what());
    }
    return false;
}
} // namespace

GPUDeviceStub::GPUDeviceStub() : initialized(false) {
    XPUM_LOG_DEBUG("GPUDeviceStub()");
}

GPUDeviceStub::~GPUDeviceStub() {
    // XPUM_LOG_DEBUG("~GPUDeviceStub()");
}

GPUDeviceStub& GPUDeviceStub::instance() {
    static GPUDeviceStub stub;
    std::unique_lock<std::mutex> lock(stub.mutex);
    if (!stub.initialized) {
        stub.init();
    }
    return stub;
}

PCIeManager GPUDeviceStub::pcie_manager;

static std::string getFileValue(std::string file_name) {
    std::ifstream ifs(file_name);
    std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    return content;
}

std::mutex GPUDeviceStub::pvc_idle_power_mutex;
std::map<std::string, std::shared_ptr<MeasurementData>> GPUDeviceStub::pvc_idle_powers;
std::set<std::string> GPUDeviceStub::pvc_gpu_bdfs;
bool GPUDeviceStub::has_pvc_idle_powers = true;

std::shared_ptr<MeasurementData> GPUDeviceStub::loadPVCIdlePowers(std::string bdf, bool fresh, int index) {
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    if (!has_pvc_idle_powers) {
        return ret;
    }

    std::unique_lock<std::mutex> lock(GPUDeviceStub::pvc_idle_power_mutex);
    if (bdf != "" && pvc_idle_powers.count(bdf) > 0) {
        ret = pvc_idle_powers[bdf];
        pvc_idle_powers.erase(bdf);
        if (!fresh)
            has_pvc_idle_powers = false;
        return ret;
    }

    if (has_pvc_idle_powers && pvc_gpu_bdfs.size() == 0) {
        DIR *pdir = NULL;
        struct dirent *pdirent = NULL;
        pdir = opendir("/sys/class/drm");
        if (pdir != NULL) {
            while ((pdirent = readdir(pdir)) != NULL) {
                if (pdirent->d_name[0] == '.') {
                    continue;
                }
                if (strncmp(pdirent->d_name, "render", 6) == 0) {
                    continue;
                }
                if (strncmp(pdirent->d_name, "card", 4) != 0) {
                    continue;
                }
                if (strstr(pdirent->d_name, "-") != NULL) {
                    continue;
                }
                std::string uevent = getFileValue("/sys/class/drm/" + std::string(pdirent->d_name) +"/device/uevent");
                std::string key = "PCI_ID=8086:";
                auto pos = uevent.find(key); 
                if (pos != std::string::npos) {
                    std::string bdf_key = "PCI_SLOT_NAME=";
                    auto bdf_pos = uevent.find(bdf_key); 
                    if (bdf_pos != std::string::npos) {
                        auto device_id = uevent.substr(pos + key.length(), 4);
                        if (device_id.compare(0, 3, "0BD") == 0 || device_id.compare(0, 3, "0BE") == 0)
                            pvc_gpu_bdfs.insert(uevent.substr(bdf_pos + bdf_key.length(), 12));
                    }
                }
            }
            closedir(pdir);
        }
    }
    // PVC Not Found
    if (pvc_gpu_bdfs.size() == 0) {
        has_pvc_idle_powers = false;
        return ret;
    }

    auto gpu_bdfs = pvc_gpu_bdfs;
    if (bdf != "") {
        if (gpu_bdfs.count(bdf) == 0) {
            return ret;
        } else {
            // Only read target PVC idle power
            gpu_bdfs.clear();
            gpu_bdfs.insert(bdf);
        }
    }

    std::map<std::string, std::map<std::uint32_t, std::string>> gpu_bdf_to_power_paths;
    char path[PATH_MAX];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    pdir = opendir("/sys/class/hwmon");
    if (pdir != NULL) {
        while ((pdirent = readdir(pdir)) != NULL) {
            if (pdirent->d_name[0] == '.') {
                continue;
            }
            if (strncmp(pdirent->d_name, "hwmon", 5) != 0) {
                continue;
            }
 
            snprintf(path, PATH_MAX, "/sys/class/hwmon/%s", pdirent->d_name);
            char full_path[PATH_MAX];
            ssize_t full_len = ::readlink(path, full_path, sizeof(full_path));
            full_path[full_len] = '\0';

            for (auto& gpu_bdf: gpu_bdfs) {
                if (strstr(full_path, gpu_bdf.c_str()) != NULL) {
                    std::string name = getFileValue("/sys/class/hwmon/" + std::string(pdirent->d_name) +"/name");
                    name.erase(0, name.find_first_not_of(" \n\r\t"));                                                                                               
                    name.erase(name.find_last_not_of(" \n\r\t") + 1);
                    auto energy_path = "/sys/class/hwmon/" + std::string(pdirent->d_name) +"/energy1_input";
                    uint64_t value = std::stoull(getFileValue(energy_path));
                    auto timestamp = Utility::getCurrentMillisecond();
                    XPUM_LOG_TRACE("[{}] path:{}, value: {}, timestamp: {}", gpu_bdf, energy_path, value, timestamp);
                    if (pvc_idle_powers.count(gpu_bdf) == 0)
                        pvc_idle_powers[gpu_bdf] = std::make_shared<MeasurementData>();

                    pvc_idle_powers[gpu_bdf]->setTimestamp(timestamp);
                    if (name == "i915") {
                        pvc_idle_powers[gpu_bdf]->setCurrent(value);
                        gpu_bdf_to_power_paths[gpu_bdf][UINT32_MAX] = energy_path;
                    } else if (name.find("gt0") != std::string::npos) {
                        pvc_idle_powers[gpu_bdf]->setSubdeviceDataCurrent(0, value);
                        gpu_bdf_to_power_paths[gpu_bdf][0] = energy_path;
                    } else if (name.find("gt1") != std::string::npos) {
                        pvc_idle_powers[gpu_bdf]->setSubdeviceDataCurrent(1, value);
                        gpu_bdf_to_power_paths[gpu_bdf][1] = energy_path;
                    } else if (name.find("gt2") != std::string::npos) {
                        pvc_idle_powers[gpu_bdf]->setSubdeviceDataCurrent(2, value);
                        gpu_bdf_to_power_paths[gpu_bdf][2] = energy_path;
                    } else if (name.find("gt3") != std::string::npos) {
                        pvc_idle_powers[gpu_bdf]->setSubdeviceDataCurrent(3, value);
                        gpu_bdf_to_power_paths[gpu_bdf][3] = energy_path;
                    }
                }
            }
        }
        closedir(pdir);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int device_id = 0;
    for (auto& bdf_data : pvc_idle_powers) {
        // energy:microjoules timestamp:microsecond
        auto begin_time = bdf_data.second->getTimestamp(); 
        auto end_time = Utility::getCurrentMillisecond();
        bdf_data.second->setTimestamp(end_time);
        begin_time *= 1000;
        end_time *= 1000;
        if (bdf_data.second->getCurrent() != std::numeric_limits<uint64_t>::max()) {
            uint64_t value = std::stoull(getFileValue(gpu_bdf_to_power_paths[bdf_data.first][UINT32_MAX]));
            XPUM_LOG_TRACE("[{}] path:{}, value: {}, timestamp: {}", bdf_data.first, gpu_bdf_to_power_paths[bdf_data.first][UINT32_MAX], value, bdf_data.second->getTimestamp());
            auto val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * (value - bdf_data.second->getCurrent()) / (end_time - begin_time);
            bdf_data.second->setCurrent(val);
            bdf_data.second->setAvg(val);
            bdf_data.second->setMax(val);
            bdf_data.second->setMin(val);
            bdf_data.second->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
            bdf_data.second->setDeviceId(std::to_string(device_id));
            XPUM_LOG_DEBUG("[{}] idle power on device: {}", bdf_data.first, bdf_data.second->getCurrent());
        }

        for (uint32_t tile = 0; tile < 4; tile ++) {
            if (bdf_data.second->getSubdeviceDataCurrent(tile) != std::numeric_limits<uint64_t>::max()) {
                uint64_t value = std::stoull(getFileValue(gpu_bdf_to_power_paths[bdf_data.first][tile]));
                XPUM_LOG_TRACE("[{}] path:{}, value: {}, timestamp: {}", bdf_data.first, gpu_bdf_to_power_paths[bdf_data.first][tile], value, bdf_data.second->getTimestamp());
                auto val = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * (value - bdf_data.second->getSubdeviceDataCurrent(tile)) / (end_time - begin_time);
                bdf_data.second->setSubdeviceDataCurrent(tile, val);
                bdf_data.second->setSubdeviceDataMax(tile, val);
                bdf_data.second->setSubdeviceDataMin(tile, val);
                bdf_data.second->setSubdeviceDataAvg(tile, val);
                bdf_data.second->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);            
                bdf_data.second->setDeviceId(std::to_string(device_id));
                XPUM_LOG_DEBUG("[{}] idle power on tile {} : {}", bdf_data.first, tile, bdf_data.second->getSubdeviceDataCurrent(tile));
            }   
        }
        device_id++;
    }
    if (bdf == "") {
        if (pvc_idle_powers.size() == 0 || index >= (int)pvc_idle_powers.size())
            return ret;
        else {
            auto it = pvc_idle_powers.begin();
            std::advance(it, index);
            return it->second;
        }
    } else {
        ret = pvc_idle_powers[bdf];
        pvc_idle_powers.erase(bdf);
        if (!fresh)
            has_pvc_idle_powers = false;
        return ret;
    }
}

void GPUDeviceStub::init() {
    // Add a temporary workaround for PVC idle powers
    loadPVCIdlePowers();

    initialized = true;
    putenv(const_cast<char*>("ZES_ENABLE_SYSMAN=1"));
    putenv(const_cast<char*>("ZE_ENABLE_PCI_ID_DEVICE_ORDER=1"));
    if (std::getenv("ZET_ENABLE_METRICS") == NULL && std::any_of(Configuration::getEnabledMetrics().begin(), Configuration::getEnabledMetrics().end(),
                                                                 [](const MeasurementType type) { return type == METRIC_EU_ACTIVE || type == METRIC_EU_IDLE || type == METRIC_EU_STALL || type == METRIC_PERF; })) {
        putenv(const_cast<char*>("ZET_ENABLE_METRICS=1"));
    }

    ze_result_t ret = zeInit(0);
    if (ret != ZE_RESULT_SUCCESS) {
        XPUM_LOG_ERROR("GPUDeviceStub::init zeInit error: {0:x}", ret);
        checkInitDependency();
        throw LevelZeroInitializationException("zeInit error");
    }

    if (Configuration::INITIALIZE_PCIE_MANAGER) {
        pcie_manager.init();
    }
}

void GPUDeviceStub::checkInitDependency() {
    XPUM_LOG_INFO("GPUDeviceStub::checkInitDependency start");
    std::string details;

    std::vector<std::string> checkEnvVaribles = {"ZES_ENABLE_SYSMAN"};
    if (std::any_of(Configuration::getEnabledMetrics().begin(), Configuration::getEnabledMetrics().end(),
                    [](const MeasurementType type) { return type == METRIC_EU_ACTIVE || type == METRIC_EU_IDLE || type == METRIC_EU_STALL; })) {
        checkEnvVaribles.push_back("ZET_ENABLE_METRICS");
    }

    bool findEnvVaribles = true;
    for (auto it = checkEnvVaribles.begin(); it != checkEnvVaribles.end(); it++) {
        if (std::getenv((*it).c_str()) == nullptr) {
            findEnvVaribles = false;
            details = (*it);
            break;
        }
    }
    if (findEnvVaribles) {
        XPUM_LOG_INFO("Environment variables check pass");
    } else {
        XPUM_LOG_ERROR("Environment variables check failed. " + details + " is missing.");
    }

    std::vector<std::string> libs = {"libze_loader.so.1", "libze_intel_gpu.so.1"};
    bool findLibs = true;
    for (auto it = libs.begin(); it != libs.end(); it++) {
        void* handle = dlopen((*it).c_str(), RTLD_NOW);
        if (!handle) {
            findLibs = false;
            details = (*it);
            break;
        }
    }

    if (findLibs) {
        XPUM_LOG_INFO("Libraries check pass.");
    } else {
        XPUM_LOG_ERROR("Libraries check failed. " + details + " is missing.");
    }

    std::string dirName = "/dev/dri";
    DIR* dir = opendir(dirName.c_str());
    struct dirent* ent;
    if (nullptr != dir) {
        bool hasPermission = true;
        ent = readdir(dir);
        while (nullptr != ent) {
            std::string entryName = ent->d_name;
            if (isDevEntry(entryName)) {
                std::stringstream ss;
                ss << dirName << "/" << entryName;
                int ret = access(ss.str().c_str(), 4);
                if (ret != 0) {
                    hasPermission = false;
                    details = ss.str();
                    break;
                }
            }
            ent = readdir(dir);
        }
        closedir(dir);

        if (hasPermission) {
            XPUM_LOG_INFO("Permission check pass.");
        } else {
            XPUM_LOG_ERROR("Permission check failed. Access " + details + " failed.");
        }
    } else {
        XPUM_LOG_ERROR("Permission check failed.");
    }

    XPUM_LOG_INFO("GPUDeviceStub::checkInitDependency done");
}

bool GPUDeviceStub::isDevEntry(const std::string& entryName) {
    if (entryName.compare(0, 7, "renderD") == 0) {
        for (std::size_t i = 7; i < entryName.size(); i++) {
            if (!isdigit(entryName.at(i)))
                return false;
        }
        return true;
    }
    return false;
}

void GPUDeviceStub::discoverDevices(Callback_t callback) {
    invokeTask(callback, toDiscover);
}

static const std::string PCI_FILE_SYS("sys");
static const std::string PCI_FILE_DEVICES("devices");
static const std::string PCI_FILE_DRM("drm");
static std::deque<std::string> getParentPciBridges(const std::string& origin_str) {
    std::deque<std::string> res;
    if (!origin_str.empty()) {
        const char* ostr = origin_str.c_str();
        const int len = origin_str.length();
        std::string nstr;
        for (int i = 0; i < len; i++) {
            const char cc = ostr[i];
            switch (cc) {
                case '/':
                    if (!nstr.empty()) {
                        if (nstr == PCI_FILE_DRM)
                            break;
                        if (nstr != ".." && nstr != PCI_FILE_SYS && nstr != PCI_FILE_DEVICES) {
                            res.push_front(nstr);
                        }
                        nstr.clear();
                    }
                    break;
                default:
                    nstr += cc;
                    break;
            }
        }
    }
    return res;
}

static const std::string SYSTEM_SLOT_NAME_MARKER("Designation:");
static const std::string SYSTEM_SLOT_BUS_ADDRESS_MARKER("Bus Address:");
static const std::string SYSTEM_SLOT_CURRENT_USAGE_MARKER("Current Usage:");
static const std::string SYSTEM_INFO_IGNORED_STARTER(" \t");
static const std::string SYSTEM_INFO_IGNORED_ENDER("\r\n");
static std::string getValueAtMarker(const std::string& sysInfo, const std::string& marker) {
    std::string res;
    std::string spaces;
    size_t mPos = sysInfo.find(marker);
    if (mPos != std::string::npos) {
        const int len = sysInfo.length();
        int i = mPos + marker.length();
        while (i < len && SYSTEM_INFO_IGNORED_STARTER.find(sysInfo.at(i)) != std::string::npos) i++;
        char cc;
        while (i < len && SYSTEM_INFO_IGNORED_ENDER.find(cc = sysInfo.at(i)) == std::string::npos) {
            switch (cc) {
                case ' ':
                case '\t':
                    spaces += cc;
                    break;
                default:
                    if (!spaces.empty()) {
                        res += spaces;
                        spaces.clear();
                    }
                    res += cc;
                    break;
            }
            i++;
        }
    }
    return res;
}

static const std::string SYSTEM_SLOT_IN_USE("In Use");
class DMISystemSlot {
    std::string _name;
    std::string _busAddress;
    std::string _currentUsage;

   public:
    DMISystemSlot(const std::string& slotInfo) {
        _name = getValueAtMarker(slotInfo, SYSTEM_SLOT_NAME_MARKER);
        _busAddress = getValueAtMarker(slotInfo, SYSTEM_SLOT_BUS_ADDRESS_MARKER);
        _currentUsage = getValueAtMarker(slotInfo, SYSTEM_SLOT_CURRENT_USAGE_MARKER);
    }

    const std::string& name() {
        return _name;
    }

    const std::string& busAddress() {
        return _busAddress;
    }

    const std::string& currentUsage() {
        return _currentUsage;
    }

    bool inUse() {
        return _currentUsage == SYSTEM_SLOT_IN_USE;
    }
};

static const std::string SYSTEM_SLOT_MARKER("System Slot Information");
static std::vector<DMISystemSlot> getSystemSlotBlocks(const std::string& ssInfos) {
    std::vector<DMISystemSlot> res;
    size_t curPos = 0;
    size_t nextPos;
    while ((nextPos = ssInfos.find(SYSTEM_SLOT_MARKER, curPos)) != std::string::npos) {
        if (curPos > 0) {
            res.push_back(DMISystemSlot(ssInfos.substr(curPos, nextPos - curPos)));
        }
        curPos = nextPos + SYSTEM_SLOT_MARKER.length();
    }
    if (curPos > 0) {
        res.push_back(DMISystemSlot(ssInfos.substr(curPos)));
    }
    return res;
}

static std::string getCardFullPath(std::string& bdf_address) {
    char link_path[PATH_MAX];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    int len = 0;
    std::string ret = "";

    pdir = opendir("/sys/class/drm");
    if (pdir == NULL) {
        return ret;
    }

    while ((pdirent = readdir(pdir)) != NULL) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        if (strncmp(pdirent->d_name, "card", 4) != 0) {
            continue;
        }
        if (strstr(pdirent->d_name, "-") != NULL) {
            continue;
        }
        len = snprintf(link_path, PATH_MAX, "/sys/class/drm/%s", pdirent->d_name);
        if (len <= 0 || len >= PATH_MAX) {
            break;
        }
        char full_path[PATH_MAX];
        ssize_t full_len = ::readlink(link_path, full_path, sizeof(full_path));
        full_path[full_len] = '\0';

        if (strstr(full_path, bdf_address.c_str()) != NULL) {
            ret = full_path;
            break;
        }
    }
    closedir(pdir);
    return ret;
}
static bool readStrSysFsFile(char *buf, char *fileName);

std::string GPUDeviceStub::getOAMSocketId(zes_pci_address_t address) {
    std::string bdf_address = to_string(address);
    char link_path[PATH_MAX];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    int len = 0;
    std::string ret = "";

    pdir = opendir("/sys/class/drm");
    if (pdir == NULL) {
        return ret;
    }

    while ((pdirent = readdir(pdir)) != NULL) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        
        if (strstr(pdirent->d_name, "-") != NULL) {
            continue;
        }

        if (!strncmp(pdirent->d_name, "card", 4)) {
            len = snprintf(link_path, PATH_MAX, "/sys/class/drm/%s", pdirent->d_name);
            if (len <= 0 || len >= PATH_MAX) {
                break;
            }
            char full_path[PATH_MAX];
            char card_path[PATH_MAX];
            char socketId_buf[16];

            ssize_t full_len = ::readlink(link_path, full_path, sizeof(full_path));
            full_path[full_len] = '\0';
            if (strstr(full_path, bdf_address.c_str()) != NULL) {
                int cardLen = snprintf(card_path, PATH_MAX,"%s/iaf_socket_id",link_path);
                if (cardLen <= 0 || cardLen >= PATH_MAX) {
                    return ret;
                }
                if (readStrSysFsFile(socketId_buf, card_path) == false) {
                    return ret;
                } else {
                    if(!strncmp(socketId_buf, "0x1f", 4)) {
                        return ret;
                    }
                    socketId_buf[strlen(socketId_buf)-1] = '\0';
                    return socketId_buf;
                }
                break;
            }
        }
    }
    closedir(pdir);
    return ret;
}

std::string GPUDeviceStub::getPciSlot(zes_pci_address_t address) {
    std::string res;
    std::string bdf = to_string(address);
    std::string card_full_path = getCardFullPath(bdf);
    SystemCommandResult ss_res = execCommand("dmidecode -t 9 2>/dev/null");

    if (card_full_path.size() > 0 && ss_res.exitStatus() == 0) {
        /* 
            Add a temporary workaround for SMC servers because they return
            GPU BDF as bus address of a slot. Here the BDF of a GPU would be 
            added to match a slot. For a GPU which is not showed in
            dmidecode (smibios), the slot name would be updated when
            buildin groups were created.
            For Intel servers, the behavior need to be changed to follow
            smbios spec 3.3 (bus address of a slot sould be an endpoint
            instead of a bridge/ switch) once Intel smbios implementaion
            is updated.  
        */
        std::deque<std::string> allBdf = getParentPciBridges(card_full_path);
        std::vector<DMISystemSlot> systemSlots = getSystemSlotBlocks(ss_res.output());
        for (auto& pBdf : allBdf) {
            for (auto& sysSlot : systemSlots) {
                if (sysSlot.inUse() && sysSlot.busAddress() == pBdf) {
                    res = sysSlot.name();
                    break;
                }
                if (!res.empty()) break;
            }
        }
    }
    return res;
}

static std::string getDriverVersion() {
    std::string version;
    std::string release;
    std::string name = "intel-i915-dkms";
    std::string rpm_cmd = "rpm -qa 2>/dev/null| grep " + name + " 2>/dev/null";
    SystemCommandResult rpm_res = execCommand(rpm_cmd);
    if (rpm_res.exitStatus() == 0) {
        std::string strData = rpm_res.output();
        auto pos1 = strData.find(name);
        pos1 += name.length();
        pos1 = strData.find_first_of("0123456789",pos1);
        auto pos2 = strData.find_first_of("-",pos1);
        version = strData.substr(pos1,pos2-pos1);
        pos1 = pos2 + 1;
        pos2 = strData.find_first_of(".",pos1);
        release = strData.substr(pos1,pos2-pos1);
        version = version + "-" + release;
    } else {
        std::string deb_cmd = "dpkg -l 2>/dev/null| grep " + name + " 2>/dev/null";
        SystemCommandResult deb_res = execCommand(deb_cmd);
        if (deb_res.exitStatus() == 0) {
            std::string strData = deb_res.output();
            auto pos1 = strData.find(name);
            pos1 += name.length();
            pos1 = strData.find_first_of("0123456789", pos1);
            auto pos2 = strData.find_first_of(" ", pos1);
            version = strData.substr(pos1, pos2 - pos1);
        }
    }
    return version;
}

static std::string getKernelVersion() {
    std::string version;
    std::string cmd = "uname -r";
    SystemCommandResult res = execCommand(cmd);
    if (res.exitStatus() == 0) {
        version = res.output();
        version.erase(std::remove_if(version.begin(), version.end(),
                                     [](unsigned char x) { return !std::isprint(x); }),
                      version.end());
    }
    return version;
}

void GPUDeviceStub::addCapabilities(zes_device_handle_t device, const zes_device_properties_t& props, std::vector<DeviceCapability>& capabilities) {
    zes_pci_properties_t pci_props;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    std::string bdf_address;
    if (res == ZE_RESULT_SUCCESS)
        bdf_address = to_string(pci_props.address);
    else
        XPUM_LOG_WARN("Failed to get to device properties, zesDevicePciGetProperties returned: {}", res);

    if (checkCapability(props.core.name, bdf_address, "Power", toGetPower, device))
        capabilities.push_back(DeviceCapability::METRIC_POWER);
    if (checkCapability(props.core.name, bdf_address, "Actual Request Frequency", toGetActuralRequestFrequency, device))
        capabilities.push_back(DeviceCapability::METRIC_FREQUENCY);
    if (checkCapability(props.core.name, bdf_address, "GPU Temperature", toGetTemperature, device, ZES_TEMP_SENSORS_GPU))
        capabilities.push_back(DeviceCapability::METRIC_TEMPERATURE);
    if (checkCapability(props.core.name, bdf_address, "Memory Temperature", toGetTemperature, device, ZES_TEMP_SENSORS_MEMORY))
        capabilities.push_back(DeviceCapability::METRIC_MEMORY_TEMPERATURE);
    if (checkCapability(props.core.name, bdf_address, "Memory Used Utilization", toGetMemoryUsedUtilization, device))
        capabilities.push_back(DeviceCapability::METRIC_MEMORY_USED_UTILIZATION);
    if (checkCapability(props.core.name, bdf_address, "Memory Bandwidth", toGetMemoryBandwidth, device))
        capabilities.push_back(DeviceCapability::METRIC_MEMORY_BANDWIDTH);
    if (checkCapability(props.core.name, bdf_address, "Memory Read Write Throughput", toGetMemoryReadWrite, device))
        capabilities.push_back(DeviceCapability::METRIC_MEMORY_READ_WRITE_THROUGHPUT);
    if (checkCapability(props.core.name, bdf_address, "GPU Utilization", toGetGPUUtilization, device))
        capabilities.push_back(DeviceCapability::METRIC_COMPUTATION);
    if (checkCapability(props.core.name, bdf_address, "Engine Utilization", toGetEngineUtilization, device))
        capabilities.push_back(DeviceCapability::METRIC_ENGINE_UTILIZATION);
    if (checkCapability(props.core.name, bdf_address, "Energy", toGetEnergy, device))
        capabilities.push_back(DeviceCapability::METRIC_ENERGY);
    if (checkCapability(props.core.name, bdf_address, "Ras Error", toGetRasErrorOnSubdevice, device))
        capabilities.push_back(DeviceCapability::METRIC_RAS_ERROR);
    if (checkCapability(props.core.name, bdf_address, "Frequency Throttle", toGetFrequencyThrottle, device))
        capabilities.push_back(DeviceCapability::METRIC_FREQUENCY_THROTTLE);
    if (checkCapability(props.core.name, bdf_address, "Frequency Throttle Reason(GPU)", toGetFrequencyThrottleReason, device))
        capabilities.push_back(DeviceCapability::METRIC_FREQUENCY_THROTTLE_REASON_GPU);
    for (auto metric: Configuration::getEnabledMetrics()) {
        if (metric == METRIC_PCIE_READ_THROUGHPUT) {
            if (checkCapability(props.core.name, bdf_address, "PCIe read throughput", toGetPCIeReadThroughput, device))
                capabilities.push_back(DeviceCapability::METRIC_PCIE_READ_THROUGHPUT);
        } else if (metric == METRIC_PCIE_WRITE_THROUGHPUT) {
            if (checkCapability(props.core.name, bdf_address, "PCIe write throughput", toGetPCIeWriteThroughput, device))
                capabilities.push_back(DeviceCapability::METRIC_PCIE_WRITE_THROUGHPUT);
        } else if (metric == METRIC_PCIE_READ) {
            if (checkCapability(props.core.name, bdf_address, "PCIe read", toGetPCIeRead, device))
                capabilities.push_back(DeviceCapability::METRIC_PCIE_READ);
        } else if (metric == METRIC_PCIE_WRITE) {
            if (checkCapability(props.core.name, bdf_address, "PCIe write", toGetPCIeWrite, device))
                capabilities.push_back(DeviceCapability::METRIC_PCIE_WRITE);   
        }
    }
    if (checkCapability(props.core.name, bdf_address, "fabric throughput", toGetFabricThroughput, device))
        capabilities.push_back(DeviceCapability::METRIC_FABRIC_THROUGHPUT);
}

void GPUDeviceStub::addEuActiveStallIdleCapabilities(zes_device_handle_t device, const zes_device_properties_t& props, ze_driver_handle_t driver, std::vector<DeviceCapability>& capabilities) {
    if (!std::any_of(Configuration::getEnabledMetrics().begin(), Configuration::getEnabledMetrics().end(),
                     [](const MeasurementType type) { return type == METRIC_EU_ACTIVE || type == METRIC_EU_IDLE || type == METRIC_EU_STALL; })) {
        return;
    }
    zes_pci_properties_t pci_props;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    std::string bdf_address;
    if (res == ZE_RESULT_SUCCESS)
        bdf_address = to_string(pci_props.address);
    else
        XPUM_LOG_WARN("Failed to get to device properties, zesDevicePciGetProperties returned: {}", res);
    try {
        toGetEuActiveStallIdle(device, driver, MeasurementType::METRIC_EU_ACTIVE);
        capabilities.push_back(DeviceCapability::METRIC_EU_ACTIVE_STALL_IDLE);
    } catch (BaseException& e) {
        if (strcmp(e.what(), "toGetEuActiveStallIdleCore - zetMetricStreamerOpen") == 0) {
            XPUM_LOG_WARN("Device {}{} has no Active/Stall/Idle monitoring capability. Or because there are other applications on the current machine that are monitoring related data, XPUM cannot monitor these data at the same time.", props.core.name, bdf_address);
        } else if (strcmp(e.what(), "toGetEuActiveStallIdleCore - abnormal EU data") == 0) {
            XPUM_LOG_WARN("Device {}{} has no Active/Stall/Idle monitoring capability due to abnormal EU data.", props.core.name, bdf_address);
        } else {
            XPUM_LOG_WARN("Device {}{} has no Active/Stall/Idle monitoring capability.", props.core.name, bdf_address);
        }
        XPUM_LOG_DEBUG("Capability EU Active/Stall/Idle detection returned: {}", e.what());
    }
}

void GPUDeviceStub::addEngineCapabilities(zes_device_handle_t device, const zes_device_properties_t& props, std::vector<DeviceCapability>& capabilities) {
    ze_result_t res;
    uint32_t engine_grp_count = 0;
    zes_pci_properties_t pci_props;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    std::string bdf_address;
    if (res == ZE_RESULT_SUCCESS)
        bdf_address = to_string(pci_props.address);
    else
        XPUM_LOG_WARN("Failed to get to device properties, zesDevicePciGetProperties returned: {}", res);

    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    std::set<zes_engine_group_t> engine_caps;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_grp_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_engine_handle_t> engines(engine_grp_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_grp_count, engines.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& engine : engines) {
                zes_engine_properties_t props;
                XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    engine_caps.emplace(props.type);
                } else {
                    XPUM_LOG_WARN("Failed to get to get engine properties, zesEngineGetProperties returned: {}", res);
                }
            }
        } else {
            XPUM_LOG_WARN("Failed to get to enum engine groups properties, zesDeviceEnumEngineGroups returned: {}", res);
        }
    } else {
        XPUM_LOG_WARN("Failed to get to enum engine groups properties, zesDeviceEnumEngineGroups returned: {}", res);
    }
    if (engine_caps.find(ZES_ENGINE_GROUP_COMPUTE_ALL) != engine_caps.end())
        capabilities.push_back(DeviceCapability::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION);
    else
        XPUM_LOG_WARN("Device {}{} has no Compute Engine Group Utilization monitoring capability.", props.core.name, bdf_address);
    if (engine_caps.find(ZES_ENGINE_GROUP_MEDIA_ALL) != engine_caps.end())
        capabilities.push_back(DeviceCapability::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION);
    else
        XPUM_LOG_WARN("Device {}{} has no Media Engine Group Utilization monitoring capability.", props.core.name, bdf_address);
    if (engine_caps.find(ZES_ENGINE_GROUP_COPY_ALL) != engine_caps.end())
        capabilities.push_back(DeviceCapability::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION);
    else
        XPUM_LOG_WARN("Device {}{} has no Copy Engine Group Utilization monitoring capability.", props.core.name, bdf_address);
    if (engine_caps.find(ZES_ENGINE_GROUP_RENDER_ALL) != engine_caps.end())
        capabilities.push_back(DeviceCapability::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION);
    else
        XPUM_LOG_WARN("Device {}{} has no Render Engine Group Utilization monitoring capability.", props.core.name, bdf_address);
    if (engine_caps.find(ZES_ENGINE_GROUP_3D_ALL) != engine_caps.end())
        capabilities.push_back(DeviceCapability::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION);
    else
        XPUM_LOG_WARN("Device {}{} has no 3D Engine Group Utilization monitoring capability.", props.core.name, bdf_address);
}

void addPCIeProperties(ze_device_handle_t& device, std::shared_ptr<GPUDevice> p_gpu) {
    using namespace std;
    zes_pci_properties_t data;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &data));
    if (res == ZE_RESULT_SUCCESS) {
        p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_GENERATION, std::to_string(data.maxSpeed.gen)));
        p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_MAX_LINK_WIDTH, std::to_string(data.maxSpeed.width)));
    }
}

void GPUDeviceStub::logSupportedMetrics(zes_device_handle_t device, const zes_device_properties_t& props, const std::vector<DeviceCapability>& capabilities) {
    auto metric_types = Configuration::getEnabledMetrics();
    for (auto metric = metric_types.begin(); metric != metric_types.end();) {
        if (std::none_of(capabilities.begin(), capabilities.end(), [metric](xpum::DeviceCapability cap) { return (cap == Utility::capabilityFromMeasurementType(*metric)); })) {
            metric = metric_types.erase(metric);
        } else {
            metric++;
        }
    }
    zes_pci_properties_t pci_props;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    std::string bdf_address;
    if (res == ZE_RESULT_SUCCESS)
        bdf_address = to_string(pci_props.address);
    else
        XPUM_LOG_WARN("Failed to get to device properties, zesDevicePciGetProperties returned: {}", res);

    std::string log_content;
    std::vector<std::string> metric_names;
    for (auto iter = metric_types.begin(); iter != metric_types.end(); ++iter) {
        if (std::next(iter) != metric_types.end()) {
            log_content += (Utility::getXpumStatsTypeString(*iter) + std::string(", "));
        } else {
            log_content += (Utility::getXpumStatsTypeString(*iter) + std::string("."));
        }
    }
    XPUM_LOG_INFO("Device {}{} has the following monitoring metric types: {}", props.core.name, bdf_address, log_content);
}

std::shared_ptr<std::vector<std::shared_ptr<Device>>> GPUDeviceStub::toDiscover() {
    auto p_devices = std::make_shared<std::vector<std::shared_ptr<Device>>>();
    uint32_t driver_count = 0;
    ze_result_t res;
    zeDriverGet(&driver_count, nullptr);
    std::vector<ze_driver_handle_t> drivers(driver_count);
    zeDriverGet(&driver_count, drivers.data());

    for (auto& p_driver : drivers) {
        uint32_t device_count = 0;
        XPUM_ZE_HANDLE_LOCK(p_driver, zeDeviceGet(p_driver, &device_count, nullptr));
        std::vector<ze_device_handle_t> devices(device_count);
        XPUM_ZE_HANDLE_LOCK(p_driver, zeDeviceGet(p_driver, &device_count, devices.data()));
        ze_driver_properties_t driver_prop;
        XPUM_ZE_HANDLE_LOCK(p_driver, zeDriverGetProperties(p_driver, &driver_prop));

        for (auto& device : devices) {
            std::vector<DeviceCapability> capabilities;
            zes_device_handle_t zes_device = (zes_device_handle_t)device;
            zes_device_properties_t props = {};
            props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            XPUM_ZE_HANDLE_LOCK(zes_device, zesDeviceGetProperties(zes_device, &props));
            if (props.core.type == ZE_DEVICE_TYPE_GPU) {
                addCapabilities(device, props, capabilities);
                addEngineCapabilities(device, props, capabilities);
                addEuActiveStallIdleCapabilities(device, props, p_driver, capabilities);
                logSupportedMetrics(device, props, capabilities);
                auto p_gpu = std::make_shared<GPUDevice>(std::to_string(p_devices->size()), zes_device, device, p_driver, capabilities);
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_TYPE, std::string("GPU")));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, to_hex_string(props.core.deviceId)));
                // p_gpu->addProperty(Property(DeviceProperty::BOARD_NUMBER,std::string(props.boardNumber)));
                // p_gpu->addProperty(Property(DeviceProperty::BRAND_NAME,std::string(props.brandName)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DRIVER_VERSION, getDriverVersion()));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_LINUX_KERNEL_VERSION, getKernelVersion()));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_SERIAL_NUMBER, std::string(props.serialNumber)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_VENDOR_NAME, std::string(props.vendorName)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_CORE_CLOCK_RATE_MHZ, std::to_string(props.core.coreClockRate)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MAX_MEM_ALLOC_SIZE_BYTE, std::to_string(props.core.maxMemAllocSize)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MAX_HARDWARE_CONTEXTS, std::to_string(props.core.maxHardwareContexts)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MAX_COMMAND_QUEUE_PRIORITY, std::to_string(props.core.maxCommandQueuePriority)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, std::string(props.core.name)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS_PER_SUB_SLICE, std::to_string(props.core.numEUsPerSubslice)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUB_SLICES_PER_SLICE, std::to_string(props.core.numSubslicesPerSlice)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SLICES, std::to_string(props.core.numSlices)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_THREADS_PER_EU, std::to_string(props.core.numThreadsPerEU)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PHYSICAL_EU_SIMD_WIDTH, std::to_string(props.core.physicalEUSimdWidth)));
                // p_gpu->addProperty(Property(DeviceProperty::TIMER_RESOLUTION,std::to_string(props.core.timerResolution)));
                // p_gpu->addProperty(Property(DeviceProperty::TIMESTAMP_VALID_BITS,std::to_string(props.core.timestampValidBits)));
                auto& uuidBuf = props.core.uuid.id;
                char uuidStr[37] = {};
                sprintf(uuidStr,
                        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                        uuidBuf[15], uuidBuf[14], uuidBuf[13], uuidBuf[12], uuidBuf[11], uuidBuf[10], uuidBuf[9], uuidBuf[8],
                        uuidBuf[7], uuidBuf[6], uuidBuf[5], uuidBuf[4], uuidBuf[3], uuidBuf[2], uuidBuf[1], uuidBuf[0]);
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_UUID, std::string(uuidStr)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_VENDOR_ID, to_hex_string(props.core.vendorId)));
                // p_gpu->addProperty(Property(DeviceProperty::KERNEL_TIMESTAMP_VALID_BITS,std::to_string(props.core.kernelTimestampValidBits)));
                // p_gpu->addProperty(Property(DeviceProperty::FLAGS,std::to_string(props.core.flags)));

                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUBDEVICE, std::to_string(props.numSubdevices)));
                uint32_t tileCount = props.numSubdevices == 0 ? 1 : props.numSubdevices;
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, std::to_string(tileCount)));
                uint32_t euCount = tileCount * props.core.numSlices * props.core.numSubslicesPerSlice * props.core.numEUsPerSubslice;
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS, std::to_string(euCount)));
                zes_pci_properties_t pci_props;

                XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
                if (res == ZE_RESULT_SUCCESS) {
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, to_string(pci_props.address)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE, getDRMDevice(pci_props)));
                    auto tmpAddr = pci_props.address;
                    p_gpu->setPciAddress({tmpAddr.domain, tmpAddr.bus, tmpAddr.device, tmpAddr.function});
                    std::string stepping = "unknown";
                    std::ifstream infile("/sys/bus/pci/devices/" + to_string(pci_props.address) + "/revision");
                    if (infile.good()) {
                        std::string rev;
                        getline(infile, rev);
                        if (rev.size() > 0) {
                            int val = std::stoi(rev, nullptr, 16);
                            if ((props.core.deviceId/0x10 == 0x0bd) || props.core.deviceId == 0x0be5) {
                                std::map<int, std::string> pvc_steppings = {{0x00, "A0"}, {0x01, "A0p"}, {0x03, "A0"},
                                                                            {0x1E, "B0"}, {0x26, "B1"}, {0x2E, "B3"}, {0x2F, "B4"}};
                                if (pvc_steppings.count(val) > 0) {
                                    stepping = pvc_steppings[val];
                                }
                            } else {
                                if (val >= 0 && val < 8) // A0 ~ A3, B0 ~ B3
                                    stepping = (char)('A' + val / 4) + std::to_string(val % 4);
                                else if (val >= 8 && val < 18) // C0 ~ C9
                                    stepping = (char)('C') + std::to_string(val - 8);
                            }
                        }                        
                    }
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_STEPPING, stepping));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_SLOT, getPciSlot(pci_props.address)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_OAM_SOCKET_ID, getOAMSocketId(pci_props.address)));
                }

                uint64_t physical_size = 0;
                uint64_t free_size = 0;
                uint32_t mem_module_count = 0;
                //zes_mem_health_t memory_health = ZES_MEM_HEALTH_OK;
                XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
                std::vector<zes_mem_handle_t> mems(mem_module_count);
                XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
                if (res == ZE_RESULT_SUCCESS) {
                    for (auto& mem : mems) {
                        uint64_t mem_module_physical_size = 0;
                        zes_mem_properties_t props;
                        props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                        XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetProperties(mem, &props));
                        if (res == ZE_RESULT_SUCCESS) {
                            mem_module_physical_size = props.physicalSize;
                            int32_t mem_bus_width = props.busWidth;
                            int32_t mem_channel_num = props.numChannels;
                            p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_BUS_WIDTH, std::to_string(mem_bus_width)));
                            p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEMORY_CHANNELS, std::to_string(mem_channel_num)));
                        }

                        zes_mem_state_t sysman_memory_state = {};
                        sysman_memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
                        XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetState(mem, &sysman_memory_state));
                        if (res == ZE_RESULT_SUCCESS) {
                            if (props.physicalSize == 0) {
                                mem_module_physical_size = sysman_memory_state.size;
                            }
                            physical_size += mem_module_physical_size;
                            free_size += sysman_memory_state.free;
                            if (sysman_memory_state.health != zes_mem_health_t::ZES_MEM_HEALTH_OK) {
                                //memory_health = sysman_memory_state.health;
                            }
                        }
                    }
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_PHYSICAL_SIZE_BYTE, std::to_string(physical_size)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_FREE_SIZE_BYTE, std::to_string(free_size)));
                    // p_gpu->addProperty(Property(DeviceProperty::MEMORY_HEALTH,get_health_state_string(memory_health)));
                }

                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_NAME, std::string("GFX")));
                std::string fwVersion = "";
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION, fwVersion));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_NAME, std::string("GFX_DATA")));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION, fwVersion));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_NAME, std::string("AMC")));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_VERSION, fwVersion));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_NAME, std::string("GFX_PSCBIN")));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_VERSION, fwVersion));

                uint32_t fabric_count = 0;
                XPUM_ZE_HANDLE_LOCK(device, zesDeviceEnumFabricPorts(device, &fabric_count, nullptr));
                if (fabric_count > 0) {
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_NUMBER, std::to_string(fabric_count)));
                    std::vector<zes_fabric_port_handle_t> fps(fabric_count);
                    XPUM_ZE_HANDLE_LOCK(device, zesDeviceEnumFabricPorts(device, &fabric_count, fps.data()));
                    if (res == ZE_RESULT_SUCCESS) {
                        for (auto& fp : fps) {
                            zes_fabric_port_properties_t props;
                            XPUM_ZE_HANDLE_LOCK(device, res = zesFabricPortGetProperties(fp, &props));
                            p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_MAX_RX_SPEED, props.maxRxSpeed.bitRate));
                            p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_MAX_TX_SPEED, props.maxTxSpeed.bitRate));
                            p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_RX_LANES_NUMBER, props.maxRxSpeed.width));
                            p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_TX_LANES_NUMBER, props.maxTxSpeed.width));
                        }
                    }
                }

                uint32_t engine_grp_count;
                uint32_t media_engine_count = 0;
                uint32_t meida_enhancement_engine_count = 0;
                XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_grp_count, nullptr));
                if (res == ZE_RESULT_SUCCESS) {
                    std::vector<zes_engine_handle_t> engines(engine_grp_count);
                    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_grp_count, engines.data()));
                    if (res == ZE_RESULT_SUCCESS) {
                        for (auto& engine : engines) {
                            zes_engine_properties_t props;
                            props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                            XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, &props));
                            if (res == ZE_RESULT_SUCCESS) {
                                if (props.type == ZES_ENGINE_GROUP_COMPUTE_SINGLE || props.type == ZES_ENGINE_GROUP_RENDER_SINGLE || props.type == ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE || props.type == ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE || props.type == ZES_ENGINE_GROUP_COPY_SINGLE || props.type == ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE || props.type == ZES_ENGINE_GROUP_3D_SINGLE) {
                                    p_gpu->addEngine((uint64_t)engine, props.type, props.onSubdevice, props.subdeviceId);
                                }
                                if (props.type == ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE) {
                                    media_engine_count += 1;
                                }
                                if (props.type == ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE) {
                                    meida_enhancement_engine_count += 1;
                                }
                            }
                        }
                    }
                }
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENGINES, std::to_string(media_engine_count)));
                p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENH_ENGINES, std::to_string(meida_enhancement_engine_count)));
                addPCIeProperties(device, p_gpu);

                p_devices->push_back(p_gpu);
            }
        }
    }

    return p_devices;
}

std::string GPUDeviceStub::getDRMDevice(const zes_pci_properties_t& pci_props) {
    char path[PATH_MAX];
    char buf[128];
    char uevent[1024];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    int len = 0;
    std::string ret = "";

    pdir = opendir("/sys/class/drm");
    if (pdir == NULL) {
        return ret;
    }

    while ((pdirent = readdir(pdir)) != NULL) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        if (strncmp(pdirent->d_name, "card", 4) != 0) {
            continue;
        }
        if (strstr(pdirent->d_name, "-") != NULL) {
            continue;
        }
        len = snprintf(path, PATH_MAX, "/sys/class/drm/%s/device/uevent",
                pdirent->d_name);
        if (len <= 0 || len >= PATH_MAX) {
            break;
        }
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            break;
        }
        int szRead = read(fd, uevent, 1024);
        close(fd);
        if (szRead < 0 || szRead >= 1024) {
            break;
        }
        uevent[szRead] = 0;
        len = snprintf(buf, 128, "%04d:%02x:%02x.%x",
                pci_props.address.domain, pci_props.address.bus,
                pci_props.address.device, pci_props.address.function);
        if (strstr(uevent, buf) != NULL) {
            ret = "/dev/dri/";
            ret += pdirent->d_name;
            break;
        }
    }
    closedir(pdir);
    return ret;
}

std::string GPUDeviceStub::get_health_state_string(zes_mem_health_t val) {
    switch (val) {
        case zes_mem_health_t::ZES_MEM_HEALTH_UNKNOWN:
            return std::string("The memory health cannot be determined.");
        case zes_mem_health_t::ZES_MEM_HEALTH_OK:
            return std::string("All memory channels are healthy.");
        case zes_mem_health_t::ZES_MEM_HEALTH_DEGRADED:
            return std::string("Excessive correctable errors have been detected on one or more channels. Device should be reset.");
        case zes_mem_health_t::ZES_MEM_HEALTH_CRITICAL:
            return std::string("Operating with reduced memory to cover banks with too many uncorrectable errors.");
        case zes_mem_health_t::ZES_MEM_HEALTH_REPLACE:
            return std::string("Device should be replaced due to excessive uncorrectable errors.");
        default:
            return std::string("The memory health cannot be determined.");
    }
}

std::string GPUDeviceStub::get_freq_throttle_string(zes_freq_throttle_reason_flags_t flags) {
    if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP) == ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP)
        return std::string("frequency throttled due to average power excursion.");
    if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP) == ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP)
        return std::string("frequency throttled due to burst power excursion.");
    if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT) == ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT)
        return std::string("frequency throttled due to current excursion.");
    if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT) == ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT)
        return std::string("frequency throttled due to thermal excursion.");
    if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT) == ZES_FREQ_THROTTLE_REASON_FLAG_PSU_ALERT)
        return std::string("frequency throttled due to power supply assertion.");
    if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE) == ZES_FREQ_THROTTLE_REASON_FLAG_SW_RANGE)
        return std::string("frequency throttled due to software supplied frequency range.");
    if ((flags & ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE) == ZES_FREQ_THROTTLE_REASON_FLAG_HW_RANGE)
        return std::string("frequency throttled due to a sub block that has a lower frequency.");
    
    return std::string("frequency throttled reason cannot be determined.");
}

std::string GPUDeviceStub::to_string(ze_device_uuid_t val) {
    std::ostringstream os;
    std::reverse_iterator<uint8_t*> begin(val.id + sizeof(val.id) / sizeof(val.id[0]));
    std::reverse_iterator<uint8_t*> end(val.id);
    auto& iter = begin;
    while (iter != end) {
        os << std::setfill('0') << std::setw(2) << std::hex << (int)*iter << std::dec;
        iter++;
    }
    return os.str();
}

std::string GPUDeviceStub::to_string(zes_pci_address_t address) {
    std::ostringstream os;
    os << std::setfill('0') << std::setw(4) << std::hex
       << address.domain << std::string(":")
       << std::setw(2)
       << address.bus << std::string(":")
       << std::setw(2)
       << address.device << std::string(".")
       << address.function;
    return os.str();
}

std::string GPUDeviceStub::to_regex_string(zes_pci_address_t address) {
    std::ostringstream os;
    os << std::setfill('0') << std::setw(4) << std::hex
       << address.domain << std::string(":")
       << std::setw(2)
       << address.bus << std::string(":")
       << std::setw(2)
       << address.device << std::string("\\.")
       << address.function;
    return os.str();
}

std::string GPUDeviceStub::to_hex_string(uint32_t val) {
    std::stringstream s;
    s << std::string("0x") << std::hex << val << std::dec;
    return s.str();
}

void GPUDeviceStub::getPower(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetPower, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPower(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetPower error");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t power_domain_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& power : power_handles) {
            zes_power_properties_t props = {};
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
            if (res == ZE_RESULT_SUCCESS) {
                zes_power_energy_counter_t snap;
                XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap));
                if (res == ZE_RESULT_SUCCESS) {
                    props.onSubdevice ? ret->setSubdeviceRawData(props.subdeviceId, Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * snap.energy) : ret->setRawData(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * snap.energy);
                    props.onSubdevice ? ret->setSubdeviceDataRawTimestamp(props.subdeviceId, snap.timestamp) : ret->setRawTimestamp(snap.timestamp);
                    ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                    data_acquired = true;
                } else {
                    exception_msgs["zesPowerGetEnergyCounter"] = res;
                }
            } else {
                exception_msgs["zesPowerGetProperties"] = res;
            }
        }
    } else {
        exception_msgs["zesDeviceEnumPowerDomains"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getEnergy(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetEnergy, device);
}

std::string GPUDeviceStub::buildErrors(const std::map<std::string, ze_result_t>& exception_msgs, const char* func, uint32_t line) {
    if (!exception_msgs.empty()) {
        std::string content;
        bool first = true;
        for (auto info : exception_msgs) {
            if (first) {
                std::string func_name = func;
                content.append("[" + func_name + ":" + std::to_string(line) + "] " + info.first + ":" + to_hex_string(info.second));
                first = false;
            } else {
                content.append(", " + info.first + ":" + to_hex_string(info.second));
            }
        }
        return content;
    }
    return "";
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEnergy(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetEnergy");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t power_domain_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& power : power_handles) {
            zes_power_properties_t props = {};
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
            if (res == ZE_RESULT_SUCCESS) {
                zes_power_energy_counter_t counter;
                XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &counter));
                if (res == ZE_RESULT_SUCCESS) {
                    props.onSubdevice ? ret->setSubdeviceDataCurrent(props.subdeviceId, counter.energy * 1.0 / 1000) : ret->setCurrent(counter.energy * 1.0 / 1000);
                    data_acquired = true;
                } else {
                    exception_msgs["zesPowerGetEnergyCounter"] = res;
                }
            } else {
                exception_msgs["zesPowerGetProperties"] = res;
            }
        }
    } else {
        exception_msgs["zesDeviceEnumPowerDomains"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getActuralRequestFrequency(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetActuralRequestFrequency, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetActuralRequestFrequency(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetActuralRequestFrequency error");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t freq_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr));
    std::vector<zes_freq_handle_t> freq_handles(freq_count);
    if (res == ZE_RESULT_SUCCESS) {
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data()));
        for (auto& ph_freq : freq_handles) {
            zes_freq_properties_t props;
            XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetProperties(ph_freq, &props));
            if (res == ZE_RESULT_SUCCESS) {
                zes_freq_state_t freq_state;
                XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetState(ph_freq, &freq_state));
                if (res == ZE_RESULT_SUCCESS && freq_state.actual >= 0) {
                    uint32_t subdeviceId = UINT32_MAX;
                    if (props.onSubdevice) {
                        subdeviceId = props.subdeviceId;
                        ret->setSubdeviceDataCurrent(props.subdeviceId, freq_state.actual);
                    } else {
                        ret->setCurrent(freq_state.actual);
                    }
                    ret->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_REQUEST_FREQUENCY, freq_state.request);
                    data_acquired = true;
                } else {
                    exception_msgs["zesFrequencyGetState"] = res;
                }
            } else {
                exception_msgs["zesFrequencyGetProperties"] = res;
            }
        }
    } else {
        exception_msgs["zesDeviceEnumFrequencyDomains"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getFrequencyThrottle(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetFrequencyThrottle, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetFrequencyThrottle(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetFrequencyThrottle error");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t freq_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr));
    std::vector<zes_freq_handle_t> freq_handles(freq_count);
    if (res == ZE_RESULT_SUCCESS) {
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data()));
        for (auto& ph_freq : freq_handles) {
            zes_freq_properties_t props;
            XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetProperties(ph_freq, &props));
            if (res == ZE_RESULT_SUCCESS) {
                zes_freq_throttle_time_t freq_throttle;
                XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetThrottleTime(ph_freq, &freq_throttle));
                if (res == ZE_RESULT_SUCCESS) {
                    props.onSubdevice ? ret->setSubdeviceRawData(props.subdeviceId, Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * freq_throttle.throttleTime) : ret->setRawData(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * freq_throttle.throttleTime);
                    props.onSubdevice ? ret->setSubdeviceDataRawTimestamp(props.subdeviceId, freq_throttle.timestamp) : ret->setRawTimestamp(freq_throttle.timestamp);
                    ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                    data_acquired = true;
                } else {
                    exception_msgs["zesFrequencyGetThrottleTime"] = res;
                }
            } else {
                exception_msgs["zesFrequencyGetProperties"] = res;
            }
        }
    } else {
        exception_msgs["zesDeviceEnumFrequencyDomains"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getTemperature(const zes_device_handle_t& device, Callback_t callback, zes_temp_sensors_t type) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetTemperature, device, type);
}

int GPUDeviceStub::get_register_value_from_sys(const zes_device_handle_t& device, uint64_t offset) {
    int val = -1;
    ze_result_t res;
    zes_pci_properties_t pci_props;
    pci_props.stype = ZES_STRUCTURE_TYPE_PCI_PROPERTIES;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    if (res == ZE_RESULT_SUCCESS) {
        std::string bdf_address;
        bdf_address = to_string(pci_props.address);
        std::string resource_file = "/sys/bus/pci/devices/" + bdf_address + "/resource0";
        int fd;
        void *map_base, *virt_addr;
        uint64_t read_result;
        char *filename;
        off_t target, target_base;
        int type_width = 4;
        int map_size = 4096UL;

        filename = const_cast<char*>(resource_file.c_str());
        target = offset;
        if ((fd = open(filename, O_RDONLY | O_SYNC)) == -1) {
            return -1;
        }
        target_base = target & ~(sysconf(_SC_PAGE_SIZE)-1);
        if (target + type_width - target_base > map_size)
            map_size = target + type_width - target_base;
        
        map_base = mmap(0, map_size, PROT_READ, MAP_SHARED, fd, target_base);

        if (map_base == (void *) -1) {
            close(fd);
            return -1;
        }
        virt_addr = (uint8_t *)map_base + target - target_base;
        read_result = *((uint32_t *) virt_addr);
        val = read_result;

        munmap(map_base, map_size);
        close(fd);
    }
    return val;
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetTemperature(const zes_device_handle_t& device, zes_temp_sensors_t type) {
    if (device == nullptr) {
        throw BaseException("toGetTemperature error");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t temp_sensor_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, nullptr));
    if (temp_sensor_count == 0) {
        zes_device_properties_t props;
        props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceGetProperties(device, &props));
        if (type == ZES_TEMP_SENSORS_GPU && res == ZE_RESULT_SUCCESS 
            && (to_hex_string(props.core.deviceId).find("56c0") != std::string::npos 
                || to_hex_string(props.core.deviceId).find("56c1") != std::string::npos)) {
            int val = get_register_value_from_sys(device, 0x145978);
            if (val > 0) {
                ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                ret->setCurrent(val * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                return ret;
            } else {
                throw BaseException("Failed to read register value from sys");        
            }
        }
        throw BaseException("No temperature sensor detected");
    }
    std::vector<zes_temp_handle_t> temp_sensors(temp_sensor_count);
    if (res == ZE_RESULT_SUCCESS) {
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, temp_sensors.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& temp : temp_sensors) {
                zes_temp_properties_t props;
                XPUM_ZE_HANDLE_LOCK(temp, res = zesTemperatureGetProperties(temp, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    switch (props.type) {
                        case ZES_TEMP_SENSORS_GPU:
                            if (type == props.type) {
                                double temp_val = 0;
                                XPUM_ZE_HANDLE_LOCK(temp, res = zesTemperatureGetState(temp, &temp_val));
                                // filter abnormal temperatures
                                if (res == ZE_RESULT_SUCCESS && temp_val < 150) {
                                    ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                                    if (props.onSubdevice) {
                                        ret->setSubdeviceDataCurrent(props.subdeviceId, temp_val * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                                    } else {
                                        ret->setCurrent(temp_val * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                                    }
                                    data_acquired = true;
                                } else {
                                    exception_msgs["zesTemperatureGetState"] = res;
                                }
                            }
                            break;
                        case ZES_TEMP_SENSORS_MEMORY:
                            if (type == props.type) {
                                double temp_val = 0;
                                XPUM_ZE_HANDLE_LOCK(temp, res = zesTemperatureGetState(temp, &temp_val));
                                // filter abnormal temperatures
                                if (res == ZE_RESULT_SUCCESS && temp_val < 150) {
                                    ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                                    if (props.onSubdevice) {
                                        ret->setSubdeviceDataCurrent(props.subdeviceId, temp_val * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                                    } else {
                                        ret->setCurrent(temp_val * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                                    }
                                    data_acquired = true;
                                } else {
                                    exception_msgs["zesTemperatureGetState"] = res;
                                }
                            }
                            break;
                        default:
                            break;
                    }
                } else {
                    exception_msgs["zesTemperatureGetProperties"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumTemperatureSensors"] = res;
        }
    } else {
        exception_msgs["zesDeviceEnumTemperatureSensors"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getMemoryUsedUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetMemoryUsedUtilization, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemoryUsedUtilization(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetMemoryUsedUtilization error");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    uint32_t mem_module_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_mem_handle_t> mems(mem_module_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& mem : mems) {
                zes_mem_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetProperties(mem, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    zes_mem_state_t sysman_memory_state = {};
                    sysman_memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
                    XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetState(mem, &sysman_memory_state));
                    if (res == ZE_RESULT_SUCCESS && sysman_memory_state.size != 0) {
                        uint64_t used = props.physicalSize == 0 ? sysman_memory_state.size - sysman_memory_state.free : props.physicalSize - sysman_memory_state.free;
                        uint64_t utilization = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * used * 100 / (props.physicalSize == 0 ? sysman_memory_state.size : props.physicalSize);
                        uint32_t subdeviceId = UINT32_MAX;
                        if (props.onSubdevice) {
                            subdeviceId = props.subdeviceId;
                            ret->setSubdeviceDataCurrent(props.subdeviceId, used);
                        } else {
                            ret->setCurrent(used);
                        }
                        ret->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_MEMORY_UTILIZATION, utilization, Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                        data_acquired = true;
                    } else {
                        exception_msgs["zesMemoryGetState"] = res;
                    }
                } else {
                    exception_msgs["zesMemoryGetProperties"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumMemoryModules"] = res;
        }
    } else {
        exception_msgs["zesDeviceEnumMemoryModules"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getMemoryBandwidth(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetMemoryBandwidth, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemoryBandwidth(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetMemoryBandwidth error");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t mem_module_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_mem_handle_t> mems(mem_module_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& mem : mems) {
                zes_mem_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetProperties(mem, &props));
                if (res != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE) {
                    continue;
                }

                zes_mem_bandwidth_t s1, s2;
                XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetBandwidth(mem, &s1));
                if (res == ZE_RESULT_SUCCESS) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::MEMORY_BANDWIDTH_MONITOR_INTERNAL_PERIOD));
                    XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetBandwidth(mem, &s2));
                    if (res == ZE_RESULT_SUCCESS && (s2.maxBandwidth * (s2.timestamp - s1.timestamp)) != 0) {
                        uint64_t val = 1000000 * ((s2.readCounter - s1.readCounter) + (s2.writeCounter - s1.writeCounter)) / (s2.maxBandwidth * (s2.timestamp - s1.timestamp));
                        if (val > 100) {
                            val = 100;
                        }   
                        if (props.onSubdevice) {
                            ret->setSubdeviceDataCurrent(props.subdeviceId, val);
                        } else {
                            ret->setCurrent(val);
                        }
                        data_acquired = true;
                    } else {
                        XPUM_LOG_DEBUG("zesMemoryGetBandwidth return s1 timestamp: {}, s2 timestamp: {}, s2.maxBandwidth: {}", s1.timestamp, s2.timestamp, s2.maxBandwidth);
                        exception_msgs["zesMemoryGetBandwidth-2"] = res;
                    }
                } else {
                    exception_msgs["zesMemoryGetBandwidth-1"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumMemoryModules"] = res;
        }
    } else {
        exception_msgs["zesDeviceEnumMemoryModules"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getMemoryReadWrite(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetMemoryReadWrite, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemoryReadWrite(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetMemoryReadWrite error");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t mem_module_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_mem_handle_t> mems(mem_module_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& mem : mems) {
                zes_mem_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetProperties(mem, &props));
                if (res != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE) {
                    continue;
                }

                zes_mem_bandwidth_t mem_bandwidth;
                XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetBandwidth(mem, &mem_bandwidth));
                if (res == ZE_RESULT_SUCCESS) {
                    uint32_t subdeviceId = UINT32_MAX;
                    if (props.onSubdevice) {
                        subdeviceId = props.subdeviceId;
                        ret->setSubdeviceDataCurrent(props.subdeviceId, mem_bandwidth.readCounter);
                    } else {
                        ret->setCurrent(mem_bandwidth.readCounter);
                    }
                    ret->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_MEMORY_WRITE, mem_bandwidth.writeCounter);
                    ret->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_MEMORY_READ_THROUGHPUT, mem_bandwidth.readCounter / 1024 * 1000, 1, true, Utility::getCurrentMillisecond());
                    ret->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_MEMORY_WRITE_THROUGHPUT, mem_bandwidth.writeCounter / 1024 * 1000, 1, true, Utility::getCurrentMillisecond());
                    data_acquired = true;
                } else {
                    exception_msgs["zesMemoryGetBandwidth"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumMemoryModules"] = res;
        }
    } else {
        exception_msgs["zesDeviceEnumMemoryModules"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getEuActiveStallIdle(const ze_device_handle_t& device, const ze_driver_handle_t& driver, MeasurementType type, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetEuActiveStallIdle, device, driver, type);
}

std::mutex GPUDeviceStub::metric_streamer_mutex;
std::map<ze_device_handle_t, zet_metric_group_handle_t> GPUDeviceStub::target_metric_groups;
std::map<ze_device_handle_t, ze_context_handle_t> GPUDeviceStub::target_metric_contexts;
void GPUDeviceStub::toGetEuActiveStallIdleCore(const ze_device_handle_t& device, uint32_t subdeviceId, const ze_driver_handle_t& driver, MeasurementType type, std::shared_ptr<MeasurementData>& data) {
    ze_result_t res;
    std::unique_lock<std::mutex> lock(GPUDeviceStub::metric_streamer_mutex);
    zet_metric_group_handle_t hMetricGroup = nullptr;
    if (GPUDeviceStub::target_metric_groups.find(device) != GPUDeviceStub::target_metric_groups.end()) {
        hMetricGroup = GPUDeviceStub::target_metric_groups.at(device);
    } else {
        uint32_t metricGroupCount = 0;
        XPUM_ZE_HANDLE_LOCK(device, res = zetMetricGroupGet(device, &metricGroupCount, nullptr));
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zet_metric_group_handle_t> metricGroups(metricGroupCount);
            XPUM_ZE_HANDLE_LOCK(device, res = zetMetricGroupGet(device, &metricGroupCount, metricGroups.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& metric_group : metricGroups) {
                    zet_metric_group_properties_t metric_group_properties;
                    metric_group_properties.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
                    res = zetMetricGroupGetProperties(metric_group, &metric_group_properties);
                    if (res == ZE_RESULT_SUCCESS) {
                        if (std::strcmp(metric_group_properties.name, "ComputeBasic") == 0 && metric_group_properties.samplingType == ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) {
                            GPUDeviceStub::target_metric_groups[device] = metric_group;
                            hMetricGroup = metric_group;
                            break;
                        }
                    }
                }
            }
        }
    }

    if (hMetricGroup == nullptr) {
        throw BaseException("toGetEuActiveStallIdleCore");
    }
    ze_context_handle_t hContext = nullptr;
    if (GPUDeviceStub::target_metric_contexts.find(device) != GPUDeviceStub::target_metric_contexts.end()) {
        hContext = GPUDeviceStub::target_metric_contexts.at(device);
    } else {
        ze_context_desc_t context_desc = {
                ZE_STRUCTURE_TYPE_CONTEXT_DESC,
                nullptr, 
                0
        };
        XPUM_ZE_HANDLE_LOCK(driver, res = zeContextCreate(driver, &context_desc, &hContext));
        if (res != ZE_RESULT_SUCCESS) {
            throw BaseException("toGetEuActiveStallIdleCore - zeContextCreate");
        }
        GPUDeviceStub::target_metric_contexts[device] = hContext;
    }

    zet_metric_streamer_handle_t hMetricStreamer = nullptr;
    zet_metric_streamer_desc_t metricStreamerDesc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC};
    XPUM_ZE_HANDLE_LOCK(device, res = zetContextActivateMetricGroups(hContext, device, 1, &hMetricGroup));
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdleCore - zetContextActivateMetricGroups");
    }

    metricStreamerDesc.samplingPeriod = Configuration::EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD;
    XPUM_ZE_HANDLE_LOCK(device, res = zetMetricStreamerOpen(hContext, device, hMetricGroup, &metricStreamerDesc, nullptr, &hMetricStreamer));
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdleCore - zetMetricStreamerOpen");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::EU_ACTIVE_STALL_IDLE_MONITOR_INTERNAL_PERIOD));
    size_t rawSize = 0;
    res = zetMetricStreamerReadData(hMetricStreamer, UINT32_MAX, &rawSize, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdleCore");
    }
    std::vector<uint8_t> rawData(rawSize);
    res = zetMetricStreamerReadData(hMetricStreamer, UINT32_MAX, &rawSize, rawData.data());
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdleCore");
    }

    res = zetMetricStreamerClose(hMetricStreamer);
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("zetMetricStreamerClose");
    }
    res = zetContextActivateMetricGroups(hContext, device, 0, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("zetContextActivateMetricGroups");
    }
    uint32_t numMetricValues = 0;
    zet_metric_group_calculation_type_t calculationType = ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES;
    res = zetMetricGroupCalculateMetricValues(hMetricGroup, calculationType, rawSize, rawData.data(), &numMetricValues, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdleCore");
    }
    std::vector<zet_typed_value_t> metricValues(numMetricValues);
    res = zetMetricGroupCalculateMetricValues(hMetricGroup, calculationType, rawSize, rawData.data(), &numMetricValues, metricValues.data());
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdleCore");
    }
    uint32_t metricCount = 0;
    res = zetMetricGet(hMetricGroup, &metricCount, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdleCore");
    }
    std::vector<zet_metric_handle_t> phMetrics(metricCount);
    res = zetMetricGet(hMetricGroup, &metricCount, phMetrics.data());
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdleCore");
    }

    uint32_t numReports = numMetricValues / metricCount;
    std::vector<uint64_t> currents(4);
    uint64_t totalGpuBusy = 0;
    uint64_t totalEuStall = 0;
    uint64_t totalEuActive = 0;
    uint64_t totalGPUElapsedTime = 0;
    for (uint32_t report = 0; report < numReports; ++report) {
        uint64_t currentGpuBusy = 0;
        uint64_t currentEuStall = 0;
        uint64_t currentEuActive = 0;
        uint64_t currentXveStall = 0;
        uint64_t currentXueActive = 0;
        uint64_t currentGPUElapsedTime = 0;
        for (uint32_t metric = 0; metric < metricCount; metric++) {
            zet_typed_value_t data = metricValues[report * metricCount + metric];
            zet_metric_properties_t metricProperties;
            res = zetMetricGetProperties(phMetrics[metric], &metricProperties);
            if (res != ZE_RESULT_SUCCESS) {
                throw BaseException("toGetEuActiveStallIdleCore");
            }
            if (std::strcmp(metricProperties.name, "GpuBusy") == 0) {
                currentGpuBusy = data.value.fp32;
            }
            if (std::strcmp(metricProperties.name, "EuActive") == 0) {
                currentEuActive = data.value.fp32;
            }
            if (std::strcmp(metricProperties.name, "EuStall") == 0) {
                currentEuStall = data.value.fp32;
            }
            if (strcmp(metricProperties.name, "XveActive") == 0) {
                currentXueActive = data.value.fp32;
            }
            if (strcmp(metricProperties.name, "XveStall") == 0) {
                currentXveStall = data.value.fp32;
            }
            if (std::strcmp(metricProperties.name, "GpuTime") == 0) {
                currentGPUElapsedTime = data.value.ui64;
            }
        }
        currentEuActive = std::max(currentEuActive, currentXueActive);
        currentEuStall = std::max(currentEuStall, currentXveStall);
        if (currentEuActive > 100 || currentEuStall > 100) {
            throw BaseException("toGetEuActiveStallIdleCore - abnormal EU data");
        }        
        totalGpuBusy += currentGPUElapsedTime * currentGpuBusy;
        totalEuStall += currentGPUElapsedTime * currentEuStall;
        totalEuActive += currentGPUElapsedTime * currentEuActive;
        totalGPUElapsedTime += currentGPUElapsedTime;
    }
    uint64_t euActive = totalEuActive / totalGPUElapsedTime;
    uint64_t euStall = totalEuStall / totalGPUElapsedTime;
    uint64_t euIdle = 100 - euActive - euStall;
    int scale = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
    euActive *= scale;
    euStall *= scale;
    euIdle *= scale;
    data->setScale(scale);
    if (type == MeasurementType::METRIC_EU_ACTIVE) {
        if (subdeviceId == UINT32_MAX)
            data->setCurrent(euActive);
        else
            data->setSubdeviceDataCurrent(subdeviceId, euActive);
        data->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_EU_STALL, euStall, scale);
        data->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_EU_IDLE, euIdle, scale);
    } else if (type == MeasurementType::METRIC_EU_STALL) {
        data->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_EU_ACTIVE, euActive, scale);
        if (subdeviceId == UINT32_MAX)
            data->setCurrent(euStall);
        else
            data->setSubdeviceDataCurrent(subdeviceId, euStall);
        data->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_EU_IDLE, euIdle, scale);
    } else if (type == MeasurementType::METRIC_EU_IDLE) {
        data->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_EU_ACTIVE, euActive, scale);
        data->setSubdeviceAdditionalData(subdeviceId, MeasurementType::METRIC_EU_STALL, euStall, scale);
        if (subdeviceId == UINT32_MAX)
            data->setCurrent(euIdle);
        else
            data->setSubdeviceDataCurrent(subdeviceId, euIdle);
    }
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEuActiveStallIdle(const ze_device_handle_t& device, const ze_driver_handle_t& driver, MeasurementType type) {
    if (device == nullptr) {
        throw BaseException("toGetEuActiveStallIdle");
    }
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    uint32_t sub_device_count = 0;
    XPUM_ZE_HANDLE_LOCK(device, res = zeDeviceGetSubDevices(device, &sub_device_count, nullptr));
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdle");
    }
    std::vector<ze_device_handle_t> sub_device_handles(sub_device_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zeDeviceGetSubDevices(device, &sub_device_count, sub_device_handles.data()));
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetEuActiveStallIdle");
    }
    if (sub_device_count == 0) {
        toGetEuActiveStallIdleCore(device, UINT32_MAX, driver, type, ret);
        return ret;
    }
    for (auto& sub_device : sub_device_handles) {
        ze_device_properties_t props = {};
        props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        XPUM_ZE_HANDLE_LOCK(device, res = zeDeviceGetProperties(sub_device, &props));
        if (res != ZE_RESULT_SUCCESS) {
            throw BaseException("toGetEuActiveStallIdle");
        }
        toGetEuActiveStallIdleCore(sub_device, props.subdeviceId, driver, type, ret);
    }
    return ret;
}

void GPUDeviceStub::getRasError(const zes_device_handle_t& device, Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept {
    if (device == nullptr) {
        return;
    }
    //invokeTask(callback, toGetRasError, device,ZES_RAS_ERROR_CAT_RESET,ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    invokeTask(callback, toGetRasError, device, rasCat, rasType);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetRasError(const zes_device_handle_t& device, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) {
    //rasCat: ZES_RAS_ERROR_CAT_RESET; rasType: ZES_RAS_ERROR_TYPE_CORRECTABLE,ZES_RAS_ERROR_TYPE_UNCORRECTABLE
    if (device == nullptr) {
        throw BaseException("toGetRasError error");
    }
    uint32_t numRasErrorSets = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumRasErrorSets(device, &numRasErrorSets, nullptr));
    if (res == ZE_RESULT_SUCCESS && numRasErrorSets > 0) {
        std::vector<zes_ras_handle_t> phRasErrorSets(numRasErrorSets);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumRasErrorSets(device, &numRasErrorSets, phRasErrorSets.data()));
        if (res == ZE_RESULT_SUCCESS) {
            uint64_t rasCounter = 0;
            for (auto& rasHandle : phRasErrorSets) {
                // globally lock for RAS APIs to avoid two issues: 1) invalid read/write memory in zesRasGetState; 2) kernel error msg "mei-gsc mei-gscfi.3.auto: id exceeded 256"
                std::lock_guard<std::mutex> lock(ras_m);
                zes_ras_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_RAS_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetProperties(rasHandle, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    //if (props.supported && props.enabled) {
                    if (props.type == rasType) {
                        zes_ras_state_t errorDetails;
                        XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetState(rasHandle, 0, &errorDetails));
                        if (res == ZE_RESULT_SUCCESS) {
                            rasCounter += errorDetails.category[rasCat];
                        }
                    }
                    //}
                }
            }
            return std::make_shared<MeasurementData>(rasCounter);
        }
    }
    throw BaseException("toGetRasError error");
}

void GPUDeviceStub::getRasErrorOnSubdevice(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    //invokeTask(callback, toGetRasErrorOnSubdevice, device,ZES_RAS_ERROR_CAT_RESET,ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    invokeTask(callback, toGetRasErrorOnSubdevice, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetRasErrorOnSubdevice(const zes_device_handle_t& device) {
    //rasCat: ZES_RAS_ERROR_CAT_RESET; rasType: ZES_RAS_ERROR_TYPE_CORRECTABLE,ZES_RAS_ERROR_TYPE_UNCORRECTABLE
    if (device == nullptr) {
        throw BaseException("toGetRasErrorOnSubdevice error");
    }

    ////
    bool dataAcquired = false;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    uint32_t subdeviceId = UINT32_MAX;
    uint64_t rasCounter = 0;
    uint32_t numRasErrorSets = 0;
    ze_result_t res;
    zes_ras_state_t errorDetails;

    ////
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumRasErrorSets(device, &numRasErrorSets, nullptr));
    if (res == ZE_RESULT_SUCCESS && numRasErrorSets > 0) {
        std::vector<zes_ras_handle_t> phRasErrorSets(numRasErrorSets);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumRasErrorSets(device, &numRasErrorSets, phRasErrorSets.data()));
        if (res == ZE_RESULT_SUCCESS) {
            //uint64_t rasCounter = 0;
            for (auto& rasHandle : phRasErrorSets) {
                // globally lock for RAS APIs to avoid two issues: 1) invalid read/write memory in zesRasGetState; 2) kernel error msg "mei-gsc mei-gscfi.3.auto: id exceeded 256"
                std::lock_guard<std::mutex> lock(ras_m);
                zes_ras_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_RAS_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetProperties(rasHandle, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    //if (props.supported && props.enabled) {
                    if (props.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
                        XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetState(rasHandle, 0, &errorDetails));
                        if (res == ZE_RESULT_SUCCESS) {
                            subdeviceId = props.onSubdevice ? props.subdeviceId : UINT32_MAX;
                            //
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_RESET];
                            props.onSubdevice ? ret->setSubdeviceDataCurrent(subdeviceId, rasCounter) : ret->setCurrent(rasCounter);
                            //
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS];
                            ret->setSubdeviceAdditionalData(subdeviceId, METRIC_RAS_ERROR_CAT_PROGRAMMING_ERRORS, rasCounter);
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS];
                            ret->setSubdeviceAdditionalData(subdeviceId, METRIC_RAS_ERROR_CAT_DRIVER_ERRORS, rasCounter);
                            //
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS];
                            ret->setSubdeviceAdditionalData(subdeviceId, METRIC_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, rasCounter);
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS];
                            ret->setSubdeviceAdditionalData(subdeviceId, METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE, rasCounter);
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS];
                            ret->setSubdeviceAdditionalData(subdeviceId, METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, rasCounter);
                            dataAcquired = true;
                        }
                    } else if (props.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
                        XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetState(rasHandle, 0, &errorDetails));
                        if (res == ZE_RESULT_SUCCESS) {
                            subdeviceId = props.onSubdevice ? props.subdeviceId : UINT32_MAX;
                            //
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS];
                            ret->setSubdeviceAdditionalData(subdeviceId, METRIC_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, rasCounter);
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS];
                            ret->setSubdeviceAdditionalData(subdeviceId, METRIC_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE, rasCounter);
                            rasCounter = errorDetails.category[ZES_RAS_ERROR_CAT_NON_COMPUTE_ERRORS];
                            ret->setSubdeviceAdditionalData(subdeviceId, METRIC_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, rasCounter);
                            dataAcquired = true;
                        }
                    }
                    //}
                }
            }
            //return std::make_shared<MeasurementData>(rasCounter);
        }
    }
    if (res == ZE_RESULT_SUCCESS && dataAcquired) {
        return ret;
    } else {
        throw BaseException("toGetRasErrorOnSubdevice error");
    }
}

void GPUDeviceStub::getRasError(const zes_device_handle_t& device, uint64_t errorCategory[XPUM_RAS_ERROR_MAX]) noexcept {
    //uint64_t errorCategory[XPUM_RAS_ERROR_MAX];
    for (int i = 0; i < XPUM_RAS_ERROR_MAX; i++) {
        errorCategory[i] = 0;
    }

    if (device == nullptr) {
        return;
    }

    //
    uint32_t numRasErrorSets = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumRasErrorSets(device, &numRasErrorSets, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_ras_handle_t> phRasErrorSets(numRasErrorSets);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumRasErrorSets(device, &numRasErrorSets, phRasErrorSets.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& rasHandle : phRasErrorSets) {
                // globally lock for RAS APIs to avoid two issues: 1) invalid read/write memory in zesRasGetState; 2) kernel error msg "mei-gsc mei-gscfi.3.auto: id exceeded 256"
                std::lock_guard<std::mutex> lock(ras_m);
                zes_ras_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_RAS_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetProperties(rasHandle, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    if (props.type == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
                        zes_ras_state_t errorDetails;
                        XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetState(rasHandle, 0, &errorDetails));
                        if (res == ZE_RESULT_SUCCESS) {
                            errorCategory[XPUM_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE] += errorDetails.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS];
                            errorCategory[XPUM_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE] += errorDetails.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS];
                        }
                    } else if (props.type == ZES_RAS_ERROR_TYPE_UNCORRECTABLE) {
                        zes_ras_state_t errorDetails;
                        XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetState(rasHandle, 0, &errorDetails));
                        if (res == ZE_RESULT_SUCCESS) {
                            errorCategory[XPUM_RAS_ERROR_CAT_RESET] += errorDetails.category[ZES_RAS_ERROR_CAT_RESET];
                            errorCategory[XPUM_RAS_ERROR_CAT_PROGRAMMING_ERRORS] += errorDetails.category[ZES_RAS_ERROR_CAT_PROGRAMMING_ERRORS];
                            errorCategory[XPUM_RAS_ERROR_CAT_DRIVER_ERRORS] += errorDetails.category[ZES_RAS_ERROR_CAT_DRIVER_ERRORS];
                            //
                            errorCategory[XPUM_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE] += errorDetails.category[ZES_RAS_ERROR_CAT_CACHE_ERRORS];
                            errorCategory[XPUM_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE] += errorDetails.category[ZES_RAS_ERROR_CAT_DISPLAY_ERRORS];
                        }
                    }
                }
            }
            return;
        }
    }
    //throw BaseException("getRasError error");
}

void GPUDeviceStub::getRasErrorOnSubdevice(const zes_device_handle_t& device, Callback_t callback, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) noexcept {
    if (device == nullptr) {
        return;
    }
    //invokeTask(callback, toGetRasErrorOnSubdevice, device,ZES_RAS_ERROR_CAT_RESET,ZES_RAS_ERROR_TYPE_UNCORRECTABLE);
    invokeTask(callback, toGetRasErrorOnSubdeviceOld, device, rasCat, rasType);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetRasErrorOnSubdeviceOld(const zes_device_handle_t& device, const zes_ras_error_cat_t& rasCat, const zes_ras_error_type_t& rasType) {
    //rasCat: ZES_RAS_ERROR_CAT_RESET; rasType: ZES_RAS_ERROR_TYPE_CORRECTABLE,ZES_RAS_ERROR_TYPE_UNCORRECTABLE
    if (device == nullptr) {
        throw BaseException("toGetRasErrorOnSubdevice error");
    }
    bool dataAcquired = false;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();

    ////
    uint32_t numRasErrorSets = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumRasErrorSets(device, &numRasErrorSets, nullptr));
    if (res == ZE_RESULT_SUCCESS && numRasErrorSets > 0) {
        std::vector<zes_ras_handle_t> phRasErrorSets(numRasErrorSets);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumRasErrorSets(device, &numRasErrorSets, phRasErrorSets.data()));
        if (res == ZE_RESULT_SUCCESS) {
            //uint64_t rasCounter = 0;
            for (auto& rasHandle : phRasErrorSets) {
                // globally lock for RAS APIs to avoid two issues: 1) invalid read/write memory in zesRasGetState; 2) kernel error msg "mei-gsc mei-gscfi.3.auto: id exceeded 256"
                std::lock_guard<std::mutex> lock(ras_m);
                zes_ras_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_RAS_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetProperties(rasHandle, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    //if (props.supported && props.enabled) {
                    if (props.type == rasType) {
                        zes_ras_state_t errorDetails;
                        XPUM_ZE_HANDLE_LOCK(rasHandle, res = zesRasGetState(rasHandle, 0, &errorDetails));
                        if (res == ZE_RESULT_SUCCESS) {
                            uint64_t rasCounter = errorDetails.category[rasCat];
                            props.onSubdevice ? ret->setSubdeviceDataCurrent(props.subdeviceId, rasCounter) : ret->setCurrent(rasCounter);
                            dataAcquired = true;
                        }
                    }
                    //}
                }
            }
            //return std::make_shared<MeasurementData>(rasCounter);
        }
    }
    if (res == ZE_RESULT_SUCCESS && dataAcquired) {
        return ret;
    } else {
        throw BaseException("toGetRasErrorOnSubdevice error");
    }
}

void GPUDeviceStub::getGPUUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetGPUUtilization, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetGPUUtilization(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetGPUUtilization error");
    }

    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t engine_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    zes_device_properties_t props = {};
    props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceGetProperties(device, &props));
    if (res == ZE_RESULT_SUCCESS) {
        ret->setNumSubdevices(props.numSubdevices);
    } else {
        exception_msgs["zesDeviceGetProperties"] = res;
    }

    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_engine_handle_t> engines(engine_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_count, engines.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& engine : engines) {
                zes_engine_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    if (props.type == ZES_ENGINE_GROUP_ALL) {
                        zes_engine_stats_t snap = {};
                        XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetActivity(engine, &snap));
                        if (res == ZE_RESULT_SUCCESS) {
                            ExtendedMeasurementData data;
                            data.on_subdevice = props.onSubdevice;
                            data.subdevice_id = props.subdeviceId;
                            data.type = props.type;
                            data.active_time = snap.activeTime;
                            data.timestamp = snap.timestamp;
                            ret->addExtendedData(uint64_t(engine), data);
                            data_acquired = true;
                        } else {
                            exception_msgs["zesEngineGetActivity"] = res;
                        }
                    }
                } else {
                    exception_msgs["zesEngineGetProperties"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumEngineGroups"] = res;
        }
    } else {
        exception_msgs["zesDeviceEnumEngineGroups"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getEngineUtilization(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetEngineUtilization, device);
}

std::shared_ptr<EngineCollectionMeasurementData> GPUDeviceStub::toGetEngineUtilization(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetEngineUtilization error");
    }

    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t engine_count = 0;
    std::shared_ptr<EngineCollectionMeasurementData> ret = std::make_shared<EngineCollectionMeasurementData>();
    ze_result_t res;
    zes_device_properties_t props = {};
    props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceGetProperties(device, &props));
    if (res == ZE_RESULT_SUCCESS) {
        ret->setNumSubdevices(props.numSubdevices);
    } else {
        exception_msgs["zesDeviceGetProperties"] = res;
    }

    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_engine_handle_t> engines(engine_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_count, engines.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& engine : engines) {
                zes_engine_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    zes_engine_stats_t snap = {};
                    XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetActivity(engine, &snap));
                    if (res == ZE_RESULT_SUCCESS) {
                        ret->addRawData(uint64_t(engine), props.type, (bool)props.onSubdevice, props.subdeviceId, snap.activeTime, snap.timestamp);
                        data_acquired = true;
                    } else {
                        exception_msgs["zesEngineGetActivity"] = res;
                    }
                } else {
                    exception_msgs["zesEngineGetProperties"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumEngineGroups"] = res;
        }
    } else {
        exception_msgs["zesDeviceEnumEngineGroups"] = res;
    }
    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getEngineGroupUtilization(const zes_device_handle_t& device, Callback_t callback, zes_engine_group_t engine_group_type) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetEngineGroupUtilization, device, engine_group_type);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEngineGroupUtilization(const zes_device_handle_t& device, zes_engine_group_t engine_group_type) {
    if (device == nullptr) {
        throw BaseException("toGetEngineGroupUtilization error");
    }

    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t engine_count = 0;
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    ze_result_t res;
    zes_device_properties_t props = {};
    props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceGetProperties(device, &props));
    if (res == ZE_RESULT_SUCCESS) {
        ret->setNumSubdevices(props.numSubdevices);
    } else {
        exception_msgs["zesDeviceGetProperties"] = res;
    }
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_engine_handle_t> engines(engine_count);
        std::map<uint32_t, std::vector<uint32_t>> group_utilizations;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_count, engines.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& engine : engines) {
                zes_engine_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    switch (engine_group_type) {
                        case ZES_ENGINE_GROUP_COMPUTE_ALL:
                            if (props.type != ZES_ENGINE_GROUP_COMPUTE_SINGLE && props.type != ZES_ENGINE_GROUP_COMPUTE_ALL) {
                                continue;
                            }
                            break;
                        case ZES_ENGINE_GROUP_RENDER_ALL:
                            if (props.type != ZES_ENGINE_GROUP_RENDER_SINGLE && props.type != ZES_ENGINE_GROUP_RENDER_ALL) {
                                continue;
                            }
                            break;
                        case ZES_ENGINE_GROUP_MEDIA_ALL:
                            if (props.type != ZES_ENGINE_GROUP_MEDIA_ALL && !(props.type == ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE || props.type == ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE || props.type == ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE)) {
                                continue;
                            }
                            break;
                        case ZES_ENGINE_GROUP_COPY_ALL:
                            if (props.type != ZES_ENGINE_GROUP_COPY_SINGLE && props.type != ZES_ENGINE_GROUP_COPY_ALL) {
                                continue;
                            }
                            break;
                        case ZES_ENGINE_GROUP_3D_ALL:
                            if (props.type != ZES_ENGINE_GROUP_3D_SINGLE && props.type != ZES_ENGINE_GROUP_3D_ALL) {
                                continue;
                            }
                            break;
                        default:
                            break;
                    }
                    zes_engine_stats_t snap = {};
                    XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetActivity(engine, &snap));
                    if (res == ZE_RESULT_SUCCESS) {
                        ExtendedMeasurementData data;
                        data.on_subdevice = props.onSubdevice;
                        data.subdevice_id = props.subdeviceId;
                        data.type = props.type;
                        data.active_time = snap.activeTime;
                        data.timestamp = snap.timestamp;
                        ret->addExtendedData(uint64_t(engine), data);
                        data_acquired = true;
                    } else {
                        exception_msgs["zesEngineGetActivity"] = res;
                    }
                } else {
                    exception_msgs["zesEngineGetProperties"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumEngineGroups"] = res;
        }
    } else {
        exception_msgs["zesDeviceEnumEngineGroups"] = res;
    }

    if (data_acquired) {
        if (!exception_msgs.empty()) {
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__) + ". Engine group type " + std::to_string(engine_group_type));
        }
        return ret;
    } else {
        throw BaseException(buildErrors(exception_msgs, __func__, __LINE__) + ". Engine group type " + std::to_string(engine_group_type));
    }
}

void GPUDeviceStub::getSchedulers(const zes_device_handle_t& device, std::vector<Scheduler>& schedulers) {
    if (device == nullptr) {
        return;
    }
    uint32_t scheduler_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumSchedulers(device, &scheduler_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_sched_handle_t> scheds(scheduler_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumSchedulers(device, &scheduler_count, scheds.data()));
        for (auto& sched : scheds) {
            zes_sched_properties_t props;
            XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerGetProperties(sched, &props));
            if (res == ZE_RESULT_SUCCESS) {
                zes_sched_mode_t mode;
                zes_sched_timeout_properties_t timeout;
                zes_sched_timeslice_properties_t timeslice;
                uint64_t val1, val2;
                XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerGetCurrentMode(sched, &mode));
                if (mode == ZES_SCHED_MODE_TIMEOUT) {
                    XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerGetTimeoutModeProperties(sched, false, &timeout));
                    val1 = timeout.watchdogTimeout;
                    val2 = 0;
                } else if (mode == ZES_SCHED_MODE_TIMESLICE) {
                    XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerGetTimesliceModeProperties(sched, false, &timeslice));
                    val1 = timeslice.interval;
                    val2 = timeslice.yieldTimeout;
                } else if (mode == ZES_SCHED_MODE_EXCLUSIVE) {
                    val1 = 0;
                    val2 = 0;
                } else {
                    val1 = -1;
                    val2 = -1;
                }
                schedulers.push_back(*(new Scheduler(props.onSubdevice,
                                                     props.subdeviceId,
                                                     props.canControl,
                                                     props.engines,
                                                     props.supportedModes,
                                                     mode, val1, val2)));
            }
        }
    }
}

bool GPUDeviceStub::resetDevice(const zes_device_handle_t& device, ze_bool_t force) {
    if (device == nullptr) {
        return false;
    }
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceReset(device, force));
    if (res == ZE_RESULT_SUCCESS) {
        return true;
    } else {
        return false;
    }
}

void GPUDeviceStub::getDeviceProcessState(const zes_device_handle_t& device, std::vector<device_process>& processes) {
    if (device == nullptr) {
        return;
    }
    uint32_t process_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceProcessesGetState(device, &process_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_process_state_t> procs(process_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceProcessesGetState(device, &process_count, procs.data()));
        for (auto& proc : procs) {
            std::string pn = getProcessName(proc.processId);
            processes.push_back(*(new device_process(proc.processId, proc.memSize, proc.sharedSize, proc.engines, pn)));
        }
    } else {
        return;
    }
}

/* At the first stage:
1. C-stlye string, file API are used, but SDL guideline is followed (task T196 was checked)
2. No diagnositic mechanism for troubleshooting yet
3. Helper fucntions might be moved to other source file for more orgnized structure
*/

static bool strToUInt32(uint32_t *val, char *buf) {
    char *endAddr;
    errno = 0;
    int32_t ret  = strtol(buf, &endAddr, 0);
    if ((errno == ERANGE && (ret  == LONG_MAX || ret  == LONG_MIN))
            || (errno != 0 && ret == 0) || endAddr == buf || ret < 0) {
        return false;
    }
    *val = (uint32_t)ret;
    return true;
}

static bool strToUInt64(uint64_t *val, char *buf) {
    char *endAddr;
    errno = 0;
    int64_t ret  = strtoll(buf, &endAddr, 0);
    if ((errno == ERANGE && (ret == LLONG_MAX || ret == LLONG_MIN))
            || (errno != 0 && ret == 0) || endAddr == buf || ret < 0) {
        return false;
    }
    *val = (uint64_t)ret;
    return true;
}

#define BUF_SIZE 128
static bool readStrSysFsFile(char *buf, char *fileName) {
    int fd = open(fileName, O_RDONLY);
    if (fd < 0) {
        return false;
    }
    int szRead = read(fd, buf, BUF_SIZE);
    close(fd);
    if (szRead < 0 || szRead >= BUF_SIZE) {
        return false;
    }
    buf[szRead] = 0;
    return true;
}

//The parameter round must be eithr 0 or 1
static bool getEngineActiveTime(device_util_by_proc &util, uint32_t round,
        uint32_t card_idx, char *client) {

    char path[PATH_MAX];
    char buf[BUF_SIZE];

    //read rendering engine data
    int len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/busy/0",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) == false) {
        return false;
    }
    if (strToUInt64(&(util.reData[round]), buf) == false) {
        return false;
    }

    //read copy engine data
    len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/busy/1",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) == false) {
        return false;
    }
    if (strToUInt64(&(util.cpyData[round]), buf) == false) {
        return false;
    }

    //read media engine data
    len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/busy/2",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) == false) {
        return false;
    }
    if (strToUInt64(&(util.meData[round]), buf) == false) {
        return false;
    }

    //read media enhancement engine data
    len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/busy/3",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) == false) {
        return false;
    }
    if (strToUInt64(&(util.meeData[round]), buf) == false) {
        return false;
    }

    //read compute engine data
    len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/busy/4",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) == false) {
        return false;
    }
    if (strToUInt64(&(util.ceData[round]), buf) == false) {
        return false;
    }

    return true;
}

static bool getProcNameAndMem(device_util_by_proc &util,uint32_t card_idx,
        char *client) {
    char path[PATH_MAX];
    char buf[BUF_SIZE];
    //Read proc name
    int len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/name",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) != true) {
        return false;
    }
    std::string procName(buf);
    procName.pop_back();
    util.setProcessName(procName);

    //Read mem size
    len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/total_device_memory_buffer_objects/created_bytes",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) != true) {
        return false;
    }
    uint64_t memSize = 0;
    if (strToUInt64(&memSize, buf) == false) {
        return false;
    }
    util.setMemSize(memSize);

    //Read shared mem size
    len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/total_device_memory_buffer_objects/imported_bytes",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) != true) {
        return false;
    }
    uint64_t sharedMemSize = 0;
    if (strToUInt64(&sharedMemSize, buf) == false) {
        return false;
    }
    util.setSharedMemSize(sharedMemSize);
    return true;
}

static bool getProcID(uint32_t &pid, uint32_t card_idx, char *client) {
    char path[PATH_MAX];
    char buf[BUF_SIZE];
    int len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients/%s/pid",
            card_idx, client);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    if (readStrSysFsFile(buf, path) != true) {
        return false;
    }
    char *p = buf;
    // the pid file may come with a pair of <>, skip '<' in the case
    if (p[0] == '<') {
        p++;
    }
    if (strToUInt32(&pid, p) == false) {
        return false;
    }
    return true;
}

static bool getCardIdx(uint32_t &card_idx, const zes_device_handle_t& device) {
    ze_result_t res;
    zes_pci_properties_t pci_props;
    XPUM_ZE_HANDLE_LOCK(device,
            res = zesDevicePciGetProperties(device, &pci_props));
    if (res != ZE_RESULT_SUCCESS) {
        return false;
    }

    char path[PATH_MAX];
    char buf[BUF_SIZE];
    char uevent[1024];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    int len = 0;

    pdir = opendir("/sys/class/drm");
    if (pdir == NULL) {
        return false;
    }

    bool ret = false;
    while ((pdirent = readdir(pdir)) != NULL) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        if (strncmp(pdirent->d_name, "card", 4) != 0) {
            continue;
        }
        if (strstr(pdirent->d_name, "-") != NULL) {
            continue;
        }
        len = snprintf(path, PATH_MAX, "/sys/class/drm/%s/device/uevent",
                pdirent->d_name);
        if (len <= 0 || len >= PATH_MAX) {
            break;
        }
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            break;
        }
        int szRead = read(fd, uevent, 1024);
        close(fd);
        if (szRead < 0 || szRead >= 1024) {
            break;
        }
        uevent[szRead] = 0;
        len = snprintf(buf, BUF_SIZE, "%04d:%02x:%02x.%x",
                pci_props.address.domain, pci_props.address.bus,
                pci_props.address.device, pci_props.address.function);
        if (strstr(uevent, buf) != NULL) {
            sscanf(pdirent->d_name, "card%d", &card_idx);
            ret = true;
            break;
        }
    }
    closedir(pdir);
    return ret;
}


typedef struct {
    uint32_t dup_cnt;
    uint32_t dup_num;
    device_util_by_proc *putil;
} dup_proc_t;

static bool mergeDupProc(std::map<uint32_t, dup_proc_t>& dup_proc_map,
        std::vector<device_util_by_proc>& utils) {
    //Convert dupilication counter to number of duplicated process (n)
    //n * (n - 1) = counted number
    //find n and assign it to dup_cnt
    uint32_t solved= 0;
    auto iter1 = dup_proc_map.begin();
    while (iter1 != dup_proc_map.end()) {
        for (uint32_t n = 2; n < 1024; n++) {
            if (n * (n - 1) == iter1->second.dup_cnt) {
                iter1->second.dup_num = n;
                solved++;
                break;
            }
        }
        iter1++;
    }
    if (solved != dup_proc_map.size()) {
        return false;
    }

    //Remove invalid process from the vector
    //Merge util data for duplicated pid
    auto iter = utils.begin();
    while (iter != utils.end()) {
        if (iter->elapsed == 0) {
            iter = utils.erase(iter);
        } else {
            if (dup_proc_map.find(iter->getProcessId()) ==
                    dup_proc_map.end()) {
                iter++;
            } else {
                device_util_by_proc *putil =
                    dup_proc_map[iter->getProcessId()].putil;
                iter->merge(putil);
                if (dup_proc_map[iter->getProcessId()].dup_num > 1) {
                    putil->setval(&(*iter));
                    dup_proc_map[iter->getProcessId()].dup_num--;
                    iter = utils.erase(iter);
                } else {
                    iter++;
                }
            }
        }
    }
    return true;
}

//The first round to read utilization (active time) data per device/ card
static bool readUtil1(std::vector<device_util_by_proc>& vec, 
		uint32_t& card_idx, const zes_device_handle_t& device, 
		std::string device_id) {
    
    char path[PATH_MAX];
    int len = 0;
    DIR* pdir = NULL;
    struct dirent* pdirent = NULL;
    
    if (getCardIdx(card_idx, device) == false) {
        return false;
    }

    len = snprintf(path, PATH_MAX,
                   "/sys/class/drm/card%d/clients", card_idx);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    pdir = opendir(path);
    if (pdir == NULL) {
        return false;
    }
    while ((pdirent = readdir(pdir))) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        uint32_t pid = 0;
        if (getProcID(pid, card_idx, pdirent->d_name) == false) {
            closedir(pdir);
            return false;
        }
        device_util_by_proc util(pid);
        util.setDeviceId(std::stoi(device_id));
        memcpy(util.d_name, pdirent->d_name, 32);
        util.d_name[31] = '\0';
        if (getEngineActiveTime(util, 0, card_idx, pdirent->d_name) == false) {
            closedir(pdir);
            return false;
        }
        vec.push_back(util);
    }
    closedir(pdir);
    return true;
}

//The second round to read utilization (active time) data per device/ card
static bool readUtil2(std::vector<device_util_by_proc>& vec, 
    uint32_t card_idx, uint64_t elapsed) { 

    char path[PATH_MAX];
    int len = 0;
    DIR* pdir = NULL;
    struct dirent* pdirent = NULL;
    std::map<uint32_t, dup_proc_t> dup_proc_map;

    len = snprintf(path, PATH_MAX,
            "/sys/class/drm/card%d/clients", card_idx);
    if (len <= 0 || len >= PATH_MAX) {
        return false;
    }
    pdir = opendir(path);
    if (pdir == NULL) {
        return false;
    }
    while ((pdirent = readdir(pdir))) {
        if (pdirent->d_name[0] == '.') {
            continue;
        }
        uint32_t pid = 0;
        if (getProcID(pid, card_idx, pdirent->d_name) == false) {
            closedir(pdir);
            return false;
        }
        device_util_by_proc *putil = NULL;
        for (auto &util : vec) {
            if (util.getProcessId() == pid) {
                if (strncmp(util.d_name,
                            pdirent->d_name, strnlen(util.d_name, 32)) == 0) {
                    if (getEngineActiveTime(util, 1, card_idx, pdirent->d_name)
                            == true) {
                        putil = &util;
                    }
                } else {
                    if (dup_proc_map.find(pid) == dup_proc_map.end()) {
                        dup_proc_t proc;
                        //dup_cnt (duplication counter) would be n * (n - 1)
                        //n = number of duplicated processes
                        //Memory would be released after merge
                        proc.dup_cnt = 1;
                        proc.putil = new device_util_by_proc(pid);
                        dup_proc_map.insert(std::make_pair(pid, proc));
                    } else {
                        dup_proc_map[pid].dup_cnt++;
                    }
                }
            }
        }
        //if pid is not found, it might be created during the nap time, skip it
        if (putil == NULL) {
            continue;
        }

        if (getProcNameAndMem(*putil, card_idx, pdirent->d_name) == false) {
            closedir(pdir);
            auto iter = dup_proc_map.begin();
            while (iter != dup_proc_map.end()) {
                delete iter->second.putil;
                iter++;
            }
            return false;
        }
        putil->elapsed = elapsed;
    }
    closedir(pdir);

    bool ret = mergeDupProc(dup_proc_map, vec);
    //Whatever release the memory, though it could be done in the merge process
    //but there is risk if the the sysfs does not behave as expected
    auto iter = dup_proc_map.begin();
    while (iter != dup_proc_map.end()) {
        delete iter->second.putil;
        iter++;
    }

    return ret;
}

//Get per process utilization for multple devices
//For each device/ card, there is a card_idx, device_id, device_handle
//and a vector of utilizations, the utilizations of all devices are returned
//by a vector of utilzation vector
bool GPUDeviceStub::getDeviceUtilByProc(
    const std::vector<zes_device_handle_t>& devices,
    const std::vector<std::string>& device_ids,
    uint32_t utilInterval,
    std::vector<std::vector<device_util_by_proc>>& utils) {

    std::vector<uint32_t> card_idxes;
    auto begin = std::chrono::high_resolution_clock::now();
    auto size0 = devices.size();
    for (uint64_t i = 0; i < size0; i++) {
        std::vector<device_util_by_proc> vec;
        uint32_t card_idx = 0;
        if (readUtil1(vec, card_idx, devices[i], device_ids[i]) == false) {
            utils.clear();
            return false;
        }
        utils.push_back(vec);
        card_idxes.push_back(card_idx);
    }

    //Nap time
    std::this_thread::sleep_for(std::chrono::microseconds(utilInterval));
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dura = (end - begin) * 1000 * 1000 * 1000;
    uint64_t elapsed = (uint64_t)dura.count();
    
    auto size1 = utils.size();
    for (uint64_t i = 0; i < size1;  i++) {
        if (readUtil2(utils[i], card_idxes[i], elapsed) == false) {
            utils.clear();
            return false;
        }
    }
    
    return true;
}

std::string GPUDeviceStub::getProcessName(uint32_t processId) {
    std::string processName = "";
    std::ifstream pinfo;
    char path[255];
    sprintf(path, "/proc/%d/cmdline", processId);
    pinfo.open(path);
    if (pinfo.is_open()) {
        std::getline(pinfo, processName);
        pinfo.close();
    }
    return processName;
}

bool GPUDeviceStub::setPerformanceFactor(const zes_device_handle_t& device, PerformanceFactor& pf) {
    if (device == nullptr) {
        return false;
    }
    uint32_t pfCount = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPerformanceFactorDomains(device, &pfCount, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        zes_perf_handle_t hPerf[pfCount];
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPerformanceFactorDomains(device, &pfCount, hPerf));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto perf : hPerf) {
                zes_perf_properties_t prop;
                XPUM_ZE_HANDLE_LOCK(perf, res = zesPerformanceFactorGetProperties(perf, &prop));
                if (res == ZE_RESULT_SUCCESS) {
                    if (prop.subdeviceId == pf.getSubdeviceId() && prop.engines == pf.getEngine()) {
                        XPUM_ZE_HANDLE_LOCK(perf, res = zesPerformanceFactorSetConfig(perf, pf.getFactor()));
                        if (res == ZE_RESULT_SUCCESS) {
                            return true;
                        } else {
                            return false;
                        }
                    }
                    continue;
                } else {
                    return false;
                }
            }
            return false;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

void GPUDeviceStub::getPerformanceFactor(const zes_device_handle_t& device, std::vector<PerformanceFactor>& pf) {
    if (device == nullptr) {
        return;
    }
    uint32_t pfCount = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPerformanceFactorDomains(device, &pfCount, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        zes_perf_handle_t hPerf[pfCount];
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPerformanceFactorDomains(device, &pfCount, hPerf));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto perf : hPerf) {
                zes_perf_properties_t prop;
                double factor;
                XPUM_ZE_HANDLE_LOCK(perf, res = zesPerformanceFactorGetProperties(perf, &prop));
                if (res == ZE_RESULT_SUCCESS) {
                    XPUM_ZE_HANDLE_LOCK(perf, res = zesPerformanceFactorGetConfig(perf, &factor));
                    if (res == ZE_RESULT_SUCCESS) {
                        pf.push_back(*(new PerformanceFactor(prop.onSubdevice, prop.subdeviceId, prop.engines, factor)));
                        continue;
                    }
                } else {
                    continue;
                }
            }
            return;
        } else {
            return;
        }
    } else {
        return;
    }
}

void GPUDeviceStub::getStandbys(const zes_device_handle_t& device, std::vector<Standby>& standbys) {
    if (device == nullptr) {
        return;
    }
    uint32_t standby_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumStandbyDomains(device, &standby_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_standby_handle_t> stans(standby_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumStandbyDomains(device, &standby_count, stans.data()));
        for (auto& stan : stans) {
            zes_standby_properties_t props;
            XPUM_ZE_HANDLE_LOCK(stan, res = zesStandbyGetProperties(stan, &props));
            if (res == ZE_RESULT_SUCCESS) {
                zes_standby_promo_mode_t mode;
                XPUM_ZE_HANDLE_LOCK(stan, res = zesStandbyGetMode(stan, &mode));
                standbys.push_back(*(new Standby(props.type, (bool)props.onSubdevice, props.subdeviceId, mode)));
            }
        }
    }
}

void GPUDeviceStub::getPowerProps(const zes_device_handle_t& device, std::vector<Power>& powers) {
    if (device == nullptr) {
        return;
    }
    uint32_t power_domain_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& power : power_handles) {
            zes_power_properties_t props = {};
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
            if (res == ZE_RESULT_SUCCESS) {
                powers.push_back(*(new Power(props.onSubdevice,
                                             props.subdeviceId,
                                             props.canControl,
                                             props.isEnergyThresholdSupported,
                                             props.defaultLimit,
                                             props.minLimit,
                                             props.maxLimit)));
            }
        }
    }
}

void GPUDeviceStub::getAllPowerLimits(const zes_device_handle_t& device,
                                      std::vector<uint32_t>& tileIds,
                                      std::vector<Power_sustained_limit_t>& sustained_limits,
                                      std::vector<Power_burst_limit_t>& burst_limits,
                                      std::vector<Power_peak_limit_t>& peak_limit) {
    if (device == nullptr) {
        return;
    }
    uint32_t power_domain_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& power : power_handles) {
            zes_power_sustained_limit_t sustained;
            Power_sustained_limit_t* sustainedLimit;
            zes_power_burst_limit_t burst;
            Power_burst_limit_t* burstLimit;
            Power_peak_limit_t* peakLimit;
            zes_power_peak_limit_t peak;
            zes_power_properties_t props = {};
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
            if (res == ZE_RESULT_SUCCESS) {
                tileIds.push_back(props.subdeviceId);
                XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetLimits(power, &sustained, &burst, &peak));
                if (res == ZE_RESULT_SUCCESS) {
                    sustainedLimit = new Power_sustained_limit_t();
                    sustainedLimit->enabled = sustained.enabled;
                    sustainedLimit->power = sustained.power;
                    sustainedLimit->interval = sustained.interval;
                    sustained_limits.push_back(*sustainedLimit);
                    burstLimit = new Power_burst_limit_t();
                    burstLimit->enabled = burst.enabled;
                    burstLimit->power = burst.power;
                    burst_limits.push_back(*burstLimit);
                    peakLimit = new Power_peak_limit_t();
                    peakLimit->power_AC = peak.powerAC;
                    peakLimit->power_DC = peak.powerDC;
                    peak_limit.push_back(*peakLimit);
                    delete sustainedLimit;
                    delete burstLimit;
                    delete peakLimit;
                }
            }
        }
    }
}

void GPUDeviceStub::getPowerLimits(const zes_device_handle_t& device,
                                   Power_sustained_limit_t& sustained_limit,
                                   Power_burst_limit_t& burst_limit,
                                   Power_peak_limit_t& peak_limit) {
    if (device == nullptr) {
        return;
    }
    uint32_t power_domain_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& power : power_handles) {
            zes_power_sustained_limit_t sustained;
            //zes_power_burst_limit_t burst;
            //zes_power_peak_limit_t peak;
            zes_power_properties_t props = {};
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
            if (res == ZE_RESULT_SUCCESS) {
                if (props.onSubdevice == true) {
                    continue;
                }
            }
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetLimits(power, &sustained, nullptr, nullptr));
            if (res == ZE_RESULT_SUCCESS) {
                sustained_limit.enabled = sustained.enabled;
                sustained_limit.power = sustained.power;
                sustained_limit.interval = 0;
/*
                sustained_limit.interval = sustained.interval;
                burst_limit.enabled = burst.enabled;
                burst_limit.power = burst.power;

                peak_limit.power_AC = peak.powerAC;
                peak_limit.power_DC = peak.powerDC;
*/
            }
        }
    }
}

bool GPUDeviceStub::setPowerSustainedLimits(const zes_device_handle_t& device, int32_t tileId,
                                            const Power_sustained_limit_t& sustained_limit) {
    if (device == nullptr) {
        return false;
    }
    uint32_t power_domain_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& power : power_handles) {
            zes_power_properties_t props = {};
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
            if (res == ZE_RESULT_SUCCESS) {
                if (props.subdeviceId == (uint32_t)tileId || (tileId == -1 && props.onSubdevice == false)) {
                    zes_power_sustained_limit_t sustained;
                    sustained.enabled = sustained_limit.enabled;
                    sustained.power = sustained_limit.power;
                    sustained.interval = sustained_limit.interval;
                    XPUM_ZE_HANDLE_LOCK(power, res = zesPowerSetLimits(power, &sustained, nullptr, nullptr));
                    if (res == ZE_RESULT_SUCCESS) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool GPUDeviceStub::setPowerBurstLimits(const zes_device_handle_t& device,
                                        const Power_burst_limit_t& burst_limit) {
    if (device == nullptr) {
        return false;
    }
    uint32_t power_domain_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& power : power_handles) {
            zes_power_burst_limit_t burst;
            burst.enabled = burst_limit.enabled;
            burst.power = burst_limit.power;
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerSetLimits(power, nullptr, &burst, nullptr));
            if (res == ZE_RESULT_SUCCESS) {
                return true;
            }
        }
    }
    return false;
}

bool GPUDeviceStub::setPowerPeakLimits(const zes_device_handle_t& device,
                                       const Power_peak_limit_t& peak_limit) {
    if (device == nullptr) {
        return false;
    }
    uint32_t power_domain_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
    std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& power : power_handles) {
            zes_power_peak_limit_t peak;
            peak.powerAC = peak_limit.power_AC;
            peak.powerDC = peak_limit.power_DC;
            XPUM_ZE_HANDLE_LOCK(power, res = zesPowerSetLimits(power, nullptr, nullptr, &peak));
            if (res == ZE_RESULT_SUCCESS) {
                return true;
            }
        }
    }
    return false;
}

void GPUDeviceStub::getFrequencyRanges(const zes_device_handle_t& device, std::vector<Frequency>& frequencies) {
    if (device == nullptr) {
        return;
    }
    uint32_t freq_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr));
    std::vector<zes_freq_handle_t> freq_handles(freq_count);
    if (res == ZE_RESULT_SUCCESS) {
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data()));
        for (auto& ph_freq : freq_handles) {
            zes_freq_properties_t prop = {};
            prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
            XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetProperties(ph_freq, &prop));
            if (res == ZE_RESULT_SUCCESS) {
                if (prop.type != ZES_FREQ_DOMAIN_GPU) {
                    continue;
                }
                zes_freq_range_t range;
                XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetRange(ph_freq, &range));
                if (res == ZE_RESULT_SUCCESS) {
                    frequencies.push_back(*(new Frequency(prop.type,
                                                          prop.onSubdevice,
                                                          prop.subdeviceId,
                                                          prop.canControl,
                                                          prop.isThrottleEventSupported,
                                                          range.min,
                                                          range.max)));
                }
            }
        }
    }
}

void GPUDeviceStub::getFreqAvailableClocks(const zes_device_handle_t& device, uint32_t subdevice_id, std::vector<double>& clocks) {
    if (device == nullptr) {
        return;
    }
    uint32_t freq_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr));
    std::vector<zes_freq_handle_t> freq_handles(freq_count);
    if (res == ZE_RESULT_SUCCESS) {
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data()));
        for (auto& ph_freq : freq_handles) {
            zes_freq_properties_t prop = {};
            prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
            XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetProperties(ph_freq, &prop));
            if (res == ZE_RESULT_SUCCESS) {
                if (prop.type != ZES_FREQ_DOMAIN_GPU || prop.subdeviceId != subdevice_id) {
                    continue;
                }
                uint32_t pCount = 0;
                XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetAvailableClocks(ph_freq, &pCount, nullptr));
                double clockArray[pCount];
                if (res == ZE_RESULT_SUCCESS) {
                    XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetAvailableClocks(ph_freq, &pCount, clockArray));
                    for (uint32_t i = 0; i < pCount; i++) {
                        clocks.push_back(clockArray[i]);
                    }
                }
            }
        }
    }
}

bool GPUDeviceStub::setFrequencyRangeForAll(const zes_device_handle_t& device, const Frequency& freq) {
    if (device == nullptr) {
        return false;
    }
    uint32_t freq_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr));
    std::vector<zes_freq_handle_t> freq_handles(freq_count);
    if (res == ZE_RESULT_SUCCESS) {
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data()));
        for (auto& ph_freq : freq_handles) {
            zes_freq_properties_t prop = {};
            prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
            XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetProperties(ph_freq, &prop));
            if (res == ZE_RESULT_SUCCESS) {
                if (prop.type != freq.getType()) {
                    continue;
                }
                zes_freq_range_t range;
                range.min = freq.getMin();
                range.max = freq.getMax();
                XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencySetRange(ph_freq, &range));
                /*if (res != ZE_RESULT_SUCCESS) {
                    return false;
                }*/
            }
        }
        return true;
    }
    return false;
}
bool GPUDeviceStub::setFrequencyRange(const zes_device_handle_t& device, const Frequency& freq) {
    if (device == nullptr) {
        return false;
    }
    uint32_t freq_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr));
    std::vector<zes_freq_handle_t> freq_handles(freq_count);
    if (res == ZE_RESULT_SUCCESS) {
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data()));
        for (auto& ph_freq : freq_handles) {
            zes_freq_properties_t prop = {};
            prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
            XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetProperties(ph_freq, &prop));
            if (res == ZE_RESULT_SUCCESS) {
                if (prop.type != freq.getType() || prop.subdeviceId != freq.getSubdeviceId()) {
                    continue;
                }
                zes_freq_range_t range;
                range.min = freq.getMin();
                range.max = freq.getMax();
                XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencySetRange(ph_freq, &range));
                if (res == ZE_RESULT_SUCCESS) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool GPUDeviceStub::setStandby(const zes_device_handle_t& device, const Standby& standby) {
    if (device == nullptr) {
        return false;
    }
    uint32_t standby_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumStandbyDomains(device, &standby_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_standby_handle_t> stans(standby_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumStandbyDomains(device, &standby_count, stans.data()));
        for (auto& stan : stans) {
            zes_standby_properties_t props;
            XPUM_ZE_HANDLE_LOCK(stan, res = zesStandbyGetProperties(stan, &props));
            if (res == ZE_RESULT_SUCCESS) {
                if (props.subdeviceId != standby.getSubdeviceId()) {
                    continue;
                }
                XPUM_ZE_HANDLE_LOCK(stan, res = zesStandbySetMode(stan, standby.getMode()));
                if (res == ZE_RESULT_SUCCESS) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool GPUDeviceStub::setSchedulerTimeoutMode(const zes_device_handle_t& device,
                                            const SchedulerTimeoutMode& mode) {
    bool ret = false;
    if (device == nullptr) {
        return ret;
    }
    uint32_t scheduler_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumSchedulers(device, &scheduler_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_sched_handle_t> scheds(scheduler_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumSchedulers(device, &scheduler_count, scheds.data()));
        for (auto& sched : scheds) {
            zes_sched_properties_t props;
            XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerGetProperties(sched, &props));
            if (res == ZE_RESULT_SUCCESS) {
                if (props.subdeviceId != mode.subdevice_Id) {
                    continue;
                }
                ze_bool_t needReload;
                zes_sched_timeout_properties_t prop;
                prop.stype = ZES_STRUCTURE_TYPE_SCHED_TIMEOUT_PROPERTIES;
                prop.pNext = nullptr;
                prop.watchdogTimeout = mode.mode_setting.watchdogTimeout;
                XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerSetTimeoutMode(sched, &prop, &needReload));
                if (res == ZE_RESULT_SUCCESS) {
                    ret = ret || true;
                }
            }
        }
    }
    return ret;
}

bool GPUDeviceStub::setSchedulerTimesliceMode(const zes_device_handle_t& device,
                                              const SchedulerTimesliceMode& mode) {
    bool ret = false;
    if (device == nullptr) {
        return ret;
    }
    uint32_t scheduler_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumSchedulers(device, &scheduler_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_sched_handle_t> scheds(scheduler_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumSchedulers(device, &scheduler_count, scheds.data()));
        for (auto& sched : scheds) {
            zes_sched_properties_t props;
            XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerGetProperties(sched, &props));
            if (res == ZE_RESULT_SUCCESS) {
                if (props.subdeviceId != mode.subdevice_Id) {
                    continue;
                }
                ze_bool_t needReload;
                zes_sched_timeslice_properties_t prop;
                prop.stype = ZES_STRUCTURE_TYPE_SCHED_TIMESLICE_PROPERTIES;
                prop.pNext = nullptr;
                prop.interval = mode.mode_setting.interval;
                prop.yieldTimeout = mode.mode_setting.yieldTimeout;
                XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerSetTimesliceMode(sched, &prop, &needReload));
                if (res == ZE_RESULT_SUCCESS) {
                    ret = ret || true;
                }
            }
        }
    }
    return ret;
}

bool GPUDeviceStub::setSchedulerExclusiveMode(const zes_device_handle_t& device, const SchedulerExclusiveMode& mode) {
    bool ret = false;
    if (device == nullptr) {
        return ret;
    }
    uint32_t scheduler_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumSchedulers(device, &scheduler_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_sched_handle_t> scheds(scheduler_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumSchedulers(device, &scheduler_count, scheds.data()));
        for (auto& sched : scheds) {
            zes_sched_properties_t props;
            XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerGetProperties(sched, &props));
            if (res == ZE_RESULT_SUCCESS) {
                if (props.subdeviceId != mode.subdevice_Id) {
                    continue;
                }
                ze_bool_t needReload;
                XPUM_ZE_HANDLE_LOCK(sched, res = zesSchedulerSetExclusiveMode(sched, &needReload));
                if (res == ZE_RESULT_SUCCESS) {
                    ret = ret || true;
                }
            }
        }
    }
    return ret;
}

bool GPUDeviceStub::getFrequencyState(const zes_device_handle_t& device, std::string& freq_throttle_message) {
    bool ret = false;
    if (device == nullptr) {
        return ret;
    }

    uint32_t freq_count = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr));
    std::vector<zes_freq_handle_t> freq_handles(freq_count);
    if (res == ZE_RESULT_SUCCESS) {
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data()));
        for (auto& ph_freq : freq_handles) {
            zes_freq_properties_t props;
            props.pNext = nullptr;
            XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetProperties(ph_freq, &props));
            if (res == ZE_RESULT_SUCCESS) {
                zes_freq_state_t freq_state;
                XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetState(ph_freq, &freq_state));
                if (res == ZE_RESULT_SUCCESS) {
                    if (freq_state.throttleReasons == 0) {   
                        ret = true;
                        continue;
                    }
                    if (props.onSubdevice) {
                        if (freq_throttle_message.size() > 0)
                            freq_throttle_message += " ";
                        freq_throttle_message += "Tile " + std::to_string(props.subdeviceId) + " " + get_freq_throttle_string(freq_state.throttleReasons);
                        ret = true;
                    } else {
                        freq_throttle_message = "Device " + get_freq_throttle_string(freq_state.throttleReasons);
                        return true;
                    }
                }
            }
        }
    }

    return ret;
}

void GPUDeviceStub::getHealthStatus(const zes_device_handle_t& device, xpum_health_type_t type, xpum_health_data_t* data,
                                    int core_thermal_threshold, int memory_thermal_threshold, int power_threshold, bool global_default_limit) {
    if (device == nullptr) {
        return;
    }

    xpum_health_status_t status = xpum_health_status_t::XPUM_HEALTH_STATUS_UNKNOWN;
    std::string description;

    if (type == xpum_health_type_t::XPUM_HEALTH_MEMORY) {
        description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_UNKNOWN);
        uint32_t mem_module_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zes_mem_handle_t> mems(mem_module_count);
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& mem : mems) {
                    zes_mem_state_t memory_state = {};
                    memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
                    XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetState(mem, &memory_state));
                    if (res == ZE_RESULT_SUCCESS) {
                        if (memory_state.health == ZES_MEM_HEALTH_OK && (int)status < ZES_MEM_HEALTH_OK) {
                            status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
                            description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_OK);
                        }
                        if (memory_state.health == ZES_MEM_HEALTH_DEGRADED && (int)status < ZES_MEM_HEALTH_DEGRADED) {
                            status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
                            description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_DEGRADED);
                        }
                        if (memory_state.health == ZES_MEM_HEALTH_CRITICAL && (int)status < ZES_MEM_HEALTH_CRITICAL) {
                            status = xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL;
                            description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_CRITICAL);
                        }
                        if (memory_state.health == ZES_MEM_HEALTH_REPLACE && (int)status < ZES_MEM_HEALTH_REPLACE) {
                            status = xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL;
                            description = get_health_state_string(zes_mem_health_t::ZES_MEM_HEALTH_REPLACE);
                        }
                    }
                }
            }
        }
    } else if (type == xpum_health_type_t::XPUM_HEALTH_POWER) {
        if (power_threshold <= 0) {
            description = "Power health threshold is not set";
            return;
        }

        description = "The power health cannot be determined.";
        uint32_t power_domain_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
        std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
        if (res == ZE_RESULT_SUCCESS) {
            auto current_device_value = 0;
            auto current_sub_device_value_sum = 0;
            for (auto& power : power_handles) {
                zes_power_properties_t props = {};
                props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
                if (res != ZE_RESULT_SUCCESS) {
                    continue;
                }
                zes_power_energy_counter_t snap1, snap2;
                XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap1));
                if (res == ZE_RESULT_SUCCESS) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::POWER_MONITOR_INTERNAL_PERIOD));
                    XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap2));
                    if (res == ZE_RESULT_SUCCESS) {
                        int value = (snap2.energy - snap1.energy) / (snap2.timestamp - snap1.timestamp);
                        if (!props.onSubdevice) {
                            current_device_value = value;
                        } else {
                            current_sub_device_value_sum += value;
                        }
                    }
                }
            }
            XPUM_LOG_DEBUG("health: current device power value: {}", current_device_value);
            XPUM_LOG_DEBUG("health: current sum of sub-device power values: {}", current_sub_device_value_sum);
            auto power_val = std::max(current_device_value, current_sub_device_value_sum);
            if (power_val < power_threshold && status < xpum_health_status_t::XPUM_HEALTH_STATUS_OK) {
                status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
                description = "All power domains are healthy.";
            }
            if (power_val >= power_threshold && status < xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING) {
                status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
                description = "Find an unhealthy power domain. Its power is " + std::to_string(power_val) + " that reaches or exceeds the " + (global_default_limit ? "global defalut limit " : "threshold ") + std::to_string(power_threshold) + ".";
            }
        }
    } else if (type == xpum_health_type_t::XPUM_HEALTH_CORE_THERMAL || type == xpum_health_type_t::XPUM_HEALTH_MEMORY_THERMAL) {
        if (core_thermal_threshold <= 0 || memory_thermal_threshold <= 0) {
            description = "Temperature health threshold is not set";
            return;
        }
        int thermal_threshold = 0;
        if (type == xpum_health_type_t::XPUM_HEALTH_CORE_THERMAL)
            thermal_threshold = core_thermal_threshold;
        else
            thermal_threshold = memory_thermal_threshold;
        double temp_val = 0;
        description = "The temperature health cannot be determined.";
        uint32_t temp_sensor_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, nullptr));
        if (temp_sensor_count == 0 && type == xpum_health_type_t::XPUM_HEALTH_CORE_THERMAL) {
            zes_device_properties_t props;
            props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceGetProperties(device, &props));
            if (res == ZE_RESULT_SUCCESS && (to_hex_string(props.core.deviceId).find("56c0") != std::string::npos 
                    || to_hex_string(props.core.deviceId).find("56c1") != std::string::npos)) {
                int val = get_register_value_from_sys(device, 0x145978);
                if (val > 0) {
                    temp_val = val;
                }
            }
        } else if (temp_sensor_count > 0) {
            std::vector<zes_temp_handle_t> temp_sensors(temp_sensor_count);
            if (res == ZE_RESULT_SUCCESS) {
                XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, temp_sensors.data()));
                for (auto& temp : temp_sensors) {
                    zes_temp_properties_t props;
                    XPUM_ZE_HANDLE_LOCK(temp, res = zesTemperatureGetProperties(temp, &props));
                    if (res != ZE_RESULT_SUCCESS) {
                        continue;
                    }
                    if (type == xpum_health_type_t::XPUM_HEALTH_CORE_THERMAL && props.type != ZES_TEMP_SENSORS_GPU) {
                        continue;
                    }
                    if (type == xpum_health_type_t::XPUM_HEALTH_MEMORY_THERMAL && props.type != ZES_TEMP_SENSORS_MEMORY) {
                        continue;
                    }
                    double val = 0;
                    XPUM_ZE_HANDLE_LOCK(temp, res = zesTemperatureGetState(temp, &val));
                    // filter abnormal temperatures
                    if (res == ZE_RESULT_SUCCESS && val < 150) {
                        temp_val = val;
                    }
                }
            }
        }
        if (temp_val > 0 && temp_val < thermal_threshold && status < xpum_health_status_t::XPUM_HEALTH_STATUS_OK) {
            status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
            description = "All temperature sensors are healthy.";
        }
        if (temp_val >= thermal_threshold && status < xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING) {
            status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
            std::stringstream temp_buffer;
            temp_buffer << std::fixed << std::setprecision(2) << temp_val;
            description = "Find an unhealthy temperature sensor. Its temperature is " + temp_buffer.str() + " that reaches or exceeds the " + (global_default_limit ? "global defalut limit " : "threshold ") + std::to_string(thermal_threshold) + ".";
        }
    } else if (type == xpum_health_type_t::XPUM_HEALTH_FABRIC_PORT) {
        description = "All port statuses cannot be determined.";
        uint32_t fabric_ports_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &fabric_ports_count, nullptr));
        if (res == ZE_RESULT_SUCCESS && fabric_ports_count > 0) {
            std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_ports_count);
            std::vector<std::string> failed_fabric_ports, degraded_fabric_ports, disabled_fabric_ports;
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &fabric_ports_count, fabric_ports.data()));
            for (auto& fabric_port : fabric_ports) {
                zes_fabric_port_properties_t fabric_port_properties;
                fabric_port_properties.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(fabric_port, res = zesFabricPortGetProperties(fabric_port, &fabric_port_properties));
                if (res != ZE_RESULT_SUCCESS) {
                    continue;
                }
                zes_fabric_port_state_t fabric_port_state;
                fabric_port_state.stype = ZES_STRUCTURE_TYPE_FABRIC_PORT_STATE;
                XPUM_ZE_HANDLE_LOCK(fabric_port, res = zesFabricPortGetState(fabric_port, &fabric_port_state));
                if (res != ZE_RESULT_SUCCESS) {
                    continue;
                }
                if (fabric_port_state.status == ZES_FABRIC_PORT_STATUS_FAILED) {
                    failed_fabric_ports.emplace_back("Tile" + std::to_string(fabric_port_properties.portId.attachId) + "-" + std::to_string((int)(fabric_port_properties.portId.portNumber)));
                }
                if (fabric_port_state.status == ZES_FABRIC_PORT_STATUS_DEGRADED) {
                    degraded_fabric_ports.emplace_back("Tile" + std::to_string(fabric_port_properties.portId.attachId) + "-" + std::to_string((int)(fabric_port_properties.portId.portNumber)));
                }
                if (fabric_port_state.status == ZES_FABRIC_PORT_STATUS_DISABLED) {
                    disabled_fabric_ports.emplace_back("Tile" + std::to_string(fabric_port_properties.portId.attachId) + "-" + std::to_string((int)(fabric_port_properties.portId.portNumber)));
                }
            }

            if (failed_fabric_ports.empty() && degraded_fabric_ports.empty() && disabled_fabric_ports.empty()) {
                status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
                description = "All ports are up and operating as expected.";
            } else {
                description = "";
                status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
                if (!failed_fabric_ports.empty()) {
                    status = xpum_health_status_t::XPUM_HEALTH_STATUS_CRITICAL;
                    description += "Ports ";
                    for (auto port : failed_fabric_ports) {
                        description += port + " ";
                    }
                    description += "connection instabilities are preventing workloads making forward progress. ";
                }
                if (!degraded_fabric_ports.empty()) {
                    description += "Ports ";
                    for (auto port : degraded_fabric_ports) {
                        description += port + " ";
                    }
                    description += "are up but have quality and/or speed degradation. ";
                }
                if (!disabled_fabric_ports.empty()) {
                    description += "Ports ";
                    for (auto port : disabled_fabric_ports) {
                        description += port + " ";
                    }
                    description += "are configured down. ";
                }
            }
        } else {
            description = "The device has no Xe Link capability.";
        }
    } else if (type == xpum_health_type_t::XPUM_HEALTH_FREQUENCY) {
        description = "The device frequency state cannot be determined.";
        std::string freq_throttle_message;
        bool get_state = getFrequencyState(device, freq_throttle_message);
        if (get_state) {
            if (!freq_throttle_message.empty()) {
                status = xpum_health_status_t::XPUM_HEALTH_STATUS_WARNING;
                description = freq_throttle_message;
            } else {
                status = xpum_health_status_t::XPUM_HEALTH_STATUS_OK;
                description = "The device frequency not throttled";
            }
        }
    }

    data->status = status;
    int index = 0;
    while (index < (int)description.size() && index < XPUM_MAX_STR_LENGTH - 1) {
        data->description[index] = description[index];
        index++;
    }
    data->description[index] = 0;
}

bool GPUDeviceStub::getFabricPorts(const zes_device_handle_t& device, std::vector<port_info>& portInfo) {
    if (device == nullptr) {
        return false;
    }
    uint32_t numPorts = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &numPorts, nullptr));
    if (res != ZE_RESULT_SUCCESS || numPorts == 0) {
        return false;
    }

    std::vector<zes_fabric_port_handle_t> fp_handles(numPorts);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &numPorts, fp_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& hPort : fp_handles) {
            port_info info;
            zes_fabric_port_properties_t props;
            zes_fabric_port_state_t state;
            zes_fabric_link_type_t link;
            zes_fabric_port_config_t config;
            memset(&props, 0, sizeof(props));
            memset(&state, 0, sizeof(state));
            memset(&link, 0, sizeof(link));
            memset(&config, 0, sizeof(config));

            XPUM_ZE_HANDLE_LOCK(device, res = zesFabricPortGetProperties(hPort, &props));
            if (res != ZE_RESULT_SUCCESS) {
                XPUM_LOG_WARN("Failed to zesFabricPortGetProperties returned: {}", res);
            }
            info.portProps = props;

            XPUM_ZE_HANDLE_LOCK(device, res = zesFabricPortGetState(hPort, &state));
            if (res != ZE_RESULT_SUCCESS) {
                XPUM_LOG_WARN("Failed to zesFabricPortGetState returned: {} port:{}.{}.{}",
                              res, props.portId.fabricId, props.portId.attachId, props.portId.portNumber);
            }
            info.portState = state;

            XPUM_ZE_HANDLE_LOCK(device, res = zesFabricPortGetLinkType(hPort, &link));
            if (res != ZE_RESULT_SUCCESS) {
                XPUM_LOG_WARN("Failed to zesFabricPortGetLinkType returned: {} port:{}.{}.{}",
                              res, props.portId.fabricId, props.portId.attachId, props.portId.portNumber);
            }
            info.portLink = link;

            XPUM_ZE_HANDLE_LOCK(device, res = zesFabricPortGetConfig(hPort, &config));
            if (res != ZE_RESULT_SUCCESS) {
                XPUM_LOG_WARN("Failed to zesFabricPortGetLinkType returned: {} port:{}.{}.{}",
                              res, props.portId.fabricId, props.portId.attachId, props.portId.portNumber);
            }
            info.portConf = config;
            portInfo.push_back(info);
        }
    }
    return true;
}

bool GPUDeviceStub::setFabricPorts(const zes_device_handle_t& device, const port_info_set& portInfoSet) {
    if (device == nullptr) {
        return false;
    }
    uint32_t numPorts = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &numPorts, nullptr));
    if (res != ZE_RESULT_SUCCESS || numPorts == 0) {
        return false;
    }

    std::vector<zes_fabric_port_handle_t> fp_handles(numPorts);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &numPorts, fp_handles.data()));
    if (res == ZE_RESULT_SUCCESS) {
        for (auto& hPort : fp_handles) {
            zes_fabric_port_properties_t props;
            zes_fabric_port_config_t config;
            memset(&props, 0, sizeof(props));
            memset(&config, 0, sizeof(config));

            XPUM_ZE_HANDLE_LOCK(hPort, res = zesFabricPortGetProperties(hPort, &props));
            if (res != ZE_RESULT_SUCCESS) {
                continue;
            }
            if (props.subdeviceId == portInfoSet.subdeviceId && props.portId.portNumber == portInfoSet.portId.portNumber) {
                XPUM_ZE_HANDLE_LOCK(hPort, res = zesFabricPortGetConfig(hPort, &config));
                if (res != ZE_RESULT_SUCCESS) {
                    return false;
                }
                if (portInfoSet.setting_enabled) {
                    config.enabled = portInfoSet.enabled;
                }
                if (portInfoSet.setting_beaconing) {
                    config.beaconing = portInfoSet.beaconing;
                }
                XPUM_ZE_HANDLE_LOCK(hPort, res = zesFabricPortSetConfig(hPort, &config));
                if (res != ZE_RESULT_SUCCESS) {
                    return false;
                } else {
                    return true;
                }
            }
        }
    }
    return false;
}

bool GPUDeviceStub::getEccState(const zes_device_handle_t& device, MemoryEcc& ecc) {
    ecc.setAvailable(false);
    ecc.setConfigurable(false);
    ecc.setCurrent(ECC_STATE_UNAVAILABLE);
    ecc.setPending(ECC_STATE_UNAVAILABLE);
    ecc.setAction(ECC_ACTION_NONE);

    if (device == nullptr) {
        return false;
    }
    //temp
    return true;
#if 0
    ze_result_t res;
    ze_bool_t boolValue = false;

    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEccAvailable(device,  &boolValue));
    if (res != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (boolValue == false) {
        return true;
    }
    ecc.setAvailable(true);

    boolValue = false;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEccConfigurable(device,  &boolValue));
    if (res != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (boolValue == false) {
        return true;
    }
    ecc.setConfigurable(true);

    zes_device_ecc_properties_t props = {ZES_DEVICE_ECC_STATE_UNAVAILABLE, ZES_DEVICE_ECC_STATE_UNAVAILABLE, ZES_DEVICE_ACTION_NONE};
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceGetEccState(device, &props));
    if (res != ZE_RESULT_SUCCESS) {
        return false;
    }
    ecc.setCurrent(props.currentState);
    ecc.setPending(props.pendingState);
    ecc.setAction(props.pendingAction);
    return true;
#endif
}

bool GPUDeviceStub::setEccState(const zes_device_handle_t& device, ecc_state_t& newState, MemoryEcc& ecc) {
    ecc.setAvailable(false);
    ecc.setConfigurable(false);
    ecc.setCurrent(ECC_STATE_UNAVAILABLE);
    ecc.setPending(ECC_STATE_UNAVAILABLE);
    ecc.setAction(ECC_ACTION_NONE);

    if (device == nullptr) {
        return false;
    }

    //temp
    return true;
#if 0
    ze_result_t res;
    ze_bool_t boolValue = false;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEccAvailable(device,  &boolValue));
    if (res != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (boolValue == false) {
        return true;
    }
    ecc.setAvailable(true);

    boolValue = false;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEccConfigurable(device,  &boolValue));
    if (res != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (boolValue == false) {
        return true;
    }
    ecc.setConfigurable(true);

    zes_device_ecc_properties_t props = {ZES_DEVICE_ECC_STATE_UNAVAILABLE, ZES_DEVICE_ECC_STATE_UNAVAILABLE, ZES_DEVICE_ACTION_NONE};
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceSetEccState(device, newState, &props));
    if (res != ZE_RESULT_SUCCESS) {
        return false;
    }
    ecc.setCurrent(props.currentState);
    ecc.setPending(props.pendingState);
    ecc.setAction(props.pendingAction);
    return true;
#endif
}

void GPUDeviceStub::getFrequencyThrottleReason(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetFrequencyThrottleReason, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetFrequencyThrottleReason(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetFrequencyThrottleReason error: device handle is nullptr");
    }
    uint32_t freqDomainCount = 0;
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freqDomainCount, nullptr));
    if (res != ZE_RESULT_SUCCESS) {
        std::stringstream err;
        err << "zesDeviceEnumFrequencyDomains error, result: 0x" << std::hex << res;
        throw BaseException(err.str());
    }
    std::vector<zes_freq_handle_t> freqDomainList(freqDomainCount);
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freqDomainCount, freqDomainList.data()));
    if (res != ZE_RESULT_SUCCESS) {
        std::stringstream err;
        err << "zesDeviceEnumFrequencyDomains error, result: 0x" << std::hex << res;
        throw BaseException(err.str());
    }
    std::shared_ptr<MeasurementData> outData = std::make_shared<MeasurementData>();
    zes_freq_throttle_reason_flags_t deviceLevelFlag = 0;
    bool hasDataOnSubDevice = false;
    for (auto &hFreq: freqDomainList) {
        zes_freq_properties_t freqProps;
        XPUM_ZE_HANDLE_LOCK(hFreq, res = zesFrequencyGetProperties(hFreq, &freqProps));
        if (res != ZE_RESULT_SUCCESS) {
            std::stringstream err;
            err << "zesFrequencyGetProperties error, result: 0x" << std::hex << res;
            throw BaseException(err.str());
        }
        if (freqProps.type == ZES_FREQ_DOMAIN_GPU) {
            zes_freq_state_t freqState;
            XPUM_ZE_HANDLE_LOCK(hFreq, zesFrequencyGetState(hFreq, &freqState));
            if (res != ZE_RESULT_SUCCESS) {
                std::stringstream err;
                err << "zesFrequencyGetState error, result: 0x" << std::hex << res;
                throw BaseException(err.str());
            }
            if (freqProps.onSubdevice) {
                outData->setSubdeviceDataCurrent(freqProps.subdeviceId, freqState.throttleReasons);
                deviceLevelFlag |= freqState.throttleReasons;
                hasDataOnSubDevice = true;
            } else {
                outData->setCurrent(freqState.throttleReasons);
            }
        }
    }
    if (hasDataOnSubDevice) {
        outData->setCurrent(deviceLevelFlag);
    }
    return outData;
}

void GPUDeviceStub::getPCIeReadThroughput(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetPCIeReadThroughput, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPCIeReadThroughput(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetPCIeReadThroughput error");
    }

    ze_result_t res;
    zes_pci_properties_t pci_props;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    std::string bdf_address;
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetPCIeReadThroughput error");
    }
    bdf_address = to_string(pci_props.address);
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    auto value = pcie_manager.getLatestPCIeReadThroughput(bdf_address.substr(5));
    ret->setCurrent(value);
    return ret;
}

void GPUDeviceStub::getPCIeWriteThroughput(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetPCIeWriteThroughput, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPCIeWriteThroughput(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetPCIeWriteThroughput error");
    }

    ze_result_t res;
    zes_pci_properties_t pci_props;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    std::string bdf_address;
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetPCIeWriteThroughput error");
    }
    bdf_address = to_string(pci_props.address);
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    auto value = pcie_manager.getLatestPCIeWriteThroughput(bdf_address.substr(5));
    ret->setCurrent(value);
    return ret;
}

void GPUDeviceStub::getPCIeRead(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetPCIeRead, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPCIeRead(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetPCIeRead error");
    }

    ze_result_t res;
    zes_pci_properties_t pci_props;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    std::string bdf_address;
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetPCIeRead error");
    }
    bdf_address = to_string(pci_props.address);
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    auto value = pcie_manager.getLatestPCIeRead(bdf_address.substr(5));
    ret->setCurrent(value);
    return ret;
}

void GPUDeviceStub::getPCIeWrite(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetPCIeWrite, device);
}

std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPCIeWrite(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetPCIeWrite error");
    }

    ze_result_t res;
    zes_pci_properties_t pci_props;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
    std::string bdf_address;
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetPCIeWrite error");
    }
    bdf_address = to_string(pci_props.address);
    std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
    auto value = pcie_manager.getLatestPCIeWrite(bdf_address.substr(5));
    ret->setCurrent(value);
    return ret;
}

void GPUDeviceStub::getFabricThroughput(const zes_device_handle_t& device, Callback_t callback) noexcept {
    if (device == nullptr) {
        return;
    }
    invokeTask(callback, toGetFabricThroughput, device);
}

std::shared_ptr<FabricMeasurementData> GPUDeviceStub::toGetFabricThroughput(const zes_device_handle_t& device) {
    if (device == nullptr) {
        throw BaseException("toGetFabricThroughput error");
    }
    std::map<std::string, ze_result_t> exception_msgs;
    bool data_acquired = false;
    uint32_t fabric_port_count = 0;
    std::shared_ptr<FabricMeasurementData> ret = std::make_shared<FabricMeasurementData>();
    ze_result_t res;
    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &fabric_port_count, nullptr));
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_fabric_port_handle_t> fabric_ports(fabric_port_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFabricPorts(device, &fabric_port_count, fabric_ports.data()));
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& fp : fabric_ports) {
                zes_fabric_port_properties_t props;
                XPUM_ZE_HANDLE_LOCK(device, res = zesFabricPortGetProperties(fp, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    zes_fabric_port_state_t state;
                    XPUM_ZE_HANDLE_LOCK(device, res = zesFabricPortGetState(fp, &state));
                    if (res == ZE_RESULT_SUCCESS) {
                        zes_fabric_port_throughput_t throughput;
                        XPUM_ZE_HANDLE_LOCK(device, res = zesFabricPortGetThroughput(fp, &throughput));
                        if (res == ZE_RESULT_SUCCESS) {
                            ret->addRawData(uint64_t(fp), throughput.timestamp, throughput.rxCounter, throughput.txCounter, props.portId.attachId, state.remotePortId.fabricId, state.remotePortId.attachId);
                            data_acquired = true;
                        } else {
                            exception_msgs["zesFabricPortGetThroughput"] = res;
                        }
                    } else {
                        exception_msgs["zesFabricPortGetState"] = res;
                    }
                } else {
                    exception_msgs["zesFabricPortGetProperties"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumFabricPorts"] = res;
        }
    } else {
        exception_msgs["zesDeviceEnumFabricPorts"] = res;
    }

    if (data_acquired) {
        ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    } else {
        if (fabric_port_count == 0 && exception_msgs.empty())
            throw BaseException("fabric port not found");
        else
            throw BaseException(buildErrors(exception_msgs, __func__, __LINE__));
    }
}

void GPUDeviceStub::getPerfMetrics(ze_device_handle_t& device, ze_driver_handle_t& driver, 
                                   Callback_t callback) noexcept {
    invokeTask(callback, toGetPerfMetrics, device, driver);
}

std::shared_ptr<PerfMeasurementData> GPUDeviceStub::toGetPerfMetrics(ze_device_handle_t& device, 
                                                                     ze_driver_handle_t& driver) {  
    uint32_t sub_device_count = MAX_SUB_DEVICE;
    ze_device_handle_t sub_device_handles[MAX_SUB_DEVICE];
    
    ze_result_t res = zeDeviceGetSubDevices(device, &sub_device_count, sub_device_handles);
    if (res != ZE_RESULT_SUCCESS) {
        throw BaseException("toGetPerfMetrics");
    }

    std::vector<ze_device_handle_t> target_devices;
    if (sub_device_count == 0) {
        target_devices.push_back(device);
    }
    for (uint32_t i = 0; i < sub_device_count; ++i) {
        target_devices.push_back(sub_device_handles[i]);
    }

    std::unique_lock<std::mutex> lock(GPUDeviceStub::metric_streamer_mutex);    

    std::map<ze_device_handle_t, std::shared_ptr<std::map<uint32_t, std::shared_ptr<DeviceMetricGroups_t>>>> to_active_groups;
    std::map<ze_device_handle_t, std::shared_ptr<std::vector<std::shared_ptr<DeviceMetricGroups_t>>>> remaining_groups;
    std::map<ze_device_handle_t, std::shared_ptr<PerfMetricDeviceData_t>> device_datas;
    std::map<ze_device_handle_t, ze_context_handle_t> device_contexts;
    
    for (auto device : target_devices) {
        auto p_groups = getDevicePerfMetricGroups(device, driver);
        if (p_groups->size() > 0) {
            auto p_metric_groups = std::make_shared<std::vector<std::shared_ptr<DeviceMetricGroups_t>>>();
            for (auto it = p_groups->begin(); it != p_groups->end(); it++) {
                p_metric_groups->push_back(*it);
            }
            remaining_groups[device] = p_metric_groups;
        }
    }

    while (true) {
        if (remaining_groups.size() == 0) {
            break;
        }

        for (auto it = remaining_groups.begin(); it != remaining_groups.end();) {
            std::shared_ptr<std::map<uint32_t, std::shared_ptr<DeviceMetricGroups_t>>> p_device_groups;
            if (to_active_groups.find(it->first) == to_active_groups.end()) {
                p_device_groups = std::make_shared<std::map<uint32_t, std::shared_ptr<DeviceMetricGroups_t>>>();
                to_active_groups[it->first] = p_device_groups;
            } else {
                p_device_groups = to_active_groups.at(it->first);
            }

            for (auto it_group = it->second->begin(); it_group != it->second->end(); ) {
                if (p_device_groups->find((*it_group)->domain) == p_device_groups->end()) {
                    p_device_groups->insert(std::pair<uint32_t, std::shared_ptr<DeviceMetricGroups_t>>((*it_group)->domain, *it_group));
                    it_group = it->second->erase(it_group);
                } else {
                    it_group++;
                }
            }

            if (it->second->size() == 0) {
                it = remaining_groups.erase(it);
            } else {
                it++;
            }
        }

        for (auto it = to_active_groups.begin(); it != to_active_groups.end(); it++) {
            openDevicePerfMetricStream(device, driver, it->second, device_contexts);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(
            Configuration::EU_ACTIVE_STALL_IDLE_MONITOR_INTERNAL_PERIOD));        
                
        for (auto it = to_active_groups.begin(); it != to_active_groups.end(); it++) {
            std::shared_ptr<PerfMetricDeviceData_t> p_existing_device_data;
            if (device_datas.find(it->first) != device_datas.end()) {
                p_existing_device_data = device_datas[it->first];
            } else {
                p_existing_device_data = std::make_shared<PerfMetricDeviceData_t>();
                device_datas[it->first] = p_existing_device_data;
            }

            readPerfMetricsData(it->second, p_existing_device_data);

            for (auto it_group = it->second->begin(); it_group != it->second->end(); it_group++) {
                zetMetricStreamerClose(it_group->second->streamer);
            }

            zetContextActivateMetricGroups(device_contexts[it->first], it->first, 0, nullptr);
        }

        to_active_groups.clear();
    }
    
    std::shared_ptr<PerfMeasurementData> p_measurement_data = std::make_shared<PerfMeasurementData>();
    for (auto device : target_devices) {
        if (device_datas.find(device) != device_datas.end()) {
            p_measurement_data->addData(device_datas.at(device));
        }
    }

    return p_measurement_data;
}

std::shared_ptr<std::vector<std::shared_ptr<DeviceMetricGroups_t>>> GPUDeviceStub::getDevicePerfMetricGroups(
    ze_device_handle_t& device, ze_driver_handle_t& driver) {
    if (device_perf_groups.find(device) == device_perf_groups.end()) {
        uint32_t metric_group_count = 0;
        ze_result_t res = zetMetricGroupGet(device, &metric_group_count, nullptr);
        if (res != ZE_RESULT_SUCCESS)  {
            throw BaseException("getDevicePerfMetricGroups");
        }

        std::vector<zet_metric_group_handle_t> metric_groups(metric_group_count);
        res = zetMetricGroupGet(device, &metric_group_count, metric_groups.data());
        if (res != ZE_RESULT_SUCCESS)  {
            throw BaseException("getDevicePerfMetricGroups");
        }

        std::map<std::string, std::shared_ptr<DeviceMetricGroups_t>> target_metric_groups;
        for (auto& conf : Configuration::getPerfMetrics()) {
            std::regex regex = std::regex(conf.group);
            std::regex name_regex = std::regex(conf.name);
            for (uint32_t i = 0; i < metric_group_count; i++) {
                zet_metric_group_properties_t metric_group_prop;
                res = zetMetricGroupGetProperties(metric_groups[i], &metric_group_prop);
                if (res != ZE_RESULT_SUCCESS) {
                    throw BaseException("getDevicePerfMetricGroups");
                }

                if (std::regex_match(metric_group_prop.name, regex)) {
                    std::shared_ptr<DeviceMetricGroups_t> p_metric_group;
                    if (target_metric_groups.find(metric_group_prop.name) != target_metric_groups.end()) {
                        p_metric_group = target_metric_groups.at(metric_group_prop.name);
                    } else {
                        p_metric_group = std::make_shared<DeviceMetricGroups_t>();
                        p_metric_group->group_name = metric_group_prop.name;
                        p_metric_group->domain = metric_group_prop.domain;
                        p_metric_group->metric_count = metric_group_prop.metricCount;
                        p_metric_group->metric_group = metric_groups[i];
                        target_metric_groups[metric_group_prop.name] = p_metric_group;
                    }

                    uint32_t metric_count = p_metric_group->metric_count;
                    std::vector<zet_metric_handle_t> metrics(metric_count);
                    res = zetMetricGet(p_metric_group->metric_group, &metric_count, metrics.data());
                    if (res != ZE_RESULT_SUCCESS) {
                        throw BaseException("zetMetricGet");
                    }
                    for (uint32_t j = 0; j < metric_count; ++j) {
                        zet_metric_properties_t metric_prop;
                        res = zetMetricGetProperties(metrics[j], &metric_prop);
                        if (res != ZE_RESULT_SUCCESS) {
                            throw BaseException("zetMetricGetProperties");
                        }

                        bool is_gpu_time = std::strcmp(metric_prop.name, GPU_TIME_NAME) == 0;
                        if (((std::regex_match(metric_prop.name, name_regex) && conf.type == "time") || is_gpu_time) && 
                            (p_metric_group->target_metrics.find(metric_prop.name) == p_metric_group->target_metrics.end())) {
                            auto p_metric_data = std::make_shared<PerfMetricData_t>();
                            p_metric_data->name = metric_prop.name;
                            p_metric_data->type = is_gpu_time ? "" : conf.type;
                            p_metric_data->index = j;
                            p_metric_data->current = 0;
                            p_metric_data->average = 0;
                            p_metric_group->target_metrics[metric_prop.name] = p_metric_data;
                        }
                    }
                }
            }
        }

        if (target_metric_groups.size() == 0) {
            XPUM_LOG_WARN("Device has metric group {} but no matched performance metrics", 
                           metric_group_count);
        }

        auto p_device_groups = std::make_shared<std::vector<std::shared_ptr<DeviceMetricGroups_t>>>();
        for (auto it = target_metric_groups.begin(); it !=  target_metric_groups.end(); it++) {
            if (it->second->target_metrics.size() == 0) {
                continue;
            } else if (it->second->target_metrics.size() == 1 && 
                it->second->target_metrics.find(GPU_TIME_NAME) != it->second->target_metrics.end()) {
                continue;
            }
            p_device_groups->push_back(it->second);
        }

        XPUM_LOG_WARN("Total metric group count: {}, matched metric group count: {}", metric_group_count, p_device_groups->size());

        GPUDeviceStub::device_perf_groups[device] = p_device_groups;
        return p_device_groups;
    } else {
        return GPUDeviceStub::device_perf_groups.at(device);
    }
}


void GPUDeviceStub::openDevicePerfMetricStream(ze_device_handle_t& device,
                                              ze_driver_handle_t& driver, 
                                              std::shared_ptr<std::map<uint32_t, std::shared_ptr<DeviceMetricGroups_t>>>& p_target_groups,
                                              std::map<ze_device_handle_t, ze_context_handle_t>& device_contexts) {
    
    ze_context_handle_t ze_context;
    if (device_contexts.find(device) != device_contexts.end()) {
        ze_context = device_contexts.at(device);
    } else {
        ze_context_desc_t context_desc = {
                ZE_STRUCTURE_TYPE_CONTEXT_DESC,
                nullptr, 
                0
        };     
        auto res = zeContextCreate(driver, &context_desc, &ze_context);
        if (res != ZE_RESULT_SUCCESS)  {
            throw BaseException("openDevicePerfMetricStream");
        }
        device_contexts[device] = ze_context;
    }

    std::vector<zet_metric_group_handle_t> to_active_groups;
    for (auto it = p_target_groups->begin(); it != p_target_groups->end(); it++) {
        to_active_groups.push_back(it->second->metric_group);
    }

    auto res = zetContextActivateMetricGroups(ze_context, device, 
                                            to_active_groups.size(), 
                                            to_active_groups.data());
    if (res != ZE_RESULT_SUCCESS)  {
        XPUM_LOG_WARN("Failed to zetContextActivateMetricGroups {} with {}", 
                      to_active_groups.size(), res);        
        throw BaseException("openDevicePerfMetricStream");
    }

    zet_metric_streamer_desc_t streamer_desc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC};
    streamer_desc.samplingPeriod = Configuration::EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD;
    for (auto it = p_target_groups->begin(); it != p_target_groups->end(); it++) {
        res = zetMetricStreamerOpen(ze_context, device, it->second->metric_group, &streamer_desc, 
                                    nullptr, &it->second->streamer);
        if (res != ZE_RESULT_SUCCESS)  {
            XPUM_LOG_WARN("Failed to zetMetricStreamerOpen {} with {}", 
                          to_active_groups.size(), res);            
            throw BaseException("openDevicePerfMetricStream");
        }
    }
}

void GPUDeviceStub::readPerfMetricsData(std::shared_ptr<std::map<uint32_t, std::shared_ptr<DeviceMetricGroups_t>>>& p_groups,
                                        std::shared_ptr<PerfMetricDeviceData_t> &p_metric_device_data) {
    for (auto it = p_groups->begin(); it != p_groups->end(); it++) {
        size_t raw_size = 0;
        ze_result_t res = zetMetricStreamerReadData(it->second->streamer, UINT32_MAX, &raw_size, nullptr);
        if (res != ZE_RESULT_SUCCESS) {
            throw BaseException("getPerfMetricsData");
        }

        std::vector<uint8_t> raw_data(raw_size);
        res = zetMetricStreamerReadData(it->second->streamer, UINT32_MAX, &raw_size, raw_data.data());
        if (res != ZE_RESULT_SUCCESS) {
            throw BaseException("getPerfMetricsData");
        }

        uint32_t value_count = 0;
        zet_metric_group_calculation_type_t calc_type = ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES;
        res = zetMetricGroupCalculateMetricValues(it->second->metric_group, calc_type, raw_size, 
                                                  raw_data.data(), &value_count, nullptr);
        if (res != ZE_RESULT_SUCCESS) {
            throw BaseException("getPerfMetricsData");
        }

        std::vector<zet_typed_value_t> values(value_count);
        res = zetMetricGroupCalculateMetricValues(it->second->metric_group, calc_type, raw_size, 
                                                  raw_data.data(), &value_count, values.data());
        if (res != ZE_RESULT_SUCCESS) {
            throw BaseException("getPerfMetricsData");
        }        
        
        uint32_t report_count = value_count / it->second->metric_count;
        uint64_t total_elapsed_time = 0;
        PerfMetricGroupData_t metric_group_data;

        for (uint32_t report = 0; report < report_count; ++report) {
            uint64_t current_elapsed_time = 0;
            for (uint32_t metric = 0; metric < it->second->metric_count; metric++) {
                zet_typed_value_t data = values[report * it->second->metric_count + metric];
                for (auto it_metric = it->second->target_metrics.begin(); 
                     it_metric != it->second->target_metrics.end(); it_metric++) {
                     if (it_metric->second->index == metric) {
                        bool found = false;
                        for (auto& m : metric_group_data.data) {
                            if (m.name == it_metric->second->name) {
                                if (m.name == GPU_TIME_NAME) {
                                    current_elapsed_time = data.value.ui64;
                                    m.current = data.value.ui64;
                                } else {
                                    m.current = data.value.fp32;
                                }
                                found = true;
                                break;
                            }
                        }

                        if (!found) {
                            PerfMetricData_t perf_metric_data;
                            perf_metric_data.name = it_metric->second->name; 
                            perf_metric_data.average = 0;
                            perf_metric_data.total = 0;
                            perf_metric_data.type = it_metric->second->type;

                            if (it_metric->second->name == GPU_TIME_NAME) {
                                perf_metric_data.current = data.value.ui64;
                                current_elapsed_time = data.value.ui64;
                            } else {
                                perf_metric_data.current = data.value.fp32;
                            }

                            metric_group_data.data.emplace_back(perf_metric_data);
                        }
                        break;
                     }
                }            
            }

            for (auto& m : metric_group_data.data) {
                m.total += m.type == "time" ? current_elapsed_time * m.current : m.current;
            }

            total_elapsed_time += current_elapsed_time;
        }

        for (auto& m : metric_group_data.data) {
            if (total_elapsed_time != 0) {
                m.average = m.total / (double)total_elapsed_time;
            }
        }

        metric_group_data.name = it->second->group_name;
        p_metric_device_data->data.emplace_back(metric_group_data);
    }
}

} // end namespace xpum
