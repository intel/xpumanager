/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.cpp
 */

#include "core_stub.h"

#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
#include <fstream>
#include "ze_log.h"
#include <loader/ze_loader.h>
#include <cstdlib>  
#include "xpum_structs.h"
#include "win_native.h"
using namespace xpum;


CoreStub::CoreStub() {
    initPDHQuery();
    ze_result_t status = zeInit(0);
    if (status != ZE_RESULT_SUCCESS) {
        std::cout << "Driver not initialized: " << to_string(status) << std::endl;
        exit(-1);
    }

    const ze_device_type_t type = ZE_DEVICE_TYPE_GPU;
    uint32_t driverCount = 0;
    status = zeDriverGet(&driverCount, nullptr);
    if (status != ZE_RESULT_SUCCESS) {
        std::cout << "zeDriverGet Failed with return code: " << to_string(status) << std::endl;
        exit(-1);
    }

    std::vector<ze_driver_handle_t> drivers(driverCount);
    status = zeDriverGet(&driverCount, drivers.data());
    if (status != ZE_RESULT_SUCCESS) {
        std::cout << "zeDriverGet Failed with return code: " << to_string(status) << std::endl;
        exit(-1);
    }

    if (driverCount == 0) {
        std::cout << "No driver found" << std::endl;
        exit(-1);
    }

    ze_driver_handle = drivers[0];
    ze_driver_properties_t driver_prop;
    status = zeDriverGetProperties(ze_driver_handle, &driver_prop); 
    if (status != ZE_RESULT_SUCCESS) {
        std::cout << "zeDriverGetProperties Failed with return code: " << to_string(status) << std::endl;
        exit(-1);
    }
    driver_version = std::to_string(driver_prop.driverVersion);

    uint32_t deviceCount = 0;
    status = zeDeviceGet(ze_driver_handle, &deviceCount, nullptr);
    if (status != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGet Failed with return code: " << to_string(status) << std::endl;
        exit(-1);
    }

    std::vector<ze_device_handle_t> devices(deviceCount);
    status = zeDeviceGet(ze_driver_handle, &deviceCount, devices.data());
    if (status != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGet Failed with return code: " << to_string(status) << std::endl;
        exit(-1);
    }

    for (int deviceIdx = 0; deviceIdx < deviceCount; deviceIdx++) {
        ze_device_handles.push_back(devices[deviceIdx]);
        zes_device_handles.push_back((zes_device_handle_t)devices[deviceIdx]);

        ze_device_properties_t ze_device_properties;
        ze_device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        ze_device_properties.pNext = nullptr;
        status = zeDeviceGetProperties(devices[deviceIdx], &ze_device_properties);
        if (status != ZE_RESULT_SUCCESS) {
            std::cout << "zeDeviceGetProperties Failed with return code: " << to_string(status) << std::endl;
            exit(-1);
        }
        uint32_t deviceId = ze_device_properties.deviceId;
        zes_pci_properties_t pci_props;
        status = zesDevicePciGetProperties(zes_device_handles[deviceIdx], &pci_props);
        if (status != ZE_RESULT_SUCCESS) {
            std::cout << "zesDevicePciGetProperties Failed with return code: " << to_string(status) << std::endl;
            exit(-1);
        }

        if (deviceIdx > 0 && deviceId == 0x56c1) {
            ze_device_properties_t ze_pre_device_properties;
            ze_pre_device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            ze_pre_device_properties.pNext = nullptr;
            status = zeDeviceGetProperties(devices[deviceIdx - 1], &ze_pre_device_properties);
            if (status != ZE_RESULT_SUCCESS) {
                std::cout << "zeDeviceGetProperties Failed with return code: " << to_string(status) << std::endl;
                exit(-1);
            }
            uint32_t preDeviceId = ze_pre_device_properties.deviceId;
            zes_pci_properties_t pre_pci_props;
            status = zesDevicePciGetProperties(zes_device_handles[deviceIdx - 1], &pre_pci_props);
            if (status != ZE_RESULT_SUCCESS) {
                std::cout << "zesDevicePciGetProperties Failed with return code: " << to_string(status) << std::endl;
                exit(-1);
            }

            if (preDeviceId == 0x56c1 && std::abs((int)(pci_props.address.bus - pre_pci_props.address.bus)) <= 5) {
                sibling_devices[deviceIdx - 1] = {deviceIdx - 1, deviceIdx};
                sibling_devices[deviceIdx] = {deviceIdx - 1, deviceIdx};
            }
            power_limit = 23;
        } else if (deviceId == 0x56c0) {
            power_limit = 120;
        } else if (deviceId == 0x4905) {
            power_limit = 25;
        }
    }

    std::ifstream conf_file("xpum.conf");
    if (conf_file.is_open()) {
        std::string line;
        while (getline(conf_file, line)) {
            if (line.find("memory_sampling_interval:") != std::string::npos) {
                memory_sampling_interval = std::stoi(line.substr(line.find(':') + 1));
            }
        }
    }
}

CoreStub::~CoreStub() {
    /*
    std::ofstream conf_file("xpum.conf");
    conf_file << "memory_sampling_interval:" << memory_sampling_interval << "\n";
    conf_file.close();
    */
    void closePDHQuery();
}

std::unique_ptr<nlohmann::json> CoreStub::getVersion() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());

    // (*json)["xpum_version"] = "1.1";
    // (*json)["xpum_version_git"] = "1.1";
    (*json)["level_zero_version"] = "Not Detected";
    ze_result_t res;
    size_t size = 0;
    res = zelLoaderGetVersions(&size, nullptr);
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zel_component_version_t> versions(size);
        res = zelLoaderGetVersions(&size, versions.data());
        if (res == ZE_RESULT_SUCCESS) {
            if (versions.size() > 0) {
                std::string str = std::to_string(versions[0].component_lib_version.major)
                    + "." + std::to_string(versions[0].component_lib_version.minor)
                    + "." + std::to_string(versions[0].component_lib_version.patch);
                (*json)["level_zero_version"] = str;
            }
           
        }
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getDeviceList() {

    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    std::vector<nlohmann::json> deviceJsonList;
    ze_result_t res;
    int id = 0;
    for (auto zes_device : ze_device_handles) {
        zes_device_properties_t zes_device_properties;
        zes_device_properties.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        res = zesDeviceGetProperties(zes_device, &zes_device_properties);
        if (res != ZE_RESULT_SUCCESS) {
            std::cout << "zesDeviceGetProperties Failed with return code: " << to_string(res) << std::endl;
            exit(-1);
        }
        auto deviceJson = nlohmann::json();
        deviceJson["device_id"] = id++;
        deviceJson["device_type"] = "GPU";
        auto& uuidBuf = zes_device_properties.core.uuid.id;
        deviceJson["uuid"] = getUUID(uuidBuf);
        deviceJson["device_name"] = zes_device_properties.core.name;
        std::stringstream ss;
        ss << std::hex << zes_device_properties.core.deviceId;
        deviceJson["pci_device_id"] = ss.str();
        deviceJson["pci_bdf_address"] = getBdfAddress(zes_device);
        std::string vendor_name = std::string(zes_device_properties.vendorName);
        if (vendor_name.empty())
               vendor_name = "Intel(R) Corporation";
        deviceJson["vendor_name"] = vendor_name;
        deviceJsonList.push_back(deviceJson);
    }

    (*json)["device_list"] = deviceJsonList;

    return json;
}

