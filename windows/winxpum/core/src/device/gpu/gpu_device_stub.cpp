/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file gpu_device_stub.cpp
 */

#include "pch.h"
#include "device/gpu/gpu_device_stub.h"

#include "device/gpu/gpu_device.h"
#include "device/win_native.h"
#include "infrastructure/logger.h"
#include "infrastructure/exception/level_zero_initialization_exception.h"
#include "infrastructure/handle_lock.h"
#include "infrastructure/configuration.h"
#include "infrastructure/utility.h"
#include "api/device_model.h"
#include "core/core.h"

#include <iomanip>

namespace xpum {

    GPUDeviceStub::GPUDeviceStub() : initialized(false) {
        XPUM_LOG_DEBUG("GPUDeviceStub()");
    }

    GPUDeviceStub::~GPUDeviceStub() {
        closePDHQuery();
        XPUM_LOG_DEBUG("~GPUDeviceStub()");
    }

    GPUDeviceStub& GPUDeviceStub::instance() {
        static GPUDeviceStub stub;
        std::unique_lock<std::mutex> lock(stub.mutex);
        if (!stub.initialized) {
            stub.init();
        }
        return stub;
    }

    void GPUDeviceStub::init() {
        initialized = true;

        _putenv(const_cast<char*>("ZES_ENABLE_SYSMAN=1"));
        _putenv(const_cast<char*>("ZE_ENABLE_PCI_ID_DEVICE_ORDER=1"));

        if (std::any_of(Configuration::getEnabledMetrics().begin(), Configuration::getEnabledMetrics().end(),
                                                                     [](const MeasurementType type) { return type == METRIC_EU_ACTIVE || type == METRIC_EU_IDLE || type == METRIC_EU_STALL || type == METRIC_PERF; })) {
            _putenv(const_cast<char*>("ZET_ENABLE_METRICS=1"));
        }
        ze_result_t ret = zeInit(0);
        if (ret != ZE_RESULT_SUCCESS) {
            XPUM_LOG_ERROR("GPUDeviceStub::init zeInit error: {0:x}", ret);
            throw LevelZeroInitializationException("zeInit error");
        }
        openPDHQuery();
    }


