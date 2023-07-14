/* 
 *  Copyright (C) 2022-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file precheck.cpp
 */

#include "precheck.h"
#include "infrastructure/xpum_config.h"
#include "level_zero/ze_api.h"
#include <string>
#include <regex>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>
#include "helper.h"

namespace xpum {

    int PrecheckManager::cpu_temperature_threshold = 85;

    xpum_precheck_component_info_t PrecheckManager::component_driver;

    std::vector<xpum_precheck_component_info_t> PrecheckManager::component_cpus;

    std::vector<xpum_precheck_component_info_t> PrecheckManager::component_gpus;


    /**
     * some helper functions for precheck
     * 
     */

    static std::string extractLastNChars(std::string const &str, int n) {
        if ((int)str.size() < n) {
            return str;
        }

        return str.substr(str.size() - n);
    }

    static size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos) {
        std::transform(data.begin(), data.end(), data.begin(), ::tolower);
        std::transform(toSearch.begin(), toSearch.end(), toSearch.begin(), ::tolower);
        return data.find(toSearch, pos);
    }

    static void updateErrorComponentInfo(xpum_precheck_component_info_t& cinfo, xpum_precheck_component_status_t status, std::string errorDetail, 
            int errorId, std::string time = "", 
            xpum_precheck_error_category_t error_category = XPUM_PRECHECK_ERROR_CATEGORY_HARDWARE, 
            xpum_precheck_error_severity_t error_severity = XPUM_PRECHECK_ERROR_SEVERITY_CRITICAL) {
        if (cinfo.status == XPUM_PRECHECK_COMPONENT_STATUS_PASS) {
            cinfo.status = status;
            strncpy(cinfo.errorDetail, errorDetail.c_str(), errorDetail.size() + 1);
            cinfo.errorDetail[sizeof(cinfo.errorDetail)-1] = '\0';
            cinfo.errorId = errorId;
            if (errorId > 0) {
                cinfo.errorCategory = PRECHECK_ERROR_TYPE_INFO_LIST[errorId - 1].errorCategory;
                cinfo.errorSeverity = PRECHECK_ERROR_TYPE_INFO_LIST[errorId - 1].errorSeverity;
            }
            strncpy(cinfo.time, time.c_str(), time.size() + 1);
            cinfo.time[sizeof(cinfo.time)-1] = '\0';
        }
    }

    static std::string zeInitResultToString(const int result) {
        if (result == 0) {
            return "ZE_RESULT_SUCCESS";
        } else if (result == 1) {
            return "ZE_RESULT_NOT_READY";
        } else if (result == 2) {
            return "[0x78000001] ZE_RESULT_ERROR_UNINITIALIZED. Please check if you have root privileges.";
        } else if (result == 3) {
            return "[0x70020000] ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE. Maybe the metrics libraries aren't ready.";
        } else {
            return "Generic error with ze_result_t value: " + std::to_string(static_cast<int>(result));
        }
    }

    static void updateErrorComponentInfoList(std::string bdf, int cpuId, xpum_precheck_component_status_t status, std::string errorDetail, 
            int errorId, std::string time = "", 
            xpum_precheck_error_category_t errorCategory = XPUM_PRECHECK_ERROR_CATEGORY_HARDWARE, 
            xpum_precheck_error_severity_t errorSeverity = XPUM_PRECHECK_ERROR_SEVERITY_CRITICAL) {
        if (bdf != "") {
            for (auto& component_gpu : PrecheckManager::component_gpus) {
                if (extractLastNChars(component_gpu.bdf, 7) == extractLastNChars(bdf, 7) && component_gpu.status == XPUM_PRECHECK_COMPONENT_STATUS_PASS) {
                    component_gpu.status = status;
                    strncpy(component_gpu.errorDetail, errorDetail.c_str(), errorDetail.size() + 1);
                    component_gpu.errorDetail[sizeof(component_gpu.errorDetail) - 1] = '\0';
                    component_gpu.errorId = errorId;
                    if (errorId > 0) {
                        component_gpu.errorCategory = PRECHECK_ERROR_TYPE_INFO_LIST[errorId - 1].errorCategory;
                        component_gpu.errorSeverity = PRECHECK_ERROR_TYPE_INFO_LIST[errorId - 1].errorSeverity;
                    }
                    strncpy(component_gpu.time, time.c_str(), time.size() + 1);
                    component_gpu.time[sizeof(component_gpu.time)-1] = '\0';
                    break;
                }
            }
        }

        if (cpuId != -1) {
            for (auto& component_cpu : PrecheckManager::component_cpus)
                if (component_cpu.cpuId == cpuId && component_cpu.status == XPUM_PRECHECK_COMPONENT_STATUS_PASS) {
                    component_cpu.status = status;
                    strncpy(component_cpu.errorDetail, errorDetail.c_str(), errorDetail.size() + 1);
                    component_cpu.errorDetail[sizeof(component_cpu.errorDetail)-1] = '\0';  
                    component_cpu.errorId = -1;
                    component_cpu.errorCategory = errorCategory;
                    component_cpu.errorSeverity = errorSeverity;
                    strncpy(component_cpu.time, time.c_str(), time.size() + 1);
                    component_cpu.time[sizeof(component_cpu.time) - 1] = '\0';
                    break;
                }
        }
    }

    static void updateErrorLogLine(std::string line, ErrorPattern error_pattern) {
        std::regex regTime("\\d{2}:\\d{2}:\\d{2}");
        std::smatch match;
        std::string time;
        if(std::regex_search(line, match, regTime)) {
            time = match.prefix();
            time += match.str();
            line = match.suffix();
            line = line.substr(1);
        }

        std::string bdf;
        for (auto& component_gpu : PrecheckManager::component_gpus) {
            if (line.find(extractLastNChars(component_gpu.bdf, 7)) != std::string::npos) {
                bdf = component_gpu.bdf;
                break;
            }
        }

        // kernel issue but not related with gpu driver will not update driver info
        if (error_pattern.target_type == XPUM_PRECHECK_COMPONENT_TYPE_DRIVER) {
            if (bdf == "") {
                updateErrorComponentInfo(PrecheckManager::component_driver, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, line, error_pattern.error_id, time);
            } else {
                updateErrorComponentInfoList(bdf, -1, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, line, error_pattern.error_id, time);
            }
        } else if (error_pattern.target_type == XPUM_PRECHECK_COMPONENT_TYPE_CPU) {
            auto pos = line.find("CPU ");
            if (pos != std::string::npos) {
                std::string temp_str = line.substr(pos + 1);
                int cpu_id = std::stoi(temp_str);
                if (!PrecheckManager::component_cpus.empty())
                    updateErrorComponentInfoList("", cpu_id / (PROCESSOR_COUNT / PrecheckManager::component_cpus.size()), XPUM_PRECHECK_COMPONENT_STATUS_FAIL, line, -1, time, error_pattern.error_category, error_pattern.error_severity);
            }
        } else if (bdf != "") {
            updateErrorComponentInfoList(bdf, -1, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, line, error_pattern.error_id, time);
        }
    }

    static void scanErrorLogLinesByFile(std::string print_log_cmd, std::unordered_map<std::string, std::vector<ErrorPattern>>& key_to_error_patterns) {
        FILE* f = popen(print_log_cmd.c_str(), "r");
        char c_line[1024];
        std::smatch match;
        while (fgets(c_line, 1024, f) != NULL) {
            size_t len = strnlen(c_line, 1024);
            if (len > 0 && c_line[len-1] == '\n') {
                c_line[--len] = '\0';
            }
            std::string line(c_line);
            std::string target_found;
            for (auto tw : targeted_words)
                if (findCaseInsensitive(line, tw, 0) != std::string::npos) {
                    target_found = tw;
                    break;
                }
            if (target_found.empty())
                continue;
            for (auto error_pattern : key_to_error_patterns[target_found]) {
                std::regex re(error_pattern.pattern, std::regex_constants::icase);
                if(std::regex_search(line, match, re)) {
                    if (error_pattern.filter.size() > 0 && line.find(error_pattern.filter) != std::string::npos)
                        continue;
                    updateErrorLogLine(line, error_pattern);
                }
            }
        }
        pclose(f);
    }

    static void scanErrorLogLines(std::vector<ErrorPattern> error_patterns, std::string since_time) {
        std::unordered_map<std::string, std::vector<ErrorPattern>> key_to_error_patterns;
        for (auto& key : targeted_words) {
            std::vector<ErrorPattern> patterns;
            for (auto& ep : error_patterns) {
                if (findCaseInsensitive(ep.pattern, key, 0) != std::string::npos) {
                    patterns.push_back(ep);
                }
            }
            key_to_error_patterns[key] = patterns;
        }

        std::string print_log_cmd = "journalctl -q -b 0 --dmesg";
        if (!since_time.empty())
            print_log_cmd += " --since \"" + since_time + "\"";
        scanErrorLogLinesByFile(print_log_cmd, key_to_error_patterns);
    }

    static void doPreCheckDriver() {
        // GPU level-zero driver
        std::string level0_driver_error_info;
        bool dependency_issue = false;
        pid_t pid = fork();
        if (pid == 0) {
            execlp("echo", "echo", "-n", NULL);
            putenv(const_cast<char*>("ZES_ENABLE_SYSMAN=1"));
            putenv(const_cast<char*>("ZET_ENABLE_METRICS=1"));
            int init_status = zeInit(0);
            if (init_status == 0 || init_status == 1)
                exit(init_status);
            else if (init_status == 0x78000001)
                exit(2);
            else if (init_status == 0x70020000)
                exit(3);
            else
                exit(255);
        }
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int init_res = WEXITSTATUS(status);
            if (init_res != 0) {
                level0_driver_error_info = "Failed to init level zero: " + zeInitResultToString(init_res);
                if (init_res == 3)
                    dependency_issue = true;
            } 
        } else {
            level0_driver_error_info = "Failed to init level zero due to GPU driver";
        }

        // GPU i915 driver
        bool is_i915_loaded = false;
        FILE* f = popen("modinfo -n i915 2>/dev/null", "r");
        char c_line[1024];
        while (fgets(c_line, 1024, f) != NULL) {
            std::string line(c_line);
            if (line.find("i915.ko") != std::string::npos) {
                is_i915_loaded = true;
            }
        }
        pclose(f);

        if (!is_i915_loaded) {
            updateErrorComponentInfo(PrecheckManager::component_driver, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, "i915 not loaded", XPUM_I915_NOT_LOADED);
        } else if (!level0_driver_error_info.empty()) {
            updateErrorComponentInfo(PrecheckManager::component_driver, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, level0_driver_error_info, dependency_issue? XPUM_LEVEL_ZERO_METRICS_INIT_ERROR : XPUM_LEVEL_ZERO_INIT_ERROR);
        }
    }

    static void doPreCheckGuCHuCWedgedPCIe(std::vector<std::string> gpu_ids, std::vector<std::string> gpu_bdfs, bool is_atsm_platform) {
        // GPU GuC HuC i915 wedged
        char path[PATH_MAX];
        int gpu_id_index = 0;
        for (auto gpu_id : gpu_ids) {
            std::string line;
            snprintf(path, PATH_MAX, "/sys/kernel/debug/dri/%s/gt0/uc/guc_info", gpu_id.c_str());
            bool is_guc_running = false;
            std::ifstream guc_info_file(path);
            if (guc_info_file.good()) {
                while(std::getline(guc_info_file, line)) {
                    if (!line.empty() && line.find("status: ") != std::string::npos) {
                        if (line.find("RUNNING") != std::string::npos) {
                            is_guc_running = true;
                            break;
                        }
                    }
                }
                if (!is_guc_running) {
                    updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, "GuC is not running", XPUM_GUC_NOT_RUNNING);
                }
            }
            guc_info_file.close();
            if (is_atsm_platform) {
                snprintf(path, PATH_MAX, "/sys/kernel/debug/dri/%s/gt0/uc/huc_info", gpu_id.c_str());
                bool is_huc_running = false;
                bool is_huc_disabled = false;
                std::ifstream huc_info_file(path);
                if (huc_info_file.good()) {
                    while(std::getline(huc_info_file, line)) {
                        if (!line.empty() && line.find("HuC disabled") != std::string::npos) {
                            is_huc_disabled = true;
                            break;
                        }
                        if (!line.empty() && line.find("status: ") != std::string::npos) {
                            if (line.find("RUNNING") != std::string::npos) {
                                is_huc_running = true;
                                break;
                            }
                        }
                    }
                    if (!is_huc_running) {
                        if (is_huc_disabled)
                            updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, "HuC is disabled", XPUM_HUC_DISABLED);
                        else
                            updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, "HuC is not running", XPUM_HUC_NOT_RUNNING);
                    }
                }
                huc_info_file.close();
            }

            snprintf(path, PATH_MAX, "/sys/kernel/debug/dri/%s/i915_wedged", gpu_id.c_str());
            bool is_i915_wedged = false;
            std::ifstream i915_wedged_file(path);
            if (i915_wedged_file.good()) {
                while(std::getline(i915_wedged_file, line)) {
                    if (!line.empty() && std::stoi(line) != 0) {
                        is_i915_wedged = true;
                        break;
                    }
                }
                if (is_i915_wedged) {
                    updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, "i915 wedged", XPUM_I915_ERROR);
                }
            }
            i915_wedged_file.close();

            gpu_id_index += 1;
        }

        // PCIe error
        std::vector<std::string> pci_erros = {"TAbort+", "<TAbort+", "<MAbort+", ">SERR+", "<PERR+",
                                                "CorrErr+", "NonFatalErr+", "FatalErr+"};
        for (auto bdf : gpu_bdfs) {
            bool is_pcie_ok = true;
            std::string cmd = "lspci -vvvvv -s " + bdf + " 2>/dev/null";
            FILE* f = popen(cmd.c_str(), "r");
            char c_line[1024];
            while (fgets(c_line, 1024, f) != NULL) {
                std::string line(c_line);
                if (line.find("DevSta: ") != std::string::npos || line.find("Status: ") != std::string::npos) {
                    for (auto pci_error : pci_erros) {
                        if (line.find(pci_error) != std::string::npos) {
                            is_pcie_ok = false;
                            break;
                        }
                    }
                    if (!is_pcie_ok)
                        break;
                }
            }
            pclose(f);
            if (!is_pcie_ok) {
                updateErrorComponentInfoList(bdf, -1, XPUM_PRECHECK_COMPONENT_STATUS_FAIL, "PCIe error", XPUM_PCIE_ERROR);
            }
        }
    }

    bool isPhysicalFunctionDevice(std::string pci_addr) {
        DIR *dir;
        struct dirent *ent;
        std::stringstream ss;
        ss << "/sys/bus/pci/devices/" << pci_addr;
        dir = opendir(ss.str().c_str());
        
        if (dir == NULL) {
            return false;
        }
        while ((ent = readdir(dir)) != NULL) {
            /*
                Containing `physfn` which links to the PF it belongs to
                means it's a VF, otherwise it's a PF.
            */
            if (strstr(ent->d_name, "physfn") != NULL) {
                closedir(dir);
                return false;
            }
        }
        closedir(dir);
        return true;
    }

    bool isATSMPlatform(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str.find("56c0") != std::string::npos || str.find("56c1") != std::string::npos;
    }

    static void toCheck(bool onlyGPU, std::string sinceTime, bool getComponentCount = false) {
        // reset precheck info
        PrecheckManager::component_driver.componentType = XPUM_PRECHECK_COMPONENT_TYPE_DRIVER;
        PrecheckManager::component_driver.status = XPUM_PRECHECK_COMPONENT_STATUS_PASS;
        PrecheckManager::component_driver.errorId = 0;
        PrecheckManager::component_driver.errorDetail[0] = '\0';
        PrecheckManager::component_driver.time[0] = '\0';
        PrecheckManager::component_driver.bdf[0] = '\0';
        PrecheckManager::component_gpus.clear();
        PrecheckManager::component_cpus.clear();

        bool has_privilege = (getuid() == 0);
        char path[PATH_MAX];
        DIR *pdir = NULL;
        struct dirent *pdirent = NULL;

        bool is_atsm_platform = true;
        std::vector<std::string> gpu_ids;
        std::vector<std::string> gpu_bdfs;
        pdir = opendir("/sys/class/drm");
        if (pdir != NULL) {
            char uevent[1024];
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
                snprintf(path, PATH_MAX, "/sys/class/drm/%s/device/uevent", pdirent->d_name);
                int fd = open(path, O_RDONLY);
                if (fd < 0) {
                    continue;
                }
                int cnt = read(fd, uevent, 1024);
                if (cnt < 0 || cnt >= 1024) {
                    continue;
                }
                close(fd);
                uevent[cnt] = 0;
                std::string str(uevent);
                std::string key = "PCI_ID=8086:";
                auto pos = str.find(key); 
                if (pos != std::string::npos) {
                    std::string device_id = str.substr(pos + key.length(), 4);
                    is_atsm_platform = isATSMPlatform(device_id);
                    std::string bdf_key = "PCI_SLOT_NAME=";
                    auto bdf_pos = str.find(bdf_key); 
                    if (bdf_pos != std::string::npos) {
                        std::string bdf = str.substr(bdf_pos + bdf_key.length(), 12);
                        if (isPhysicalFunctionDevice(bdf)) {
                            gpu_ids.push_back(std::string(pdirent->d_name).substr(4));
                            gpu_bdfs.push_back(bdf);
                            xpum_precheck_component_info_t gpu_component;
                            gpu_component.componentType = XPUM_PRECHECK_COMPONENT_TYPE_GPU;
                            gpu_component.status = has_privilege ? XPUM_PRECHECK_COMPONENT_STATUS_PASS : XPUM_PRECHECK_COMPONENT_STATUS_UNKNOWN;
                            gpu_component.errorDetail[0] = '\0';
                            gpu_component.time[0] = '\0';
                            strncpy(gpu_component.bdf, bdf.c_str(), bdf.size() + 1);
                            gpu_component.bdf[sizeof(gpu_component.bdf) - 1] = '\0';
                            PrecheckManager::component_gpus.push_back(gpu_component);
                        }
                    }
                }
            }
            closedir(pdir);
        }

        if (gpu_bdfs.empty()) {
            std::string cmd = "lspci -D|grep -i Display|grep -i Intel";
            FILE* f = popen(cmd.c_str(), "r");
            char c_line[1024];
            int gpu_id = 0;
            while (fgets(c_line, 1024, f) != NULL) {
                std::string line(c_line);
                is_atsm_platform = isATSMPlatform(line);
                std::string bdf = line.substr(0, 12);
                if (isPhysicalFunctionDevice(bdf)) {
                    gpu_ids.push_back(std::to_string(gpu_id));
                    gpu_bdfs.push_back(bdf);
                    xpum_precheck_component_info_t gpu_component;
                    gpu_component.componentType = XPUM_PRECHECK_COMPONENT_TYPE_GPU;
                    gpu_component.status = has_privilege ? XPUM_PRECHECK_COMPONENT_STATUS_PASS : XPUM_PRECHECK_COMPONENT_STATUS_UNKNOWN;
                    gpu_component.errorDetail[0] = '\0';
                    gpu_component.time[0] = '\0';
                    strncpy(gpu_component.bdf, bdf.c_str(), bdf.size() + 1);
                    gpu_component.bdf[sizeof(gpu_component.bdf) - 1] = '\0';
                    gpu_id++;
                }
            }
            pclose(f);
        }
        
        if (!onlyGPU) {
            pdir = opendir("/sys/class/thermal");
            if (pdir != NULL) {
                char thermal_type[1024];
                char thermal_value[1024];
                int pk_id = 0;
                while ((pdirent = readdir(pdir)) != NULL) {
                    if (pdirent->d_name[0] == '.') {
                        continue;
                    }
                    if (strncmp(pdirent->d_name, "thermal_zone", 12) != 0) {
                        continue;
                    }
                    snprintf(path, PATH_MAX, "/sys/class/thermal/%s/type", pdirent->d_name);
                    int fd = open(path, O_RDONLY);
                    if (fd < 0) {
                        continue;
                    }
                    int cnt = read(fd, thermal_type, 1024);
                    if (cnt < 0 || cnt >= 1024) {
                        continue;
                    }
                    close(fd);
                    thermal_type[cnt] = 0;
                    if (strncmp(thermal_type, "x86_pkg_temp", 12) == 0) {
                        snprintf(path, PATH_MAX, "/sys/class/thermal/%s/temp",
                                pdirent->d_name);
                        fd = open(path, O_RDONLY);
                        if (fd < 0) {
                            continue;
                        }
                        int cnt = read(fd, thermal_value, 1024);
                        thermal_value[cnt] = 0;
                        close(fd);
                        int val = std::stoi(thermal_value)/1000;
                        xpum_precheck_component_info_t cpu_component;
                        cpu_component.componentType = XPUM_PRECHECK_COMPONENT_TYPE_CPU;
                        cpu_component.status = has_privilege ? XPUM_PRECHECK_COMPONENT_STATUS_PASS : XPUM_PRECHECK_COMPONENT_STATUS_UNKNOWN;
                        cpu_component.errorId = -1;
                        cpu_component.bdf[0] = '\0';
                        cpu_component.time[0] = '\0';

                        cpu_component.cpuId = pk_id;
                        if (val >= PrecheckManager::cpu_temperature_threshold) {
                            cpu_component.status = XPUM_PRECHECK_COMPONENT_STATUS_FAIL;
                            cpu_component.errorCategory = XPUM_PRECHECK_ERROR_CATEGORY_HARDWARE;
                            cpu_component.errorSeverity = XPUM_PRECHECK_ERROR_SEVERITY_CRITICAL;
                            std::string errorDetail = "Temperature is high (" + std::to_string(val) + " Celsius Degree)";
                            strncpy(cpu_component.errorDetail, errorDetail.c_str(), errorDetail.size() + 1);
                            cpu_component.errorDetail[sizeof(cpu_component.errorDetail) - 1] = '\0';
                        } else {
                            cpu_component.errorDetail[0] = '\0';
                        }
                        PrecheckManager::component_cpus.push_back(cpu_component);
                        pk_id += 1;
                    }
                }
                closedir(pdir);
            }
        }
        
        if (getComponentCount) {
            return;
        }
        
        if (Configuration::XPUM_MODE != "xpum") {
            doPreCheckDriver();
        }

        doPreCheckGuCHuCWedgedPCIe(gpu_ids, gpu_bdfs, is_atsm_platform);
        scanErrorLogLines(error_patterns, sinceTime);
    }

    xpum_result_t PrecheckManager::precheck(xpum_precheck_component_info_t resultList[], int *count, bool onlyGPU, const char *sinceTime) {
        if (sinceTime != nullptr) {
            std::string sinceTimeStr = std::string(sinceTime);
            if (sinceTimeStr.size() > 0) {
                std::string test_cmd = "journalctl --since \"" + sinceTimeStr + "\" -n 1 >/dev/null 2>&1";
                if (system(test_cmd.c_str()) != 0) {
                    return XPUM_PRECHECK_INVALID_SINCETIME;
                }
            }
        }

        if (resultList == nullptr) {
            toCheck(false, "", true);
            int val = PrecheckManager::component_gpus.size() + 1;
            if (!onlyGPU)
                val += PrecheckManager::component_cpus.size();
            *count = val;
            return XPUM_OK;
        }
        readConfigFile(); 
        toCheck(onlyGPU, sinceTime);
        int val = PrecheckManager::component_gpus.size() + 1;
        if (!onlyGPU)
            val += PrecheckManager::component_cpus.size();
        if (*count < val) {
            *count = val;
            return XPUM_BUFFER_TOO_SMALL;
        }
        *count = val;
        std::vector<xpum_precheck_component_info_t> targets;
        targets.push_back(PrecheckManager::component_driver);
        if (!onlyGPU) {
            targets.insert(targets.end(), PrecheckManager::component_cpus.begin(), PrecheckManager::component_cpus.end());
        }
        targets.insert(targets.end(), PrecheckManager::component_gpus.begin(), PrecheckManager::component_gpus.end());
        for (int i = 0; i < *count; i++) {
            resultList[i].componentType = targets[i].componentType;
            strncpy(resultList[i].bdf, targets[i].bdf, sizeof(targets[i].bdf));
            resultList[i].cpuId = targets[i].cpuId;
            resultList[i].status = targets[i].status;
            strncpy(resultList[i].time, targets[i].time, sizeof(targets[i].time));
            resultList[i].errorId = targets[i].errorId;
            resultList[i].errorCategory = targets[i].errorCategory;
            resultList[i].errorSeverity = targets[i].errorSeverity;
            strncpy(resultList[i].errorDetail, targets[i].errorDetail, sizeof(targets[i].errorDetail));

        }
        return XPUM_OK;
    }
    
    xpum_result_t PrecheckManager::getPrecheckErrorList(xpum_precheck_error_t resultList[], int *count) {
        if (resultList == nullptr) {
            *count = XPUM_MAX_PRECHECK_ERROR_TYPE_INFO_LIST_SIZE;
            return XPUM_OK;
        } else if (*count < XPUM_MAX_PRECHECK_ERROR_TYPE_INFO_LIST_SIZE) {
            return XPUM_BUFFER_TOO_SMALL; 
        }
        for (int i = 0; i < XPUM_MAX_PRECHECK_ERROR_TYPE_INFO_LIST_SIZE; i++) {
            resultList[i].errorId = (uint32_t)PRECHECK_ERROR_TYPE_INFO_LIST[i].errorType;
            resultList[i].errorType = PRECHECK_ERROR_TYPE_INFO_LIST[i].errorType;
            resultList[i].errorCategory = PRECHECK_ERROR_TYPE_INFO_LIST[i].errorCategory;
            resultList[i].errorSeverity = PRECHECK_ERROR_TYPE_INFO_LIST[i].errorSeverity;
        }
        *count = XPUM_MAX_PRECHECK_ERROR_TYPE_INFO_LIST_SIZE;
        return XPUM_OK;
    }
}