std::string CoreStub::getUUID(uint8_t(&uuidBuf)[16]) {
    char uuidStr[37] = {};
    sprintf_s(uuidStr,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuidBuf[15], uuidBuf[14], uuidBuf[13], uuidBuf[12], uuidBuf[11], uuidBuf[10], uuidBuf[9], uuidBuf[8],
        uuidBuf[7], uuidBuf[6], uuidBuf[5], uuidBuf[4], uuidBuf[3], uuidBuf[2], uuidBuf[1], uuidBuf[0]);
    std::string uuid = std::string(uuidStr);
    return uuid;
}

std::string CoreStub::getBdfAddress(const zes_device_handle_t& zes_device) {
    zes_pci_properties_t pci_props;
    pci_props.stype = ZES_STRUCTURE_TYPE_PCI_PROPERTIES;
    ze_result_t res = zesDevicePciGetProperties(zes_device, &pci_props);
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zesDevicePciGetProperties Failed with return code: " << to_string(res) << std::endl;
    }
    std::ostringstream os;
    os << std::setfill('0') << std::setw(4) << std::hex
        << pci_props.address.domain << std::string(":")
        << std::setw(2)
        << pci_props.address.bus << std::string(":")
        << std::setw(2)
        << pci_props.address.device << std::string(".")
        << pci_props.address.function;
    return os.str();
}