    std::string GPUDeviceStub::to_hex_string(uint32_t val) {
        std::stringstream s;
        s << std::string("0x") << std::hex << val << std::dec;
        return s.str();
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
            ze_driver_properties_t driver_prop = {};
            XPUM_ZE_HANDLE_LOCK(p_driver, zeDriverGetProperties(p_driver, &driver_prop));

            for (auto& device : devices) {
                std::vector<DeviceCapability> capabilities;
                zes_device_handle_t zes_device = (zes_device_handle_t)device;
                zes_device_properties_t props = {};
                props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                XPUM_ZE_HANDLE_LOCK(zes_device, zesDeviceGetProperties(zes_device, &props));
                if (props.core.type == ZE_DEVICE_TYPE_GPU) {
                    auto p_gpu = std::make_shared<GPUDevice>(std::to_string(p_devices->size()), zes_device, p_driver, capabilities);
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_TYPE, std::string("GPU")));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID, to_hex_string(props.core.deviceId)));
                    // p_gpu->addProperty(Property(DeviceProperty::BOARD_NUMBER,std::string(props.boardNumber)));
                    // p_gpu->addProperty(Property(DeviceProperty::BRAND_NAME,std::string(props.brandName)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DRIVER_VERSION, std::to_string(driver_prop.driverVersion)));
                    // p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_LINUX_KERNEL_VERSION, getKernelVersion()));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_SERIAL_NUMBER, std::string(props.serialNumber)));
                    std::string vendor_name = std::string(props.vendorName);
                    if (vendor_name.empty())
                        vendor_name = "Intel(R) Corporation";
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_VENDOR_NAME, vendor_name));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_CORE_CLOCK_RATE_MHZ, std::to_string(props.core.coreClockRate)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MAX_MEM_ALLOC_SIZE_BYTE, std::to_string(props.core.maxMemAllocSize)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MAX_HARDWARE_CONTEXTS, std::to_string(props.core.maxHardwareContexts)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MAX_COMMAND_QUEUE_PRIORITY, std::to_string(props.core.maxCommandQueuePriority)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS_PER_SUB_SLICE, std::to_string(props.core.numEUsPerSubslice)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUB_SLICES_PER_SLICE, std::to_string(props.core.numSubslicesPerSlice)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SLICES, std::to_string(props.core.numSlices)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_THREADS_PER_EU, std::to_string(props.core.numThreadsPerEU)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PHYSICAL_EU_SIMD_WIDTH, std::to_string(props.core.physicalEUSimdWidth)));

                    auto& uuidBuf = props.core.uuid.id;
                    char uuidStr[37] = {};
                    sprintf(uuidStr,
                        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                        uuidBuf[15], uuidBuf[14], uuidBuf[13], uuidBuf[12], uuidBuf[11], uuidBuf[10], uuidBuf[9], uuidBuf[8],
                        uuidBuf[7], uuidBuf[6], uuidBuf[5], uuidBuf[4], uuidBuf[3], uuidBuf[2], uuidBuf[1], uuidBuf[0]);
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_UUID, std::string(uuidStr)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_VENDOR_ID, to_hex_string(props.core.vendorId)));

                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUBDEVICE, std::to_string(props.numSubdevices)));
                    uint32_t tileCount = props.numSubdevices == 0 ? 1 : props.numSubdevices;
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES, std::to_string(tileCount)));
                    uint32_t euCount = tileCount * props.core.numSlices * props.core.numSubslicesPerSlice * props.core.numEUsPerSubslice;
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS, std::to_string(euCount)));

                    zes_pci_properties_t pci_props = {};

                    XPUM_ZE_HANDLE_LOCK(device, res = zesDevicePciGetProperties(device, &pci_props));
                    if (res == ZE_RESULT_SUCCESS) {
                        std::string pciName = std::string(props.core.name);
                        p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, pciName));
                        p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, to_string(pci_props.address)));
                        if (pci_props.maxSpeed.gen > 0)
                            p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_GENERATION, std::to_string(pci_props.maxSpeed.gen)));
                        if (pci_props.maxSpeed.width > 0)
                            p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_MAX_LINK_WIDTH, std::to_string(pci_props.maxSpeed.width)));
                        // p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_STEPPING, stepping));
                        // p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_SLOT, getPciSlot(pci_props.address)));
                        // p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_OAM_SOCKET_ID, getOAMSocketId(pci_props.address)));
                    }
                    else {
                        p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME, std::string(props.core.name)));
                    }

                    uint64_t physical_size = 0;
                    uint64_t free_size = 0;
                    uint32_t mem_module_count = 0;
                    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
                    std::vector<zes_mem_handle_t> mems(mem_module_count);
                    XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
                    if (res == ZE_RESULT_SUCCESS) {
                        for (auto& mem : mems) {
                            uint64_t mem_module_physical_size = 0;
                            zes_mem_properties_t props = {};
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
                            }
                        }
                        p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_PHYSICAL_SIZE_BYTE, std::to_string(physical_size)));
                        p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_FREE_SIZE_BYTE, std::to_string(free_size)));
                    }
                    if (physical_size == 0) {
                        physical_size = (uint64_t)getMemSizeByNativeAPI();
                        p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_PHYSICAL_SIZE_BYTE, std::to_string(physical_size)));
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

                    uint32_t media_engine_count = 0;
                    uint32_t meida_enhancement_engine_count = 0;
                    toGetDeviceMediaEngineCount(device, media_engine_count, meida_enhancement_engine_count, props.core.deviceId);
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENGINES, std::to_string(media_engine_count)));
                    p_gpu->addProperty(Property(XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENH_ENGINES, std::to_string(meida_enhancement_engine_count)));

                    p_devices->push_back(p_gpu);
                }
            }
        }
        return p_devices;
    }

    void GPUDeviceStub::toGetDeviceMediaEngineCount(const zes_device_handle_t& device, uint32_t& media_engine_count, uint32_t& meida_enhancement_engine_count, int32_t deviceId) noexcept{
        ze_result_t res;
        uint32_t engine_grp_count = 0;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_grp_count, nullptr));
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zes_engine_handle_t> engines(engine_grp_count);
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_grp_count, engines.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& engine : engines) {
                    zes_engine_properties_t props = {};
                    props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                    props.pNext = nullptr;
                    XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, &props));
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
        if (media_engine_count == 0 || meida_enhancement_engine_count == 0){
            int model_type = getDeviceModelByPciDeviceId(deviceId);
            if (model_type == XPUM_DEVICE_MODEL_ATS_M_1 || model_type == XPUM_DEVICE_MODEL_ATS_M_3 || model_type == XPUM_DEVICE_MODEL_ATS_M_1G) {
                media_engine_count = (media_engine_count == 0) ? 2 : media_engine_count;
                meida_enhancement_engine_count = (meida_enhancement_engine_count == 0) ? 2 : meida_enhancement_engine_count;
            }
        }
        return;
    }


    void GPUDeviceStub::toGetDeviceSusPower(const zes_device_handle_t& device, int32_t& susPower, bool& supported) noexcept{
        ze_result_t status;
        uint32_t power_domain_count = 0;
        
        status = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
        if (status != ZE_RESULT_SUCCESS || power_domain_count == 0) {
            supported = false;
            return;
        }
        std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
        status = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
        if (status == ZE_RESULT_SUCCESS) {
            for (auto& power : power_handles) {
                zes_power_properties_t props = {};
                props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
                props.pNext = nullptr;
                status = zesPowerGetProperties(power, &props);
                if (status == ZE_RESULT_SUCCESS) {
                    zes_power_sustained_limit_t sustained = {};
                    status = zesPowerGetLimits(power, &sustained, nullptr, nullptr);
                    if (status == ZE_RESULT_SUCCESS) {
                        susPower = sustained.power / 1000;
                        supported = true;
                    } else {
                        supported = false;
                    }
                }
            }
        }
        return;
    }

    uint32_t GPUDeviceStub::toGetDeviceId(const zes_device_handle_t& device) noexcept {
        ze_result_t status;
        ze_device_properties_t ze_device_properties = {};
        ze_device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
        ze_device_properties.pNext = nullptr;
        status = zeDeviceGetProperties(device, &ze_device_properties);
        if (status != ZE_RESULT_SUCCESS) {
            return 0;
        }
        return ze_device_properties.deviceId;
    }

    uint32_t GPUDeviceStub::toGetDevicePowerLimit(const zes_device_handle_t& device) noexcept {
        int32_t max_limit = 300;
        uint32_t deviceId = toGetDeviceId(device);
        switch (deviceId) {
            case 0x56c1:
                max_limit = 25;
                break;
            case 0x56c0:
                max_limit = 120;
                break;
            case 0x56c2:
                max_limit = 120;
                break;
            case 0x4905:
                max_limit = 25;
                break;
            default:
                max_limit = 300;
        }
        return max_limit*1000;
    }

    bool GPUDeviceStub::toSetPowerSustainedLimits(const zes_device_handle_t& device, int32_t powerLimit) noexcept {
        ze_result_t status;
        uint32_t power_domain_count = 0;
        int32_t max_limit;
        max_limit = toGetDevicePowerLimit(device);

        if (powerLimit > max_limit) {
            return false;
        }
        status = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
        if (status != ZE_RESULT_SUCCESS || power_domain_count == 0) {
            return false;
        }
        std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
        status = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data());
        if (status == ZE_RESULT_SUCCESS) {
            for (auto& power : power_handles) {
                zes_power_properties_t props = {};
                props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
                props.pNext = nullptr;
                status = zesPowerGetProperties(power, &props);
                if (status == ZE_RESULT_SUCCESS) {
                    zes_power_sustained_limit_t sustained = {};
                    sustained.enabled = true;
                    sustained.power = powerLimit;
                    status = zesPowerSetLimits(power, &sustained, nullptr, nullptr);
                    if (status != ZE_RESULT_SUCCESS) {
                        return false;
                    } else {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void GPUDeviceStub::toGetDeviceFrequencyRange(const zes_device_handle_t& device, int32_t tileId, double& min, double& max, std::string& clocks, bool& supported) noexcept {
        std::vector<std::string> res;
        uint32_t frequency_domain_count = 0;
        
        ze_result_t status;
        min = 0.0;
        max = 0.0;
        clocks = "";
        supported = false;
        status = zesDeviceEnumFrequencyDomains(device, &frequency_domain_count, nullptr);
        if (status != ZE_RESULT_SUCCESS || frequency_domain_count == 0) {
            XPUM_LOG_WARN("zesDeviceEnumFrequencyDomains Failed with return code: {}", status);
            supported = false;
            return;
        }
        std::vector<zes_freq_handle_t> freq_handles(frequency_domain_count);
        status = zesDeviceEnumFrequencyDomains(device, &frequency_domain_count, freq_handles.data());
        if (status != ZE_RESULT_SUCCESS) {
            supported = false;
            return;
        } else {
            if (frequency_domain_count == 0) {
                supported = false;
                XPUM_LOG_WARN("zesDeviceEnumFrequencyDomains Failed with zero frequency domain ");
            }
            if (status == ZE_RESULT_SUCCESS) {
                for (auto& freq : freq_handles) {
                    zes_freq_properties_t prop = {};
                    prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
                    prop.pNext = nullptr;
                    status = zesFrequencyGetProperties(freq, &prop);
                    if (status == ZE_RESULT_SUCCESS) {
                        if (prop.type != ZES_FREQ_DOMAIN_GPU) {
                            continue;
                        }
                        if (/*prop.onSubdevice != true || */ prop.subdeviceId != tileId) {
                            continue;
                        } 
                    }
                    zes_freq_range_t range = {};
                    status = zesFrequencyGetRange(freq, &range);
                    if (status == ZE_RESULT_SUCCESS) {
                        min = range.min;
                        max = range.max;//res.push_back(std::to_string((int)range.max))
                        supported = true;
                    } else {
                        XPUM_LOG_WARN("zesFrequencyGetRange Failed with return code: {}",status);
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
                            clocks = str;
                        }
                    }
                    break;
                }
            }
        }
        return;
    }

    bool GPUDeviceStub::toSetDeviceFrequencyRange(const zes_device_handle_t& device, int32_t tileId, double min, double max) noexcept {
        std::vector<std::string> res;
        uint32_t frequency_domain_count = 0;

        ze_result_t status;
        status = zesDeviceEnumFrequencyDomains(device, &frequency_domain_count, nullptr);
        if (status != ZE_RESULT_SUCCESS || frequency_domain_count == 0) {
            XPUM_LOG_WARN("zesDeviceEnumFrequencyDomains Failed with return code: {}", status);
            return false;
        }
        std::vector<zes_freq_handle_t> freq_handles(frequency_domain_count);
        status = zesDeviceEnumFrequencyDomains(device, &frequency_domain_count, freq_handles.data());
        if (status != ZE_RESULT_SUCCESS) {
            return false;
        } else {
            if (status == ZE_RESULT_SUCCESS) {
                for (auto& freq : freq_handles) {
                    zes_freq_properties_t prop = {};
                    prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
                    prop.pNext = nullptr;
                    status = zesFrequencyGetProperties(freq, &prop);
                    if (status == ZE_RESULT_SUCCESS) {
                        if (prop.type != ZES_FREQ_DOMAIN_GPU) {
                            continue;
                        }
                        if (/*prop.onSubdevice != true || */ prop.subdeviceId != tileId) {
                            continue;
                        }
                    }
                    zes_freq_range_t range = {};
                    range.min = min;
                    range.max = max;
                    status = zesFrequencySetRange(freq, &range);
                    if (status != ZE_RESULT_SUCCESS) {
                        XPUM_LOG_WARN("zesFrequencyGetRange Failed with return code: {}", status);
                        return false;
                    } else {
                        return true;
                    }
                    break;
                }
            }
        }
        return false;
    }

    void GPUDeviceStub::toGetSimpleEccState(uint8_t& current, uint8_t& pending) noexcept {
        return;
    }

    void GPUDeviceStub::toGetFreqAvailableClocks(const zes_device_handle_t& device, int32_t tileId, std::vector<double>& clocksList) noexcept {
        uint32_t freq_count = 0;
        ze_result_t res;
        res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr);
        std::vector<zes_freq_handle_t> freq_handles(freq_count);
        if (res == ZE_RESULT_SUCCESS) {
            res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data());
            for (auto& ph_freq : freq_handles) {
                zes_freq_properties_t prop = {};
                prop.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
                res = zesFrequencyGetProperties(ph_freq, &prop);
                if (res == ZE_RESULT_SUCCESS) {
                    if (prop.type != ZES_FREQ_DOMAIN_GPU || prop.subdeviceId != tileId) {
                        continue;
                    }
                    uint32_t pCount = 0;
                    res = zesFrequencyGetAvailableClocks(ph_freq, &pCount, nullptr);
                    double clockArray[255];
                    if (res == ZE_RESULT_SUCCESS && pCount <= 255) {
                        res = zesFrequencyGetAvailableClocks(ph_freq, &pCount, clockArray);
                        if (res == ZE_RESULT_SUCCESS) {
                            for (uint32_t i = 0; i < pCount; i++) {
                                clocksList.push_back(clockArray[i]);
                            }
                        }
                    }
                }
            }
        }
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPower(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetPower error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
        uint32_t power_domain_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr));
        std::vector<zes_pwr_handle_t> power_handles(power_domain_count);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPowerDomains(device, &power_domain_count, power_handles.data()));
        if (res == ZE_RESULT_SUCCESS && power_domain_count > 0) {
            for (auto& power : power_handles) {
                zes_power_properties_t props = {};
                XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetProperties(power, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    zes_power_energy_counter_t snap1 = {};
                    zes_power_energy_counter_t snap2 = {};
                    XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap1));
                    if (res == ZE_RESULT_SUCCESS) {
                        uint64_t time1 = Utility::getCurrentMicrosecond();
                        std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::POWER_MONITOR_INTERNAL_PERIOD));
                        XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap2));
                        if (res == ZE_RESULT_SUCCESS) {
                            uint64_t time2 = Utility::getCurrentMicrosecond();
                            ret->setCurrent((snap2.energy - snap1.energy) / (time2 - time1));
                        } else {
                            exception_msgs["zesPowerGetEnergyCounter"] = res;    
                        }
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
    
        if (!exception_msgs.empty())
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    }

    void GPUDeviceStub::getPerformanceFactor(const zes_device_handle_t& device, std::vector<PerformanceFactor>& pf) {
        if (device == nullptr) {
            return;
        }
        uint32_t pfCount = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPerformanceFactorDomains(device, &pfCount, nullptr));
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zes_perf_handle_t> hPerf(pfCount);
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumPerformanceFactorDomains(device, &pfCount, hPerf.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto perf : hPerf) {
                    zes_perf_properties_t prop = {};
                    double factor = 0;
                    XPUM_ZE_HANDLE_LOCK(perf, res = zesPerformanceFactorGetProperties(perf, &prop));
                    if (res == ZE_RESULT_SUCCESS) {
                        XPUM_ZE_HANDLE_LOCK(perf, res = zesPerformanceFactorGetConfig(perf, &factor));
                        if (res == ZE_RESULT_SUCCESS) {
                            PerformanceFactor p(prop.onSubdevice, prop.subdeviceId, prop.engines, factor);
                            pf.push_back(p);
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

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetActuralRequestFrequency(const zes_device_handle_t& device, MeasurementType type) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetActuralRequestFrequency error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
        uint32_t freq_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, nullptr));
        std::vector<zes_freq_handle_t> freq_handles(freq_count);
        if (res == ZE_RESULT_SUCCESS && freq_count > 0) {
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freq_count, freq_handles.data()));
            for (auto& ph_freq : freq_handles) {
                zes_freq_properties_t props = {};
                XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetProperties(ph_freq, &props));
                if (res == ZE_RESULT_SUCCESS) {
                    if (props.type != ZES_FREQ_DOMAIN_GPU) {
                        continue;
                    }
                    zes_freq_state_t freq_state = {};
                    XPUM_ZE_HANDLE_LOCK(ph_freq, res = zesFrequencyGetState(ph_freq, &freq_state));
                    if (res == ZE_RESULT_SUCCESS && freq_state.actual >= 0) {
                        if (type == MeasurementType::METRIC_FREQUENCY)
                            ret->setCurrent(freq_state.actual);
                        else if (type == MeasurementType::METRIC_REQUEST_FREQUENCY)
                            ret->setCurrent(freq_state.request);
                        else if (type == MeasurementType::METRIC_MEDIA_ENGINE_FREQUENCY) {
                            if (Utility::isATSMPlatform(device) == true) {
                                std::vector<PerformanceFactor> pfs;
                                getPerformanceFactor(device, pfs);
                                for (auto& pf : pfs) {
                                    if (pf.getEngine() == ZES_ENGINE_TYPE_FLAG_MEDIA) {
                                        ret->setCurrent(freq_state.actual * pf.getFactor() / 100);
                                        break;
                                    }
                                }
                            }
                        }
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
        if (!exception_msgs.empty())
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetTemperature(const zes_device_handle_t& device, MeasurementType type) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetTemperature error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
        uint32_t temp_sensor_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, nullptr));
        std::vector<zes_temp_handle_t> temp_sensors(temp_sensor_count);
        if (res == ZE_RESULT_SUCCESS && temp_sensor_count > 0) {
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumTemperatureSensors(device, &temp_sensor_count, temp_sensors.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& temp : temp_sensors) {
                    zes_temp_properties_t props = {};
                    XPUM_ZE_HANDLE_LOCK(temp, res = zesTemperatureGetProperties(temp, &props));
                    if (res == ZE_RESULT_SUCCESS) {
                        switch (props.type) {
                            case ZES_TEMP_SENSORS_GPU:
                                if (type == MeasurementType::METRIC_TEMPERATURE) {
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
                                    } else {
                                        exception_msgs["zesTemperatureGetState"] = res;
                                    }
                                }
                                break;
                            case ZES_TEMP_SENSORS_MEMORY:
                                if (type == MeasurementType::METRIC_MEMORY_TEMPERATURE) {
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

        if (!exception_msgs.empty())
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemoryUsedUtilization(const zes_device_handle_t& device, MeasurementType type) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetMemoryUsedUtilization error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
        uint32_t mem_module_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
        if (res == ZE_RESULT_SUCCESS && mem_module_count > 0) {
            std::vector<zes_mem_handle_t> mems(mem_module_count);
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& mem : mems) {
                    zes_mem_state_t sysman_memory_state = {};
                    sysman_memory_state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
                    XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetState(mem, &sysman_memory_state));
                    if (res == ZE_RESULT_SUCCESS && sysman_memory_state.size != 0) {
                        uint64_t used = sysman_memory_state.size - sysman_memory_state.free;
                        uint64_t utilization = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * used * 100 / sysman_memory_state.size;
                        if (type == MeasurementType::METRIC_MEMORY_USED) {
                            ret->setCurrent(used);
                        } else if (type == MeasurementType::METRIC_MEMORY_UTILIZATION) {
                            ret->setCurrent(utilization);
                            ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                        } 
                    } else {
                        exception_msgs["zesMemoryGetState"] = res;
                    }
                }
            } else {
                exception_msgs["zesDeviceEnumMemoryModules"] = res;
            }
        } else {
            exception_msgs["zesDeviceEnumMemoryModules"] = res;
        }

        if (!exception_msgs.empty()){
            auto mem_used_byte = getMemUsedByNativeAPI();
            if (type == MeasurementType::METRIC_MEMORY_USED) {
                if (mem_used_byte > 0) {
                    ret->setCurrent(mem_used_byte);
                } else {
                    ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
                }
            } else if (type == MeasurementType::METRIC_MEMORY_UTILIZATION) {
                auto total_byte = getMemSizeByNativeAPI();
                if (mem_used_byte > 0 && total_byte > 0) {
                    uint64_t utilization = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE * mem_used_byte * 100 / total_byte;
                    ret->setCurrent(utilization);
                    ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                } else {
                    ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
                }
            }
        }
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemoryBandwidth(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetMemoryBandwidth error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
        uint32_t mem_module_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
        if (res == ZE_RESULT_SUCCESS && mem_module_count > 0) {
            std::vector<zes_mem_handle_t> mems(mem_module_count);
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& mem : mems) {
                    zes_mem_properties_t props = {};
                    props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                    XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetProperties(mem, &props));
                    if (res != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE) {
                        continue;
                    }

                    zes_mem_bandwidth_t s1 = {};
                    zes_mem_bandwidth_t s2 = {};
                    XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetBandwidth(mem, &s1));
                    if (res == ZE_RESULT_SUCCESS) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::MEMORY_BANDWIDTH_MONITOR_INTERNAL_PERIOD));
                        XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetBandwidth(mem, &s2));
                        if (res == ZE_RESULT_SUCCESS && (s2.maxBandwidth * (s2.timestamp - s1.timestamp)) != 0) {
                            uint64_t val = 100 * 1000000 * ((s2.readCounter - s1.readCounter) + (s2.writeCounter - s1.writeCounter)) / (s2.maxBandwidth * (s2.timestamp - s1.timestamp));
                            if (val > 100) {
                                val = 100;
                            }
                            ret->setCurrent(val);
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

        if (!exception_msgs.empty())
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetMemoryReadWrite(const zes_device_handle_t& device, MeasurementType type) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetMemoryReadWrite error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
        uint32_t mem_module_count = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, nullptr));
        if (res == ZE_RESULT_SUCCESS && mem_module_count > 0) {
            std::vector<zes_mem_handle_t> mems(mem_module_count);
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumMemoryModules(device, &mem_module_count, mems.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& mem : mems) {
                    zes_mem_properties_t props = {};
                    props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                    XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetProperties(mem, &props));
                    if (res != ZE_RESULT_SUCCESS || props.location != ZES_MEM_LOC_DEVICE) {
                        continue;
                    }

                    zes_mem_bandwidth_t mem_bandwidth1 = {};
                    XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetBandwidth(mem, &mem_bandwidth1));
                    if (res == ZE_RESULT_SUCCESS) {
                        if (type == MeasurementType::METRIC_MEMORY_READ) {
                            ret->setCurrent(mem_bandwidth1.readCounter);
                        } else if (type == MeasurementType::METRIC_MEMORY_WRITE) {
                            ret->setCurrent(mem_bandwidth1.writeCounter);
                        } else {
                            int sampling_interval = Configuration::MEMORY_READ_WRITE_INTERNAL_PERIOD;
                            std::this_thread::sleep_for(std::chrono::milliseconds(sampling_interval));
                            zes_mem_bandwidth_t mem_bandwidth2 = {};
                            XPUM_ZE_HANDLE_LOCK(mem, res = zesMemoryGetBandwidth(mem, &mem_bandwidth2));
                            if (res == ZE_RESULT_SUCCESS) {
                                double read_val = -1;
                                double write_val = -1;

                                if (mem_bandwidth2.readCounter >= mem_bandwidth1.readCounter)
                                    read_val = (mem_bandwidth2.readCounter - mem_bandwidth1.readCounter) * (1000.0 / sampling_interval) / 1024;
                               
                                if (mem_bandwidth2.writeCounter >= mem_bandwidth1.writeCounter)
                                    write_val = (mem_bandwidth2.writeCounter - mem_bandwidth1.writeCounter) * (1000.0 / sampling_interval) / 1024;
                                
                                if (type == MeasurementType::METRIC_MEMORY_READ_THROUGHPUT) {
                                    ret->setCurrent((uint64_t)read_val);
                                    ret->setAdditionalData(METRIC_MEMORY_WRITE_THROUGHPUT, write_val);
                                } else {
                                    ret->setCurrent((uint64_t)write_val);
                                    ret->setAdditionalData(METRIC_MEMORY_READ_THROUGHPUT, read_val);
                                }
                            } else {
                                exception_msgs["zesMemoryGetBandwidth"] = res;
                            }
                        }
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

        if (!exception_msgs.empty())
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEngineUtilization(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetGPUUtilization(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEngineGroupUtilization(const zes_device_handle_t& device, MeasurementType type) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetEngineGroupUtilization error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
        uint32_t engine_count = 0;
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
        XPUM_LOG_DEBUG("res = {}, engine_count = {}", res, engine_count);
        bool _engineGroupDataGotten = false;
        if (res == ZE_RESULT_SUCCESS && engine_count > 0) {
            std::vector<zes_engine_handle_t> engines(engine_count);
            std::map<uint32_t, std::vector<uint32_t>> group_utilizations;
            XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumEngineGroups(device, &engine_count, engines.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& engine : engines) {
                    zes_engine_properties_t props = {};
                    props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                    props.pNext = nullptr;
                    XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetProperties(engine, &props));
                    if (res == ZE_RESULT_SUCCESS) {
                        switch (type) {
                            case MeasurementType::METRIC_COMPUTATION:
                                if (props.type != ZES_ENGINE_GROUP_ALL) {
                                    continue;
                                }
                                break;
                            case MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION:
                                if (props.type != ZES_ENGINE_GROUP_COMPUTE_ALL) {
                                    continue;
                                }
                                break;
                            case MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION:
                                if (props.type != ZES_ENGINE_GROUP_RENDER_ALL) {
                                    continue;
                                }
                                break;
                            case MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION:
                                if (props.type != ZES_ENGINE_GROUP_MEDIA_ALL) {
                                    continue;
                                }
                                break;
                            case MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION:
                                if (props.type != ZES_ENGINE_GROUP_COPY_ALL) {
                                    continue;
                                }
                                break;
                            case MeasurementType::METRIC_ENGINE_GROUP_3D_ALL_UTILIZATION:
                                if (props.type != ZES_ENGINE_GROUP_3D_ALL) {
                                    continue;
                                }
                                break;
                            default:
                                break;
                        }
                        zes_engine_stats_t snap1 = {};
                        XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetActivity(engine, &snap1));
                        if (res == ZE_RESULT_SUCCESS) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::ENGINE_GPU_UTILIZATION_INTERNAL_PERIOD));
                            zes_engine_stats_t snap2 = {};
                            XPUM_ZE_HANDLE_LOCK(engine, res = zesEngineGetActivity(engine, &snap2));
                            double val = 0;
                            bool valid = false;
                            if (snap2.timestamp > snap1.timestamp) {
                                val = (snap2.activeTime - snap1.activeTime) * 100.0 / (snap2.timestamp - snap1.timestamp);
                                if (val <= 100.0 && val >= 0) {
                                    valid = true;
                                    _engineGroupDataGotten = valid || _engineGroupDataGotten;
                                }
                            }
                            if (res == ZE_RESULT_SUCCESS && valid == true) {
                                ret->setCurrent(val * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                                ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                            } else {
                                exception_msgs["zesEngineGetActivity"] = res;
                                XPUM_LOG_DEBUG("s1.activeTime = {}, s1.timestamp = {}, s2.activeTime = {}, s2.timestamp = {}", snap1.activeTime, snap1.timestamp, snap2.activeTime, snap2.timestamp);                            
                            }
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
        }
        std::vector<std::shared_ptr<Device>> devices;
        Core::instance().getDeviceManager()->getDeviceList(devices);
        const int deviceCount = devices.size();
        if (!_engineGroupDataGotten && deviceCount == 1) {
            if (type == MeasurementType::METRIC_ENGINE_GROUP_COPY_ALL_UTILIZATION) {
                ret->setCurrent((uint64_t)(getCopyEngineUtilByNativeAPI() * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE));
                ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                return ret;
            }

            if (type == MeasurementType::METRIC_ENGINE_GROUP_RENDER_ALL_UTILIZATION) {
                ret->setCurrent((uint64_t)(getRenderEngineUtilByNativeAPI() * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE));
                ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                return ret;
            }
            
            if (type == MeasurementType::METRIC_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION) {
                ret->setCurrent((uint64_t)(getComputeEngineUtilByNativeAPI() * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE));
                ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                return ret;
            }

            if (type == MeasurementType::METRIC_ENGINE_GROUP_MEDIA_ALL_UTILIZATION) {
                ret->setCurrent((uint64_t)(getMediaEngineUtilByNativeAPI() * Configuration::DEFAULT_MEASUREMENT_DATA_SCALE));
                ret->setScale(Configuration::DEFAULT_MEASUREMENT_DATA_SCALE);
                return ret;
            }
        }

        if (!exception_msgs.empty())
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEnergy(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetEnergy error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
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
                    zes_power_energy_counter_t snap = {};
                    XPUM_ZE_HANDLE_LOCK(power, res = zesPowerGetEnergyCounter(power, &snap));
                    if (res == ZE_RESULT_SUCCESS) {
                        ret->setCurrent(snap.energy * 1.0 / 1000);
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

        if (!exception_msgs.empty())
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetEuActiveStallIdle(const zes_device_handle_t& device, const ze_driver_handle_t& driver, MeasurementType type) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetEuActiveStallIdle error");
            return ret;
        }

        ze_result_t res;
        zet_metric_group_handle_t hMetricGroup = nullptr;

        uint32_t metricGroupCount = 0;
        XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle get hMetricGroup");
        XPUM_ZE_HANDLE_LOCK(device, res = zetMetricGroupGet(device, &metricGroupCount, nullptr));
        if (res == ZE_RESULT_SUCCESS) {
            std::vector<zet_metric_group_handle_t> metricGroups(metricGroupCount);
            XPUM_ZE_HANDLE_LOCK(device, res = zetMetricGroupGet(device, &metricGroupCount, metricGroups.data()));
            if (res == ZE_RESULT_SUCCESS) {
                for (auto& metric_group : metricGroups) {
                    zet_metric_group_properties_t metric_group_properties = {};
                    metric_group_properties.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
                    res = zetMetricGroupGetProperties(metric_group, &metric_group_properties);
                    if (res == ZE_RESULT_SUCCESS) {
                        if (std::strcmp(metric_group_properties.name, "ComputeBasic") == 0 && metric_group_properties.samplingType == ZET_METRIC_GROUP_SAMPLING_TYPE_FLAG_TIME_BASED) {
                            hMetricGroup = metric_group;
                            break;
                        }
                    }
                }
            }
        }
     
        if (hMetricGroup == nullptr) {
            ret->setErrors("toGetEuActiveStallIdle error");
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zetMetricGroupGet error: {0:x}", res);
            return ret;
        }
        ze_context_handle_t hContext = nullptr;
        ze_context_desc_t context_desc = {
            ZE_STRUCTURE_TYPE_CONTEXT_DESC,
            nullptr,
            0};
        XPUM_ZE_HANDLE_LOCK(driver, res = zeContextCreate(driver, &context_desc, &hContext));
        if (res != ZE_RESULT_SUCCESS) {
            ret->setErrors("toGetEuActiveStallIdle error");
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zeContextCreate error: {0:x}", res);
            return ret;
        }

        zet_metric_streamer_handle_t hMetricStreamer = nullptr;
        zet_metric_streamer_desc_t metricStreamerDesc = {ZET_STRUCTURE_TYPE_METRIC_STREAMER_DESC};
        XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle active hMetricGroup");
        XPUM_ZE_HANDLE_LOCK(device, res = zetContextActivateMetricGroups(hContext, device, 1, &hMetricGroup));
        if (res != ZE_RESULT_SUCCESS) {
            ret->setErrors("toGetEuActiveStallIdle error");
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zetContextActivateMetricGroups error: {0:x}", res);
            return ret;
        }

        metricStreamerDesc.samplingPeriod = Configuration::EU_ACTIVE_STALL_IDLE_STREAMER_SAMPLING_PERIOD;
        XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle open hMetricStreamer");
        XPUM_ZE_HANDLE_LOCK(device, res = zetMetricStreamerOpen(hContext, device, hMetricGroup, &metricStreamerDesc, nullptr, &hMetricStreamer));
        if (res != ZE_RESULT_SUCCESS) {
            ret->setErrors("toGetEuActiveStallIdle error");
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zetMetricStreamerOpen error: {0:x}", res);
            return ret;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(Configuration::EU_ACTIVE_STALL_IDLE_MONITOR_INTERNAL_PERIOD));
        size_t rawSize = 0;
        XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle read hMetricStreamer");
        res = zetMetricStreamerReadData(hMetricStreamer, UINT32_MAX, &rawSize, nullptr);
        if (res != ZE_RESULT_SUCCESS || rawSize == 0) {
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() raw data size1 {}, res {0:x}", rawSize, res);
            ret->setErrors("toGetEuActiveStallIdle error");
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zetMetricStreamerReadData error: {0:x}", res);
            return ret;
        }
        std::vector<uint8_t> rawData(rawSize);
        res = zetMetricStreamerReadData(hMetricStreamer, UINT32_MAX, &rawSize, rawData.data());
        rawSize = rawData.size();
        if (res != ZE_RESULT_SUCCESS || rawSize == 0) {
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() raw data size2 {}, res {0:x}", rawSize, res); 
            ret->setErrors("toGetEuActiveStallIdle error");
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zetMetricStreamerReadData error: {0:x}", res);
            return ret;
        }

        res = zetMetricStreamerClose(hMetricStreamer);
        if (res != ZE_RESULT_SUCCESS) {
            ret->setErrors("toGetEuActiveStallIdle error");
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zetMetricStreamerClose error: {0:x}", res);
            return ret;
        }
        res = zetContextActivateMetricGroups(hContext, device, 0, nullptr);
        if (res != ZE_RESULT_SUCCESS) {
            ret->setErrors("toGetEuActiveStallIdle error");
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zetContextActivateMetricGroups error: {0:x}", res);
            return ret;
        }
        uint32_t numMetricValues = 0;
        zet_metric_group_calculation_type_t calculationType = ZET_METRIC_GROUP_CALCULATION_TYPE_METRIC_VALUES;
        res = zetMetricGroupCalculateMetricValues(hMetricGroup, calculationType, rawSize, rawData.data(), &numMetricValues, nullptr);
        if (res != ZE_RESULT_SUCCESS || numMetricValues == 0 || rawSize == 0) {
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() raw data size3 {}, numMetricValues {}, res {0:x}", rawSize, numMetricValues, res);
            XPUM_LOG_ERROR("GPUDeviceStub::toGetEuActiveStallIdle zetMetricGroupCalculateMetricValues error: {0:x}", res);
            ret->setErrors("toGetEuActiveStallIdle error");
            return ret;
        }
        std::vector<zet_typed_value_t> metricValues(numMetricValues);
        res = zetMetricGroupCalculateMetricValues(hMetricGroup, calculationType, rawSize, rawData.data(), &numMetricValues, metricValues.data());
        numMetricValues = metricValues.size();
        if (res != ZE_RESULT_SUCCESS || numMetricValues == 0) {
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() numMetricValues {}, res {0:x}", numMetricValues, res);
            ret->setErrors("toGetEuActiveStallIdle error");
            return ret;
        }
        uint32_t metricCount = 0;
        res = zetMetricGet(hMetricGroup, &metricCount, nullptr);
        if (res != ZE_RESULT_SUCCESS || metricCount == 0) {
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() metricCount1 {}, res {0:x}", metricCount, res);
            ret->setErrors("toGetEuActiveStallIdle error");
            return ret;
        }
        std::vector<zet_metric_handle_t> phMetrics(metricCount);
        res = zetMetricGet(hMetricGroup, &metricCount, phMetrics.data());
        if (res != ZE_RESULT_SUCCESS || metricCount == 0) {
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() metricCount2 {}, res {0:x}", metricCount, res);
            ret->setErrors("toGetEuActiveStallIdle error");
            return ret;
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
                zet_metric_properties_t metricProperties = {};
                res = zetMetricGetProperties(phMetrics[metric], &metricProperties);
                if (res != ZE_RESULT_SUCCESS) {
                    ret->setErrors("toGetEuActiveStallIdle error");
                    return ret;
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
                if (strcmp(metricProperties.name, "XveActive") == 0 ||
                    strcmp(metricProperties.name, "XVE_ACTIVE") == 0) {
                    currentXueActive = data.value.fp32;
                }
                if (strcmp(metricProperties.name, "XveStall") == 0 ||
                    strcmp(metricProperties.name, "XVE_STALL") == 0) {
                    currentXveStall = data.value.fp32;
                }
                if (std::strcmp(metricProperties.name, "GpuTime") == 0) {
                    currentGPUElapsedTime = data.value.ui64;
                }
            }
            currentEuActive = max(currentEuActive, currentXueActive);
            currentEuStall = max(currentEuStall, currentXveStall);
            if (currentEuActive > 100 || currentEuStall > 100) {
                continue;
            }
            totalGpuBusy += currentGPUElapsedTime * currentGpuBusy;
            totalEuStall += currentGPUElapsedTime * currentEuStall;
            totalEuActive += currentGPUElapsedTime * currentEuActive;
            totalGPUElapsedTime += currentGPUElapsedTime;
        }
        if (totalGPUElapsedTime == 0) {
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() numReports {}", numReports);
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() totalEuActive {}", totalEuActive);
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() totalEuStall {}", totalEuStall);
            XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() totalGPUElapsedTime {}", totalGPUElapsedTime);
            ret->setErrors("toGetEuActiveStallIdle error - zero gpu elapsed time");
            return ret;
        }
        uint64_t euActive = totalEuActive / totalGPUElapsedTime;
        uint64_t euStall = totalEuStall / totalGPUElapsedTime;
        uint64_t euIdle = 100 - euActive - euStall;
        if (euIdle > 100) {
            ret->setErrors("toGetEuActiveStallIdle error - abnormal EU data");
            return ret;
        }
        int scale = Configuration::DEFAULT_MEASUREMENT_DATA_SCALE;
        XPUM_LOG_DEBUG("GPUDeviceStub::toGetEuActiveStallIdle() euActive {}, euStall {}, euIdle {}", euActive, euStall, euIdle);
        euActive *= scale;
        euStall *= scale;
        euIdle *= scale;
        ret->setScale(scale);
        if (type == MeasurementType::METRIC_EU_ACTIVE) {
            ret->setCurrent(euActive);
            ret->setAdditionalData(METRIC_EU_STALL, euStall);
            ret->setAdditionalData(METRIC_EU_IDLE, euIdle);
        } else if (type == MeasurementType::METRIC_EU_STALL) {
            ret->setCurrent(euStall);
            ret->setAdditionalData(METRIC_EU_ACTIVE, euActive);
            ret->setAdditionalData(METRIC_EU_IDLE, euIdle);
        } else if (type == MeasurementType::METRIC_EU_IDLE) {
            ret->setCurrent(euIdle);
            ret->setAdditionalData(METRIC_EU_ACTIVE, euActive);
            ret->setAdditionalData(METRIC_EU_STALL, euStall);
        }
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetRasError(const zes_device_handle_t& device, MeasurementType type) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetFrequencyThrottle(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetFrequencyThrottleReason(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        if (device == nullptr) {
            ret->setErrors("toGetFrequencyThrottleReason error");
            return ret;
        }

        std::map<std::string, ze_result_t> exception_msgs;
        uint32_t freqDomainCount = 0;
        ze_result_t res;
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freqDomainCount, nullptr));
        std::vector<zes_freq_handle_t> freqDomainList(freqDomainCount);
        XPUM_ZE_HANDLE_LOCK(device, res = zesDeviceEnumFrequencyDomains(device, &freqDomainCount, freqDomainList.data()));
        if (res == ZE_RESULT_SUCCESS && freqDomainCount > 0) {
            for (auto& hFreq : freqDomainList) {
                zes_freq_properties_t freqProps = {};
                XPUM_ZE_HANDLE_LOCK(hFreq, res = zesFrequencyGetProperties(hFreq, &freqProps));
                if (res == ZE_RESULT_SUCCESS) {
                    if (freqProps.type == ZES_FREQ_DOMAIN_GPU) {
                        zes_freq_state_t freqState = {};
                        XPUM_ZE_HANDLE_LOCK(hFreq, res = zesFrequencyGetState(hFreq, &freqState));
                        if (res == ZE_RESULT_SUCCESS) {
                            ret->setCurrent(freqState.throttleReasons);
                        } else {
                            exception_msgs["zesFrequencyGetState"] = res;
                        }
                    }
                } else {
                    exception_msgs["zesFrequencyGetProperties"] = res;
                }
            }
        } else {
            exception_msgs["zesDeviceEnumFrequencyDomains"] = res;
        }
        
        if (!exception_msgs.empty())
            ret->setErrors(buildErrors(exception_msgs, __func__, __LINE__));
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPCIeReadThroughput(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPCIeWriteThroughput(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPCIeRead(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPCIeWrite(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetFabricThroughput(const zes_device_handle_t& device) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }

    std::shared_ptr<MeasurementData> GPUDeviceStub::toGetPerfMetrics(const zes_device_handle_t& device, const ze_driver_handle_t& driver) noexcept {
        std::shared_ptr<MeasurementData> ret = std::make_shared<MeasurementData>();
        ret->setErrors("UNSUPPORTED");
        return ret;
    }
}