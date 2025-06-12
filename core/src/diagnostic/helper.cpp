/*
 *  Copyright (C) 2022-2024 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file helper.cpp
 */

#include "helper.h"
#include "diagnostic_manager.h"
#include "precheck.h"
#include "infrastructure/xpum_config.h"
#include <sys/stat.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <functional>
#include <numeric>

namespace xpum {

    double calculateMean(const std::vector<double>& data) {
        double sum = std::accumulate(std::begin(data), std::end(data), 0.0);
        double mean = sum / data.size();
        return mean;
    }

    double calcaulateVariance(const std::vector<double>& data) {
        double sum = std::accumulate(std::begin(data), std::end(data), 0.0);
        double mean = sum / data.size();
        
        double variance = 0.0;
        std::for_each(std::begin(data), std::end(data), [&](const double d) {
            variance += pow(d - mean, 2);
        });
        variance /= data.size();
        return variance;
    }

    std::string zeResultErrorCodeStr(ze_result_t ret) {
        std::ostringstream os;
        os << "0x" << std::setfill('0') << std::setw(8) << std::hex << ret;
        return os.str();
    }

    void contextCreate(ze_driver_handle_t hDriver, ze_context_handle_t *context) {
        ze_result_t ret;
        ze_context_desc_t context_desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0 };
        XPUM_ZE_HANDLE_LOCK(hDriver, ret = zeContextCreate(hDriver, &context_desc, context));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeContextCreate()[" + zeResultErrorCodeStr(ret) + "]");
        }        
    }
    void contextDestroy(ze_context_handle_t hContext) {
        ze_result_t ret = zeContextDestroy(hContext);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeContextDestroy()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void moduleCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device, std::vector<uint8_t> binary_file, ze_module_handle_t *module_handle) {
        ze_module_desc_t module_description = {};
        module_description.stype = ZE_STRUCTURE_TYPE_MODULE_DESC;
        module_description.pNext = nullptr;
        module_description.format = ZE_MODULE_FORMAT_IL_SPIRV;
        module_description.inputSize = static_cast<uint32_t>(binary_file.size());
        module_description.pInputModule = binary_file.data();
        module_description.pBuildFlags = nullptr;
        ze_result_t ret;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeModuleCreate(context, ze_device, &module_description, module_handle, nullptr));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeModuleCreate()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void moduleDestroy(ze_module_handle_t hModule) {
        ze_result_t ret = zeModuleDestroy(hModule);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeModuleDestroy()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void kernelCreate(ze_module_handle_t hModule, std::string name, ze_kernel_handle_t *hKernel) {
        ze_kernel_desc_t test_function_description = {};
        test_function_description.stype = ZE_STRUCTURE_TYPE_KERNEL_DESC;
        test_function_description.pNext = nullptr;
        test_function_description.flags = 0;
        test_function_description.pKernelName = name.c_str();

        ze_result_t ret = zeKernelCreate(hModule, &test_function_description, hKernel);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelCreate()[" + zeResultErrorCodeStr(ret) + "]");
        }        
    }

    void kernelDestroy(ze_kernel_handle_t hKernel) {
        ze_result_t ret = zeKernelDestroy(hKernel);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelDestroy()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void kernelSetGroupSize(ze_kernel_handle_t hKernel, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ) {
        ze_result_t ret = zeKernelSetGroupSize(hKernel, groupSizeX, groupSizeY, groupSizeZ);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelSetGroupSize()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void kernelSetArgumentValue(ze_kernel_handle_t hKernel, uint32_t argIndex, size_t argSize, const void *pArgValue) {
        ze_result_t ret = zeKernelSetArgumentValue(hKernel, argIndex, argSize, pArgValue);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeKernelSetArgumentValue()[" + zeResultErrorCodeStr(ret) + "]");
        }  
    }

    void memoryAlloc(const ze_context_handle_t &context, ze_device_handle_t ze_device, size_t size, size_t alignment, void **ptr) {
        ze_device_mem_alloc_desc_t device_desc = {};
        device_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        device_desc.pNext = nullptr;
        device_desc.ordinal = 0;
        device_desc.flags = 0;
        ze_result_t ret;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeMemAllocDevice(context, &device_desc, size, alignment, ze_device, ptr));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("memoryAlloc()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }
    void memoryAllocShared(const ze_context_handle_t &context, ze_device_handle_t ze_device, size_t size, size_t alignment, void **ptr) {
        ze_device_mem_alloc_desc_t device_desc_output = {};
        device_desc_output.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        device_desc_output.ordinal = 0;
        device_desc_output.flags = 0;
        device_desc_output.pNext = nullptr;

        ze_host_mem_alloc_desc_t host_desc_output = {};
        host_desc_output.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
        host_desc_output.flags = 0;
        host_desc_output.pNext = nullptr;
        ze_result_t ret;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeMemAllocShared(context, &device_desc_output, &host_desc_output, size, alignment, ze_device, ptr));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("memoryAlloc()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void memoryAllocHost(const ze_context_handle_t &context, size_t size, size_t alignment, void **ptr) {
        ze_host_mem_alloc_desc_t host_desc = {};
        host_desc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
        host_desc.pNext = nullptr;
        host_desc.flags = 0;

        ze_result_t ret = zeMemAllocHost(context, &host_desc, size, alignment, ptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("memoryAllocHost()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void memoryFree(const ze_context_handle_t &context, const void *ptr) {
        ze_result_t ret = zeMemFree(context, const_cast<void *>(ptr));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("memoryFree()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void commandQueueCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device, const uint32_t command_queue_group_ordinal, const uint32_t command_queue_index, ze_command_queue_handle_t *phCommandQueue, uint32_t flags) {
        ze_command_queue_desc_t command_queue_description{};
        command_queue_description.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        command_queue_description.pNext = nullptr;
        command_queue_description.ordinal = command_queue_group_ordinal;
        command_queue_description.index = command_queue_index;
        command_queue_description.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        command_queue_description.flags = flags;

        ze_result_t ret;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeCommandQueueCreate(context, ze_device, &command_queue_description, phCommandQueue));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueCreate()[" + zeResultErrorCodeStr(ret) + "]");
        }    
    }

    void commandQueueExecuteCommandLists(ze_command_queue_handle_t hCommandQueue, ze_command_list_handle_t hCommandList) {
        ze_result_t ret = zeCommandQueueExecuteCommandLists(hCommandQueue, 1, &hCommandList, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueExecuteCommandLists()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }
    
    void commandQueueSynchronize(ze_command_queue_handle_t hCommandQueue) {
        ze_result_t ret = zeCommandQueueSynchronize(hCommandQueue, UINT64_MAX);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueSynchronize()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void commandQueueDestroy(ze_command_queue_handle_t hCommandQueue) {
        ze_result_t ret = zeCommandQueueDestroy(hCommandQueue);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueDestroy()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void commandListCreate(const ze_context_handle_t &context, ze_device_handle_t ze_device, uint32_t command_queue_group_ordinal, ze_command_list_handle_t *phCommandList, uint32_t flags) {
        ze_command_list_desc_t command_list_description{};
        command_list_description.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
        command_list_description.commandQueueGroupOrdinal = command_queue_group_ordinal;
        command_list_description.pNext = nullptr;
        command_list_description.flags = flags;
        ze_result_t ret;
        XPUM_ZE_HANDLE_LOCK(ze_device, ret = zeCommandListCreate(context, ze_device, &command_list_description, phCommandList));
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandQueueCreate()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void commandListAppendBarrier(ze_command_list_handle_t hCommandList) {
        ze_result_t ret = zeCommandListAppendBarrier(hCommandList, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendBarrier()[" + zeResultErrorCodeStr(ret) + "]");
        }        
    }

    void commandListAppendLaunchKernel(ze_command_list_handle_t hCommandList, ze_kernel_handle_t hKernel, const ze_group_count_t *pLaunchFuncArgs) {
        ze_result_t ret = zeCommandListAppendLaunchKernel(hCommandList, hKernel, pLaunchFuncArgs, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendLaunchKernel()[" + zeResultErrorCodeStr(ret) + "]");
        }          
    }

    void commandListAppendMemoryCopy(ze_command_list_handle_t hCommandList, void *dstptr, const void *srcptr, size_t size) {
        ze_result_t ret = zeCommandListAppendMemoryCopy(hCommandList, dstptr, srcptr, size, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendMemoryCopy()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    void commandListAppendMemoryFill(ze_command_list_handle_t hCommandList, void *ptr, const void *pattern, size_t pattern_size, size_t size) {
        ze_result_t ret = zeCommandListAppendMemoryFill(hCommandList, ptr, pattern, pattern_size, size, nullptr, 0, nullptr);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListAppendMemoryCopy()[" + zeResultErrorCodeStr(ret) + "]");
        }        
    }

    void commandListReset(ze_command_list_handle_t hCommandList) {
        ze_result_t ret = zeCommandListReset(hCommandList);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("commandListReset()[" + zeResultErrorCodeStr(ret) + "]");
        }          
    }

    void commandListClose(ze_command_list_handle_t hCommandList) {
        ze_result_t ret = zeCommandListClose(hCommandList);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("commandListClose()[" + zeResultErrorCodeStr(ret) + "]");
        }        
    }

    void commandListDestroy(ze_command_list_handle_t hCommandList) {
        ze_result_t ret = zeCommandListDestroy(hCommandList);
        if (ret != ZE_RESULT_SUCCESS) {
            throw BaseException("zeCommandListDestroy()[" + zeResultErrorCodeStr(ret) + "]");
        }
    }

    bool zeDeviceCanAccessAllPeer(std::vector<ze_device_handle_t> ze_devices) {
        bool canAccessAll = true;
        ze_result_t ret;
        for (auto& ze_device_lh : ze_devices) {
            for (auto& ze_device_rh : ze_devices) {
                if (!canAccessAll)
                    break;
                if (ze_device_lh != ze_device_rh) {
                    ze_bool_t can_access;
                    XPUM_ZE_HANDLE_LOCK(ze_device_lh, ret = zeDeviceCanAccessPeer(ze_device_lh, ze_device_rh, &can_access));
                    if (ret != ZE_RESULT_SUCCESS || can_access != 1) {
                        canAccessAll = false;
                        break;
                    }                
                }
            }
        }
        return canAccessAll;   
    }

    bool isPathExist(const std::string &s) {
        struct stat buffer;
        return (stat(s.c_str(), &buffer) == 0);
    }
    
    void readConfigFile(std::string conf_file_name) {
        DiagnosticManager::thresholds.clear();
        std::string file_name = std::string(XPUM_CONFIG_DIR) + conf_file_name;
        if (!isPathExist(file_name)) {
            char exe_path[XPUM_MAX_PATH_LEN];
            ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
            if (len < 0 || len >= XPUM_MAX_PATH_LEN) {
                len = XPUM_MAX_PATH_LEN - 1;
            }            
            exe_path[len] = '\0';
            std::string current_file = exe_path;
            std::string xpum_mode = "xpum";
            if (current_file.find_last_of('/') != std::string::npos)
            xpum_mode = current_file.substr(current_file.find_last_of('/') + 1);
            if (xpum_mode != "xpu-smi") {
                file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/xpum/config/" + conf_file_name;
                if (!isPathExist(file_name))
                    file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/xpum/config/" + conf_file_name;
            } else {
                file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/xpu-smi/config/" + conf_file_name;
                if (!isPathExist(file_name))
                    file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/xpu-smi/config/" + conf_file_name;
            }
        }
        std::ifstream conf_file(file_name);
        if (conf_file.is_open()) {
            XPUM_LOG_DEBUG("read config for diagnostics and precheck from file: {}", file_name);
            std::string line;
            std::string current_device;
            while (getline(conf_file, line)) {
                line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
                if (line[0] == '#' || line.empty())
                    continue;
                auto delimiter_pos = line.find("=");
                auto name = line.substr(0, delimiter_pos);
                auto value = line.substr(delimiter_pos + 1);
                if (value.find("#") != std::string::npos)
                    value = value.substr(0, value.find("#"));
                if (conf_file_name == XPUM_GLOBAL_CONFIG_FILE) {
                    if (name == "CPU_TEMPERATURE_THRESHOLD") {
                        PrecheckManager::cpu_temperature_threshold = atoi(value.c_str());
                    } else if (name == "GPU_TEMPERATURE_THRESHOLD") {
                        DiagnosticManager::GPU_TEMPERATURE_THRESHOLD = atoi(value.c_str());
                    } else if (name == "PVC_FW_MINIMUM_VERSION") {
                        DiagnosticManager::PVC_FW_MINIMUM_VERSION = value;
                    } else if (name == "PVC_AMC_MINIMUM_VERSION") {
                        DiagnosticManager::PVC_AMC_MINIMUM_VERSION = value;
                    } else if (name == "ATSM150_FW_MINIMUM_VERSION") {
                        DiagnosticManager::ATSM150_FW_MINIMUM_VERSION = value;
                    } else if (name == "ATSM75_FW_MINIMUM_VERSION") {
                        DiagnosticManager::ATSM75_FW_MINIMUM_VERSION = value;
                    } else if (name == "MEDIA_CODER_TOOLS_PATH") {
                        if (value == "/usr/bin/" || value == "/usr/share/mfx/samples/") {
                            DiagnosticManager::MEDIA_CODER_TOOLS_PATH = value;
                        }
                    } else if (name == "MEDIA_CODER_TOOLS_1080P_FILE") {
                        DiagnosticManager::MEDIA_CODER_TOOLS_1080P_FILE = value;
                    } else if (name == "MEDIA_CODER_TOOLS_4K_FILE") {
                        DiagnosticManager::MEDIA_CODER_TOOLS_4K_FILE = value;
                    } else if (name == "KERNEL_MESSAGES_SOURCE") {
                        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                        PrecheckManager::KERNEL_MESSAGES_SOURCE = value;
                    } else if (name == "KERNEL_MESSAGES_FILE") {
                        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
                        PrecheckManager::KERNEL_MESSAGES_FILE = value;
                    } else if (name == "ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::ZE_COMMAND_QUEUE_SYNCHRONIZE_TIMEOUT = val;
                        } catch(...) { }
                    } else if (name == "MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST") {
                        try {
                            float val = std::stof(value);
                            if (val > 0 && val <= 1)
                                DiagnosticManager::MEMORY_USE_PERCENTAGE_FOR_ERROR_TEST = val;
                        } catch(...) { }
                    } else if (name == "XE_LINK_THROUGHPUT_USAGE_PERCENTAGE") {
                        try {
                            float val = std::stof(value);
                            if (val > 0 && val <= 1)
                                DiagnosticManager::XE_LINK_THROUGHPUT_USAGE_PERCENTAGE = val;
                        } catch(...) { }
                    } else if (name == "REF_XE_LINK_THROUGHPUT_ONE_TILE_DEVICE") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::REF_XE_LINK_THROUGHPUT_ONE_TILE_DEVICE = val;
                        } catch(...) { }
                    } else if (name == "REF_XE_LINK_THROUGHPUT_TWO_TILE_DEVICE") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::REF_XE_LINK_THROUGHPUT_TWO_TILE_DEVICE = val;
                        } catch(...) { }
                    } else if (name == "XE_LINK_ALL_TO_ALL_THROUGHPUT_MIN_RATIO_OF_REF") {
                        try {
                            float val = std::stof(value);
                            if (val > 0 && val <= 1)
                                DiagnosticManager::XE_LINK_ALL_TO_ALL_THROUGHPUT_MIN_RATIO_OF_REF = val;
                        } catch(...) { }
                    } else if (name == "REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X2_ONE_TILE_DEVICE") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X2_ONE_TILE_DEVICE = val;
                        } catch(...) { }
                    } else if (name == "REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X4_ONE_TILE_DEVICE") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X4_ONE_TILE_DEVICE = val;
                        } catch(...) { }
                    } else if (name == "REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X8_ONE_TILE_DEVICE") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X8_ONE_TILE_DEVICE = val;
                        } catch(...) { }
                    } else if (name == "REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X2_TWO_TILE_DEVICE") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X2_TWO_TILE_DEVICE = val;
                        } catch(...) { }
                    } else if (name == "REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X4_TWO_TILE_DEVICE") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X4_TWO_TILE_DEVICE = val;
                        } catch(...) { }
                    } else if (name == "REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X8_TWO_TILE_DEVICE") {
                        try {
                            int val = std::stoi(value);
                            if (val > 0)
                                DiagnosticManager::REF_XE_LINK_ALL_TO_ALL_THROUGHPUT_X8_TWO_TILE_DEVICE = val;
                        } catch(...) { }
                    }
                } else {
                    if (name == "NAME") {
                        current_device = value;
                    } else {
                        DiagnosticManager::thresholds[current_device][name] = atoi(value.c_str());
                    }
                }
            }
            conf_file.close();
        } else {
            XPUM_LOG_ERROR("couldn't open config file for diagnostics and precheck: {}", file_name);
        }    
    }
} // namespace xpum