std::unique_ptr<nlohmann::json> CoreStub::getDeviceProperties(int deviceId) {

    auto deviceJson = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceId < 0 || deviceId >= ze_device_handles.size()) {
        (*deviceJson)["error"] = "invalid device id";
        return deviceJson;
    }
    zes_device_handle_t zes_device = zes_device_handles[deviceId];
    ze_result_t res;
    zes_device_properties_t zes_device_properties;
    zes_device_properties.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
    res = zesDeviceGetProperties(zes_device, &zes_device_properties);
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zesDeviceGetProperties Failed with return code: " << to_string(res) << std::endl;
        exit(-1);
    }
    (*deviceJson)["device_id"] = deviceId;
    (*deviceJson)["device_type"] = "GPU";
    (*deviceJson)["device_name"] = zes_device_properties.core.name;
    std::string vendor_name = std::string(zes_device_properties.vendorName);
    if (vendor_name.empty())
        vendor_name = "Intel(R) Corporation";
    (*deviceJson)["vendor_name"] = vendor_name;
    auto& uuidBuf = zes_device_properties.core.uuid.id;
    (*deviceJson)["uuid"] = getUUID(uuidBuf);
    (*deviceJson)["serial_number"] = std::string(zes_device_properties.serialNumber);
    (*deviceJson)["core_clock_rate_mhz"] = zes_device_properties.core.coreClockRate;
    (*deviceJson)["device_stepping"] = "unknown";
    (*deviceJson)["driver_version"] = driver_version;
    std::stringstream ss;
    ss << "0x" << std::hex << zes_device_properties.core.deviceId;
    (*deviceJson)["pci_device_id"] = ss.str();
    (*deviceJson)["pci_bdf_address"] = getBdfAddress(zes_device);
    (*deviceJson)["gfx_firmware_name"] = "GFX";
    (*deviceJson)["gfx_firmware_version"] = igsc_instance.getDeviceGSCVersion((*deviceJson)["pci_bdf_address"]);
    (*deviceJson)["gfx_data_firmware_name"] = "GFX_DATA";
    (*deviceJson)["gfx_data_firmware_version"] = igsc_instance.getDeviceGSCDataVersion((*deviceJson)["pci_bdf_address"]);
    zes_pci_properties_t pci_props;
    pci_props.stype = ZES_STRUCTURE_TYPE_PCI_PROPERTIES;
    res = zesDevicePciGetProperties(zes_device, &pci_props);
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zesDevicePciGetProperties Failed with return code: " << to_string(res) << std::endl;
        exit(-1);
    }
    if (pci_props.maxSpeed.gen > 0)
        (*deviceJson)["pcie_generation"] = pci_props.maxSpeed.gen;
    if (pci_props.maxSpeed.width > 0)
        (*deviceJson)["pcie_max_link_width"] = pci_props.maxSpeed.width;
    uint64_t physical_size = 0;
    uint32_t mem_module_count = 0;
    res = zesDeviceEnumMemoryModules(zes_device, &mem_module_count, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zesDeviceEnumMemoryModules Failed with return code: " << to_string(res) << std::endl;
        exit(-1);
    }
    std::vector<zes_mem_handle_t> mems(mem_module_count);
    res = zesDeviceEnumMemoryModules(zes_device, &mem_module_count, mems.data());
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zesDeviceEnumMemoryModules Failed with return code: " << to_string(res) << std::endl;
        exit(-1);
    }
    if (mems.size() > 0) {
        for (auto& mem : mems) {
            uint64_t mem_module_physical_size = 0;
            zes_mem_properties_t props;
            props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
            res = zesMemoryGetProperties(mem, &props);
            if (res == ZE_RESULT_SUCCESS) {
                mem_module_physical_size = props.physicalSize;
                int32_t mem_bus_width = props.busWidth;
                int32_t mem_channel_num = props.numChannels;
                (*deviceJson)["memory_bus_width"] = std::to_string(mem_bus_width);
                (*deviceJson)["number_of_memory_channels"] = std::to_string(mem_channel_num);
            }

            zes_mem_state_t sysman_memory_state = {};
            sysman_memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
            res = zesMemoryGetState(mem, &sysman_memory_state);
            if (res == ZE_RESULT_SUCCESS) {
                if (props.physicalSize == 0) {
                    mem_module_physical_size = sysman_memory_state.size;
                }
                physical_size += mem_module_physical_size;
            }
        }
    } else {
        physical_size = (uint64_t)getMemSizeByNativeAPI();
    }
    (*deviceJson)["memory_physical_size_byte"] = std::to_string(physical_size);

    (*deviceJson)["max_mem_alloc_size_byte"] = zes_device_properties.core.maxMemAllocSize;
    (*deviceJson)["max_hardware_contexts"] = zes_device_properties.core.maxHardwareContexts;

    (*deviceJson)["number_of_eus"] = std::max((int)zes_device_properties.numSubdevices, 1) *
        zes_device_properties.core.numSlices * zes_device_properties.core.numSubslicesPerSlice *
        zes_device_properties.core.numEUsPerSubslice;
    (*deviceJson)["number_of_tiles"] = std::max((int)zes_device_properties.numSubdevices, 1);
    (*deviceJson)["number_of_slices"] = zes_device_properties.core.numSlices;
    (*deviceJson)["number_of_sub_slices_per_slice"] = zes_device_properties.core.numSubslicesPerSlice;
    (*deviceJson)["number_of_eus_per_sub_slice"] = zes_device_properties.core.numEUsPerSubslice;
    (*deviceJson)["number_of_threads_per_eu"] = zes_device_properties.core.numThreadsPerEU;
    (*deviceJson)["physical_eu_simd_width"] = zes_device_properties.core.physicalEUSimdWidth;
    uint32_t engine_grp_count;
    uint32_t media_engine_count = 0;
    uint32_t meida_enhancement_engine_count = 0;
    res = zesDeviceEnumEngineGroups(zes_device, &engine_grp_count, nullptr);
    if (res == ZE_RESULT_SUCCESS) {
        std::vector<zes_engine_handle_t> engines(engine_grp_count);
        res = zesDeviceEnumEngineGroups(zes_device, &engine_grp_count, engines.data());
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& engine : engines) {
                zes_engine_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                res = zesEngineGetProperties(engine, &props);
                if (res == ZE_RESULT_SUCCESS) {
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
    if (media_engine_count > 0)
        (*deviceJson)["number_of_media_engines"] = media_engine_count;
    if (meida_enhancement_engine_count > 0)
        (*deviceJson)["number_of_media_enh_engines"] = meida_enhancement_engine_count;
    return deviceJson;
}

static std::string eccStateToString(uint8_t state) {
    if (state == 1) {
        return "enabled";
    } else if (state == 0) {
        return "disabled";
    } else {
        return "unavailable";
    }
}

std::unique_ptr<nlohmann::json> CoreStub::getDeviceConfig(int deviceId, int tileId) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceId < 0 || deviceId >= ze_device_handles.size()) {
        (*json)["error"] = "invalid device id";
        return json;
    }
    (*json)["device_id"] = deviceId;
    std::vector<nlohmann::json> tileJsonList;
    ze_result_t res;
    uint32_t sub_device_count = 0;
    res = zeDeviceGetSubDevices(ze_device_handles[deviceId], &sub_device_count, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGetSubDevices Failed with return code: " << to_string(res) << std::endl;
    }
    std::vector<ze_device_handle_t> sub_device_handles(sub_device_count);
    res = zeDeviceGetSubDevices(ze_device_handles[deviceId], &sub_device_count, sub_device_handles.data());
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGetSubDevices Failed with return code: " << to_string(res) << std::endl;
    }
    if (sub_device_count > 0 && tileId >= (int)sub_device_count) {
        (*json)["error"] = "invalid tile id";
        return json;
    }
    if (sub_device_count == 0) {
        if (tileId != 0 && tileId != -1) {
            (*json)["error"] = "invalid tile id";
            return json;
        }
        sub_device_handles.push_back(ze_device_handles[deviceId]);
    }
    for (int i = 0; i < sub_device_handles.size(); ++i) {
        auto tileJson = nlohmann::json();
        tileJson["tile_id"] = i;
        bool freq_supported = true;
        auto freq_datas = handleFreqByLevel0((zes_device_handle_t)sub_device_handles[i], false, 0, 0, freq_supported);
        if (!freq_supported) {
            (*json)["error"] = "unsupported feature or insufficient privilege";
            return json;
        }
        tileJson["min_frequency"] = freq_datas[0];
        tileJson["max_frequency"] = freq_datas[1];
        tileJson["gpu_frequency_valid_options"] = freq_datas[2];
        tileJson["tile_id"] = std::to_string(deviceId) + "/" + std::to_string(i);
        tileJsonList.push_back(tileJson); 
    }
    (*json)["tile_config_data"] = tileJsonList;
    bool power_supported = true;
    auto power_datas = handlePowerByLevel0(zes_device_handles[deviceId], false, 0, 0, power_supported);
    if (!power_supported) {
        (*json)["error"] = "unsupported feature or insufficient privilege";
        return json;
    }
    (*json)["power_limit"] = power_datas[0];
    (*json)["power_vaild_range"] = "1 to " + std::to_string(power_limit);
    uint8_t cur = 0xFF;
    uint8_t pen = 0xFF;
    std::string bdf = getBdfAddress(zes_device_handles[deviceId]);
    if (igsc_instance.getDeviceEccState(bdf, &cur, &pen)) {
        (*json)["memory_ecc_current_state"] = eccStateToString(cur);
        (*json)["memory_ecc_pending_state"] = eccStateToString(pen);
    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::setDevicePowerlimit(int deviceId, int tileId, int power, int interval)
{
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceId < 0 || deviceId >= ze_device_handles.size()) {
        (*json)["error"] = "invalid device id";
        return json;
    }
    ze_result_t res;
    uint32_t sub_device_count = 0;
    res = zeDeviceGetSubDevices(ze_device_handles[deviceId], &sub_device_count, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGetSubDevices Failed with return code: " << to_string(res) << std::endl;
    }
    std::vector<ze_device_handle_t> sub_device_handles(sub_device_count);
    res = zeDeviceGetSubDevices(ze_device_handles[deviceId], &sub_device_count, sub_device_handles.data());
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGetSubDevices Failed with return code: " << to_string(res) << std::endl;
    }
    if (sub_device_count > 0 && tileId >= (int)sub_device_count) {
        (*json)["error"] = "invalid tile id";
        return json;
    }
    if (sub_device_count == 0) {
        if (tileId == -1) {
            tileId = 0;
        } else if (tileId != 0) {
            (*json)["error"] = "invalid tile id";
            return json;
        }
        sub_device_handles.push_back(ze_device_handles[deviceId]);
    }
    bool supported = true;
    auto power_datas = handlePowerByLevel0((zes_device_handle_t)sub_device_handles[tileId], true, power * 1000, interval, supported);
    if (supported) {
        if (power_datas.size() > 2 && power_datas[2] == -1) {
            (*json)["error"] = "Invalid power limit value";
        }
        else {
            (*json)["status"] = "OK";
        }
    }
    else
        (*json)["error"] = "unsupported feature";
    return json;
}

std::vector<int> CoreStub::handlePowerByLevel0(zes_device_handle_t device, bool set, int limit, int interval, bool& supported) {
    std::vector<int> res;
    uint32_t power_domain_count = 0;
    ze_result_t status;
    status = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
    if (status != ZE_RESULT_SUCCESS) {
        std::cout << "zesDeviceEnumPowerDomains Failed with return code: " << to_string(status) << std::endl;
    }
    else {
        if (power_domain_count == 0) {
            supported = false;
            std::cout << "zesDeviceEnumPowerDomains Failed with zero power domain " << std::endl;
        }
        std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
        status = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
        if (status == ZE_RESULT_SUCCESS) {
            for (auto& power : power_handles) {
                zes_power_properties_t props;
                props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
                props.pNext = nullptr;
                status = zesPowerGetProperties(power, &props);
                if (status == ZE_RESULT_SUCCESS) {
                    zes_power_sustained_limit_t sustained;
                    status = zesPowerGetLimits(power, &sustained, nullptr, nullptr);
                    if (status == ZE_RESULT_SUCCESS) {
                        res.push_back(sustained.power/1000);
                        res.push_back(sustained.interval);
                    }
                    else {
                        supported = false;
                        std::cout << "zesPowerGetProperties Failed with return code: " << to_string(status) << std::endl;
                    }
                }

                if (set) {
                    if (limit < 1) {
                        res.push_back(-1);
                        return res;
                    }
                    zes_power_sustained_limit_t sustained;
                    sustained.enabled = true;
                    sustained.power = limit;
                    sustained.interval = interval;
                    status = zesPowerSetLimits(power, &sustained, nullptr, nullptr);
                    if (status != ZE_RESULT_SUCCESS) {
                        supported = false;
                        std::cout << "zesPowerSetLimits Failed with return code: " << to_string(status) << std::endl;
                    }
                }
            }
        }
    }
    return res;
}

std::unique_ptr<nlohmann::json> CoreStub::setDeviceFrequencyRange(int deviceId, int tileId, int minFreq, int maxFreq)
{
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceId < 0 || deviceId >= ze_device_handles.size()) {
        (*json)["error"] = "invalid device id";
        return json;
    }
    ze_result_t res;
    uint32_t sub_device_count = 0;
    res = zeDeviceGetSubDevices(ze_device_handles[deviceId], &sub_device_count, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGetSubDevices Failed with return code: " << to_string(res) << std::endl;
    }
    std::vector<ze_device_handle_t> sub_device_handles(sub_device_count);
    res = zeDeviceGetSubDevices(ze_device_handles[deviceId], &sub_device_count, sub_device_handles.data());
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGetSubDevices Failed with return code: " << to_string(res) << std::endl;
    }
    if (sub_device_count > 0 && tileId >= (int)sub_device_count) {
        (*json)["error"] = "invalid tile id";
        return json;
    }
    if (sub_device_count == 0) {
        if (tileId != 0) {
            (*json)["error"] = "invalid tile id";
            return json;
        }
        sub_device_handles.push_back(ze_device_handles[deviceId]);
    }
    bool supported = true;
    handleFreqByLevel0((zes_device_handle_t)sub_device_handles[tileId], true, minFreq, maxFreq, supported);
    if (supported)
        (*json)["status"] = "OK";
    else
        (*json)["error"] = "unsupported feature";
    return json;
}

std::vector<std::string> CoreStub::handleFreqByLevel0(zes_device_handle_t device, bool set, int minFreq, int maxFreq, bool& supported)
{
    std::vector<std::string> res;
    uint32_t frequency_domain_count = 0;
    ze_result_t status;
    status = zesDeviceEnumFrequencyDomains(device, &frequency_domain_count, nullptr);
    if (status != ZE_RESULT_SUCCESS) {
        std::cout << "zesDeviceEnumFrequencyDomains Failed with return code: " << to_string(status) << std::endl;
    }
    else {
        if (frequency_domain_count == 0) {
            supported = false;
            std::cout << "zesDeviceEnumFrequencyDomains Failed with zero frequency domain " << std::endl;
        }
        std::vector<zes_freq_handle_t> freq_handles(frequency_domain_count);
        status = zesDeviceEnumFrequencyDomains(device, &frequency_domain_count, freq_handles.data());
        if (status == ZE_RESULT_SUCCESS) {
            for (auto& freq : freq_handles) {
                zes_freq_properties_t prop;
                prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
                prop.pNext = nullptr;
                status = zesFrequencyGetProperties(freq, &prop);
                if (status == ZE_RESULT_SUCCESS) {
                    if (prop.type != ZES_FREQ_DOMAIN_GPU) {
                        continue;
                    }
                }
                zes_freq_range_t range;
                status = zesFrequencyGetRange(freq, &range);
                if (status == ZE_RESULT_SUCCESS) {
                    res.push_back(std::to_string((int)range.min));
                    res.push_back(std::to_string((int)range.max));
                }
                else {
                    supported = false;
                    std::cout << "zesFrequencyGetRange Failed with return code: " << to_string(status) << std::endl;
                }
                uint32_t avaiable_clock_count = 0;
                status = zesFrequencyGetAvailableClocks(freq, &avaiable_clock_count, nullptr);
                if (status == ZE_RESULT_SUCCESS) {
                    std::vector<double> avaiable_clocks(avaiable_clock_count);
                    if (status == ZE_RESULT_SUCCESS) {
                        status = zesFrequencyGetAvailableClocks(freq, &avaiable_clock_count, avaiable_clocks.data());
                        std::string str = std::to_string((int)avaiable_clocks[0]);
                        for (uint32_t i = 1; i < avaiable_clock_count; i++) {
                            str = str + ", " + std::to_string((int)(avaiable_clocks[i]));
                        }
                        res.push_back(str);
                    }
                }

                if (set) {
                    zes_freq_range_t newrange;
                    newrange.min = minFreq;
                    newrange.max = maxFreq;
                    status = zesFrequencySetRange(freq, &newrange);
                    if (status != ZE_RESULT_SUCCESS) {
                        supported = false;
                        std::cout << "zesFrequencySetRange Failed with return code: " << to_string(status) << std::endl;
                    }
                }
            }
        }
    }
    return res;
}

long long  CoreStub::getCurrentMillisecond() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string CoreStub::isotimestamp(uint64_t t, bool withoutDate) {
    time_t seconds = t / 1000;
    int milli_seconds = t % 1000;
    tm* tm_p = localtime(&seconds);
    char buf[50];
    char milli_buf[10];
    sprintf_s(milli_buf, "%03d", milli_seconds);
    if (withoutDate) {
        strftime(buf, sizeof(buf), "%T", tm_p);
        return std::string(buf) + "." + std::string(milli_buf);
    }
    else {
        strftime(buf, sizeof(buf), "%FT%T", tm_p);
        return std::string(buf) + "." + std::string(milli_buf);
    }
}

struct MetricsTypeEntry {
    xpum_stats_type_t key;
    std::string keyStr;
};

static MetricsTypeEntry metricsTypeArray[]{
    {XPUM_STATS_COPY_UTILIZATION, "XPUM_STATS_COPY_UTILIZATION"},
    {XPUM_STATS_COMPUTE_UTILIZATION, "XPUM_STATS_COMPUTE_UTILIZATION"},
    {XPUM_STATS_MEDIA_UTILIZATION, "XPUM_STATS_MEDIA_UTILIZATION"},
    {XPUM_STATS_GPU_UTILIZATION, "XPUM_STATS_GPU_UTILIZATION"},
    {XPUM_STATS_EU_ACTIVE, "XPUM_STATS_EU_ACTIVE"},
    {XPUM_STATS_EU_STALL, "XPUM_STATS_EU_STALL"},
    {XPUM_STATS_EU_IDLE, "XPUM_STATS_EU_IDLE"},
    {XPUM_STATS_POWER, "XPUM_STATS_POWER"},
    {XPUM_STATS_ENERGY, "XPUM_STATS_ENERGY"},
    {XPUM_STATS_GPU_FREQUENCY, "XPUM_STATS_GPU_FREQUENCY"},
    {XPUM_STATS_GPU_CORE_TEMPERATURE, "XPUM_STATS_GPU_CORE_TEMPERATURE"},
    {XPUM_STATS_MEMORY_USED, "XPUM_STATS_MEMORY_USED"},
    {XPUM_STATS_MEMORY_UTILIZATION, "XPUM_STATS_MEMORY_UTILIZATION"},
    {XPUM_STATS_MEMORY_BANDWIDTH, "XPUM_STATS_MEMORY_BANDWIDTH"},
    {XPUM_STATS_MEMORY_READ, "XPUM_STATS_MEMORY_READ"},
    {XPUM_STATS_MEMORY_WRITE, "XPUM_STATS_MEMORY_WRITE"},
    {XPUM_STATS_MEMORY_READ_THROUGHPUT, "XPUM_STATS_MEMORY_READ_THROUGHPUT"},
    {XPUM_STATS_MEMORY_WRITE_THROUGHPUT, "XPUM_STATS_MEMORY_WRITE_THROUGHPUT"},
    {XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION"},
    {XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION, "XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION"},
    {XPUM_STATS_RAS_ERROR_CAT_RESET, "XPUM_STATS_RAS_ERROR_CAT_RESET"},
    {XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS"},
    {XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS, "XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_GPU_REQUEST_FREQUENCY, "XPUM_STATS_GPU_REQUEST_FREQUENCY"},
    {XPUM_STATS_MEMORY_TEMPERATURE, "XPUM_STATS_MEMORY_TEMPERATURE"},
    {XPUM_STATS_FREQUENCY_THROTTLE, "XPUM_STATS_FREQUENCY_THROTTLE"},
    {XPUM_STATS_PCIE_READ_THROUGHPUT, "XPUM_STATS_PCIE_READ_THROUGHPUT"},
    {XPUM_STATS_PCIE_WRITE_THROUGHPUT, "XPUM_STATS_PCIE_WRITE_THROUGHPUT"} };

std::string CoreStub::metricsTypeToString(xpum_stats_type_t metricsType) {
    for (auto item : metricsTypeArray) {
        if (item.key == metricsType) {
            return item.keyStr;
        }
    }
    return std::to_string(metricsType);
}

xpum_device_stats_data_t CoreStub::getMetricsByLevel0(zes_device_handle_t device, xpum_stats_type_t metricsType) {
    xpum_device_stats_data_t data{};
    if (metricsType == XPUM_STATS_POWER) {
        ze_result_t res;
        uint32_t power_domain_count = 0;
        res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
        std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
        res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
        if (res == ZE_RESULT_SUCCESS) {
            for (auto& power : power_handles) {
                zes_power_energy_counter_t snap1, snap2;
                res = zesPowerGetEnergyCounter(power, &snap1);
                if (res == ZE_RESULT_SUCCESS) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    uint64_t power_val = 0;
                    res = zesPowerGetEnergyCounter(power, &snap2);
                    if (res == ZE_RESULT_SUCCESS) {
                        power_val = measurement_data_scale * (snap2.energy - snap1.energy) / (snap2.timestamp - snap1.timestamp);
                        data.max = data.min = data.avg = data.value = (int)power_val;
                    }
                }
            }
        }
    }

    if (metricsType == XPUM_STATS_GPU_CORE_TEMPERATURE || metricsType == XPUM_STATS_MEMORY_TEMPERATURE) {
        uint32_t temp_sensor_count = 0;
        ze_result_t res;
        res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, nullptr);
        std::vector<zes_temp_handle_t> temp_sensors(temp_sensor_count);
        if (res == ZE_RESULT_SUCCESS) {
            res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, temp_sensors.data());
            for (auto& temp : temp_sensors) {
                zes_temp_properties_t props;
                res = zesTemperatureGetProperties(temp, &props);
                if (res != ZE_RESULT_SUCCESS) {
                    continue;
                }
                if (metricsType == XPUM_STATS_GPU_CORE_TEMPERATURE && props.type != ZES_TEMP_SENSORS_GPU) {
                    continue;
                }
                if (metricsType == XPUM_STATS_MEMORY_TEMPERATURE && props.type != ZES_TEMP_SENSORS_MEMORY) {
                    continue;
                }
                double temp_val = -1;
                res = zesTemperatureGetState(temp, &temp_val);
                temp_val *= measurement_data_scale;
                if (res == ZE_RESULT_SUCCESS) {
                    data.max = data.min = data.avg = data.value = (uint64_t)temp_val;
                }
            }
        }
    }

    if (metricsType == XPUM_STATS_ENERGY) {
        ze_result_t res;
        uint32_t power_domain_count = 0;
        res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
            res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& power : power_handles) {
                    zes_power_energy_counter_t energy;
                    res = zesPowerGetEnergyCounter(power, &energy);
                    if (res == ZE_RESULT_SUCCESS)
                        data.value = energy.energy / 1000;
                }
            }
        }
    }

    if (metricsType == XPUM_STATS_GPU_FREQUENCY) {
        ze_result_t res;
        uint32_t frequency_domain_count = 0;
        res = zesDeviceEnumFrequencyDomains(device, &frequency_domain_count, nullptr);
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zes_freq_handle_t> freq_handles(frequency_domain_count);
            res = zesDeviceEnumFrequencyDomains(device, &frequency_domain_count, freq_handles.data());
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& freq : freq_handles) {
                    zes_freq_properties_t prop;
                    prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
                    res = zesFrequencyGetProperties(freq, &prop);
                    if (res == ZE_RESULT_SUCCESS) {
                        if (prop.type != ZES_FREQ_DOMAIN_GPU) {
                            continue;
                        }
                        zes_freq_state_t freq_state;
                        res = zesFrequencyGetState(freq, &freq_state);
                        if (res == ZE_RESULT_SUCCESS)
                            data.max = data.min = data.avg = data.value = (uint64_t)freq_state.actual;
                    }
                }
            }
        }
    }

    if (metricsType == XPUM_STATS_MEMORY_BANDWIDTH) {
        ze_result_t res;
        uint32_t mem_module_count = 0;
        res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr);
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zes_mem_handle_t> mems(mem_module_count);
            res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data());
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& mem : mems) {
                    zes_mem_properties_t props;
                    props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                    res = zesMemoryGetProperties(mem, &props);
                    if (res != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE) {
                        continue;
                    }

                    zes_mem_bandwidth_t s1, s2;
                    res = zesMemoryGetBandwidth(mem, &s1);
                    if (res == ZE_RESULT_SUCCESS) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        res = zesMemoryGetBandwidth(mem, &s2);
                        if (res == ZE_RESULT_SUCCESS && (s2.maxBandwidth * (s2.timestamp - s1.timestamp)) != 0) {
                            uint64_t val = 1000000 * ((s2.readCounter - s1.readCounter) + (s2.writeCounter - s1.writeCounter)) / (s2.maxBandwidth * (s2.timestamp - s1.timestamp));
                            if (val > 100) {
                                val = 100;
                            }
                            data.max = data.min = data.avg = data.value = val;
                        }
                    }
                }
            }
        }
    }

    if (metricsType == XPUM_STATS_MEMORY_USED) {
        uint32_t mem_module_count = 0;
        ze_result_t res;
        res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr);
        if (res == ZE_RESULT_SUCCESS && mem_module_count > 0) {
            std::vector<zes_mem_handle_t> mems(mem_module_count);
            res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data());
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& mem : mems) {
                    zes_mem_properties_t props;
                    props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                    res = zesMemoryGetProperties(mem, &props);
                    if (res == ZE_RESULT_SUCCESS) {
                        zes_mem_state_t sysman_memory_state = {};
                        sysman_memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
                        res = zesMemoryGetState(mem, &sysman_memory_state);
                        if (res == ZE_RESULT_SUCCESS && sysman_memory_state.size != 0) {
                            uint64_t used = props.physicalSize == 0 ? sysman_memory_state.size - sysman_memory_state.free : props.physicalSize - sysman_memory_state.free;
                            data.max = data.min = data.avg = data.value = used / 1024 / 1024;
                        }
                    }
                }
            }
        } else {
            data.max = data.min = data.avg = data.value = getMemUsedByNativeAPI() / 1024 / 1024;
        }
    }

    if (metricsType == XPUM_STATS_MEMORY_READ_THROUGHPUT || metricsType == XPUM_STATS_MEMORY_WRITE_THROUGHPUT) {
        uint32_t mem_module_count = 0;
        ze_result_t res;
        res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr);
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zes_mem_handle_t> mems(mem_module_count);
            res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data());
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& mem : mems) {
                    zes_mem_properties_t props;
                    props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                    res = zesMemoryGetProperties(mem, &props);
                    if (res != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE) {
                        continue;
                    }
                    zes_mem_bandwidth_t mem_bandwidth1;
                    res = zesMemoryGetBandwidth(mem, &mem_bandwidth1);
                    if (res == ZE_RESULT_SUCCESS) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(memory_sampling_interval));
                        zes_mem_bandwidth_t mem_bandwidth2;
                        res = zesMemoryGetBandwidth(mem, &mem_bandwidth2);
                        double val = 0.0;
                        if (res == ZE_RESULT_SUCCESS) {
                            if (metricsType == XPUM_STATS_MEMORY_READ_THROUGHPUT) {
                                if (mem_bandwidth2.readCounter >= mem_bandwidth1.readCounter)
                                    val = (mem_bandwidth2.readCounter - mem_bandwidth1.readCounter) * (1000.0 / memory_sampling_interval) / 1024;
                                else
                                    val = -1;
                            }
                            else {
                                if (mem_bandwidth2.writeCounter >= mem_bandwidth1.writeCounter)
                                    val = (mem_bandwidth2.writeCounter - mem_bandwidth1.writeCounter) * (1000.0 / memory_sampling_interval) / 1024;
                                else
                                    val = -1;
                            }
                                
                            data.max = data.min = data.avg = data.value = (uint64_t)val;
                        }
                    }
                }
            }
        }
    }

    if (metricsType == XPUM_STATS_COMPUTE_UTILIZATION || metricsType == XPUM_STATS_MEDIA_UTILIZATION || metricsType == XPUM_STATS_GPU_UTILIZATION || metricsType == XPUM_STATS_COPY_UTILIZATION) {
        static uint64_t compute_engine = 0;
        static uint64_t media_engine = 0;

        if (metricsType == XPUM_STATS_GPU_UTILIZATION) {
            data.max = data.min = data.avg = data.value = std::max(compute_engine, media_engine);
            return data;
        }

        uint32_t engine_count = 0;
        ze_result_t res;
        res = zesDeviceEnumEngineGroups(device, &engine_count, nullptr);
        if (res == ZE_RESULT_SUCCESS && engine_count>0) {
            std::vector<zes_engine_handle_t> engines(engine_count);
            res = zesDeviceEnumEngineGroups(device, &engine_count, engines.data());
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& engine : engines) {
                    zes_engine_properties_t props;
                    props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                    res = zesEngineGetProperties(engine, &props);
                    if (res != ZE_RESULT_SUCCESS)
                        continue;

                    if (metricsType == XPUM_STATS_COMPUTE_UTILIZATION && props.type != ZES_ENGINE_GROUP_COMPUTE_ALL)
                        continue;

                    if (metricsType == XPUM_STATS_MEDIA_UTILIZATION && props.type != ZES_ENGINE_GROUP_MEDIA_ALL)
                        continue;

                    if (metricsType == XPUM_STATS_COPY_UTILIZATION && props.type != ZES_ENGINE_GROUP_COPY_ALL)
                        continue;

                    zes_engine_stats_t snap1;
                    res = zesEngineGetActivity(engine, &snap1);
                    if (res == ZE_RESULT_SUCCESS) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(engine_sampling_interval));
                        zes_engine_stats_t snap2;
                        res = zesEngineGetActivity(engine, &snap2);
                        double val = 0.0;
                        if (res == ZE_RESULT_SUCCESS) {
                            val = (snap2.activeTime - snap1.activeTime) * measurement_data_scale * 100.0 / (snap2.timestamp - snap1.timestamp);
                            if (val > measurement_data_scale * 100.0)
                                val = measurement_data_scale * 100.0;
                            if (metricsType == XPUM_STATS_COMPUTE_UTILIZATION) {
                                data.max = data.min = data.avg = data.value = compute_engine = (uint64_t)val;
                            } else if (metricsType == XPUM_STATS_MEDIA_UTILIZATION) {
                                data.max = data.min = data.avg = data.value = media_engine = (uint64_t)val;
                            } else {
                                data.max = data.min = data.avg = data.value = (uint64_t)val;
                            }
                        }
                    }
                }
            }
        } else {
            if (metricsType == XPUM_STATS_COMPUTE_UTILIZATION) {
                data.max = data.min = data.avg = data.value = (uint64_t)(getComputeEngineUtilByNativeAPI() * 100.0);
            } else if (metricsType == XPUM_STATS_MEDIA_UTILIZATION) {
                data.max = data.min = data.avg = data.value = (uint64_t)(getMediaEngineUtilByNativeAPI() * 100.0);
            } else if (metricsType == XPUM_STATS_COPY_UTILIZATION) {
                data.max = data.min = data.avg = data.value = (uint64_t)(getCopyEngineUtilByNativeAPI() * 100.0);
            }
        }
    }
    return data;
}

std::unique_ptr<nlohmann::json>  CoreStub::getStatistics(int deviceId, bool enableFilter) 
{
    updatePDHQuery();
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceId < 0 || deviceId >= ze_device_handles.size()) {
        (*json)["error"] = "invalid device id";
        return json;
    }
    (*json)["device_id"] = deviceId;
    uint64_t begin = getCurrentMillisecond();
    xpum_device_stats_data_t data1 = getMetricsByLevel0(zes_device_handles[deviceId], XPUM_STATS_ENERGY);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    uint64_t end = getCurrentMillisecond();
    (*json)["begin"] = isotimestamp(begin);
    (*json)["end"] = isotimestamp(end);
    (*json)["elapsed_time"] = (end - begin);
    std::vector<nlohmann::json> deviceLevelStatsDataList;
    xpum_device_stats_data_t data2 = getMetricsByLevel0(zes_device_handles[deviceId], XPUM_STATS_ENERGY);
    auto energy = nlohmann::json();
    energy["total"] = data2.value;
    energy["metrics_type"] = metricsTypeToString(XPUM_STATS_ENERGY);
    energy["value"] = data2.value - data1.value;
    deviceLevelStatsDataList.push_back(energy);

    std::vector<nlohmann::json> tileLevelStatsDataList;
    (*json)["device_id"] = deviceId;
    std::vector<nlohmann::json> tileJsonList;
    ze_result_t res;
    uint32_t sub_device_count = 0;
    res = zeDeviceGetSubDevices(ze_device_handles[deviceId], &sub_device_count, nullptr);
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGetSubDevices Failed with return code: " << to_string(res) << std::endl;
    }
    std::vector<ze_device_handle_t> sub_device_handles(sub_device_count);
    res = zeDeviceGetSubDevices(ze_device_handles[deviceId], &sub_device_count, sub_device_handles.data());
    if (res != ZE_RESULT_SUCCESS) {
        std::cout << "zeDeviceGetSubDevices Failed with return code: " << to_string(res) << std::endl;
    }
    if (sub_device_count == 0) {
        sub_device_handles.push_back(ze_device_handles[deviceId]);
    }
    for (int i = 0; i < sub_device_handles.size(); ++i) {
        std::vector<nlohmann::json> dataList;
        for (auto item : metricsTypeArray) {
            if (item.key == XPUM_STATS_POWER || item.key == XPUM_STATS_GPU_CORE_TEMPERATURE
                || item.key == XPUM_STATS_MEMORY_TEMPERATURE || item.key == XPUM_STATS_GPU_FREQUENCY
                || item.key == XPUM_STATS_MEMORY_BANDWIDTH || item.key == XPUM_STATS_MEMORY_USED 
                || item.key == XPUM_STATS_COMPUTE_UTILIZATION || item.key == XPUM_STATS_MEDIA_UTILIZATION 
                || item.key == XPUM_STATS_GPU_UTILIZATION
                || item.key == XPUM_STATS_MEMORY_READ_THROUGHPUT || item.key == XPUM_STATS_MEMORY_WRITE_THROUGHPUT || item.key == XPUM_STATS_COPY_UTILIZATION
            ) {
                xpum_device_stats_data_t data = getMetricsByLevel0((zes_device_handle_t)sub_device_handles[i], item.key);
                auto tmp = nlohmann::json();
                if (item.key == XPUM_STATS_POWER || item.key == XPUM_STATS_GPU_CORE_TEMPERATURE 
                    || item.key == XPUM_STATS_MEMORY_TEMPERATURE || item.key == XPUM_STATS_COMPUTE_UTILIZATION || item.key == XPUM_STATS_MEDIA_UTILIZATION 
                    || item.key == XPUM_STATS_GPU_UTILIZATION || item.key == XPUM_STATS_COPY_UTILIZATION) {
                    tmp["avg"] = data.avg * 1.0 / measurement_data_scale;
                    tmp["min"] = data.min * 1.0 / measurement_data_scale;
                    tmp["max"] = data.max * 1.0 / measurement_data_scale;
                    tmp["value"] = data.avg * 1.0 / measurement_data_scale;
                }
                else {
                    tmp["avg"] = data.avg;
                    tmp["min"] = data.min;
                    tmp["max"] = data.max;
                    tmp["value"] = data.avg;
                }
                tmp["metrics_type"] = metricsTypeToString(item.key);
                if (data.value > UINT32_MAX)
                    continue;
                if (data.value == 0 && (item.key == XPUM_STATS_GPU_CORE_TEMPERATURE
                    || item.key == XPUM_STATS_MEMORY_TEMPERATURE))
                    continue;
                if (item.key == XPUM_STATS_GPU_CORE_TEMPERATURE && tmp["value"] > 130)
                    continue;
                if (item.key == XPUM_STATS_MEMORY_TEMPERATURE && tmp["value"] > 100)
                    continue;
                dataList.push_back(tmp);
                if (sub_device_handles.size() == 1)
                    deviceLevelStatsDataList.push_back(tmp);
            }
        }
        auto tmp = nlohmann::json();
        tmp["tile_id"] = i;
        tmp["data_list"] = dataList;
        tileLevelStatsDataList.push_back(tmp);
    }
    (*json)["device_level"] = deviceLevelStatsDataList;
    if (tileLevelStatsDataList.size() > 0 && sub_device_handles.size() > 1)
        (*json)["tile_level"] = tileLevelStatsDataList;
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::setMemoryEccState(int deviceId, bool enabled) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceId < 0 || deviceId >= ze_device_handles.size()) {
        (*json)["error"] = "invalid device id";
        return json;
    }
    bool available = false;
    bool configurable = false;
    uint8_t cur = 0xFF;
    uint8_t pen = 0xFF;
    uint8_t req = enabled ? 1 : 0;
    std::string bdf = getBdfAddress(zes_device_handles[deviceId]);
    if (igsc_instance.setDeviceEccState(bdf, req, &cur, &pen)) {
        (*json)["status"] = "OK";
        (*json)["memory_ecc_current_state"] = eccStateToString(cur);
        (*json)["memory_ecc_pending_state"] = eccStateToString(pen);

    }
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::runFirmwareFlash(int deviceId, unsigned int type, const std::string& filePath) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceId < 0 || deviceId >= ze_device_handles.size()) {
        (*json)["error"] = "invalid device id";
        return json;
    }

    std::string invalidChars = "{}()><&*'|=?;[]$-#~!\"%:+,`";
    auto file_name_pos = filePath.find_last_of('\\');
    auto itr = std::find_if(filePath.begin() + file_name_pos, filePath.end(),
                            [invalidChars](unsigned char ch) { return invalidChars.find(ch) != invalidChars.npos; });
    if (itr != filePath.end()) {
        (*json)["error"] = "Illegal firmware image filename. Image filename should not contain following characters: {}()><&*'|=?;[]$-#~!\"%:+,`";
        return json;
    }
    auto deviceList = getSiblingDevices(deviceId);
    if (deviceList.size() == 0)
        deviceList.push_back(deviceId);

    std::string image_file = filePath;
    for (auto id : deviceList) {
        std::string bdf = getBdfAddress(zes_device_handles[id]);
        if (type == XPUM_DEVICE_FIRMWARE_GFX && !igsc_instance.isFwImageAndDeviceCompatible(bdf, image_file)) {
            (*json)["error"] = "The image file is a right FW image file, but not proper for the target GPU.";
            return json;        
        }
        std::string error_message;
        if (type == XPUM_DEVICE_FIRMWARE_GFX_DATA && !igsc_instance.isFwDataImageAndDeviceCompatible(bdf, image_file, error_message)) {
            (*json)["error"] = error_message;
            return json;  
        }

        flash_results.push_back(std::async(std::launch::async, [bdf, image_file, type, this] {
            int res = 0;
            if (type == XPUM_DEVICE_FIRMWARE_GFX)
                res = igsc_instance.runFlashGSC(bdf, image_file);
            else
                res = igsc_instance.runFlashGSCData(bdf, image_file);
            if (res == 0)
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
            else
                return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
        }));
    }

    (*json)["result"] = "OK";
    return json;
}

std::unique_ptr<nlohmann::json> CoreStub::getFirmwareFlashResult(int deviceId, unsigned int type) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (deviceId < 0 || deviceId >= ze_device_handles.size()) {
        (*json)["error"] = "invalid device id";
        return json;
    }
    for (int i = 0; i < flash_results.size(); i++) {
        std::future_status status = flash_results[i].wait_for(std::chrono::milliseconds(0));
        if (status != std::future_status::ready) {
            (*json)["result"] = "ONGOING";
            return json;
        }
    }
    for (int i = 0; i < flash_results.size(); i++) {
        if (flash_results[i].get() == xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR) {
            (*json)["result"] = "FAILED";
            return json;
        }
    }
    (*json)["result"] = "OK";
    return json;
}

std::vector<int> CoreStub::getSiblingDevices(int deviceId) {
    std::vector<int> res;
    for (int id : sibling_devices[deviceId])
        res.push_back(id);
    return res;
}