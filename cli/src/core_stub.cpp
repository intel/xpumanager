/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file core_stub.cpp
 */

#include "core_stub.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>
#include <dirent.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <regex>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unordered_set>

#include "config.h"
#include "xpum_structs.h"
#include "local_functions.h"

namespace xpum::cli {

std::string CoreStub::isotimestamp(uint64_t t, bool withoutDate) {
    time_t seconds = (long)t / 1000;
    int milli_seconds = t % 1000;
    tm* tm_p = localtime(&seconds);
    char buf[50];
    char milli_buf[10];
    sprintf(milli_buf, "%03d", milli_seconds);
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
    {XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE"},
    {XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE, "XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE"},
    {XPUM_STATS_GPU_REQUEST_FREQUENCY, "XPUM_STATS_GPU_REQUEST_FREQUENCY"},
    {XPUM_STATS_MEMORY_TEMPERATURE, "XPUM_STATS_MEMORY_TEMPERATURE"},
    {XPUM_STATS_FREQUENCY_THROTTLE, "XPUM_STATS_FREQUENCY_THROTTLE"},
    {XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU, "XPUM_STATS_FREQUENCY_THROTTLE_REASON_GPU"},
    {XPUM_STATS_PCIE_READ_THROUGHPUT, "XPUM_STATS_PCIE_READ_THROUGHPUT"},
    {XPUM_STATS_PCIE_WRITE_THROUGHPUT, "XPUM_STATS_PCIE_WRITE_THROUGHPUT"},
    {XPUM_STATS_PCIE_READ, "XPUM_STATS_PCIE_READ"},
    {XPUM_STATS_PCIE_WRITE, "XPUM_STATS_PCIE_WRITE"},
    {XPUM_STATS_ENGINE_UTILIZATION, "XPUM_STATS_ENGINE_UTILIZATION"}};

std::string CoreStub::metricsTypeToString(xpum_stats_type_t metricsType) {
    for (auto item : metricsTypeArray) {
        if (item.key == metricsType) {
            return item.keyStr;
        }
    }
    return std::to_string(metricsType);
}

std::string CoreStub::getCardUUID(const std::string& rawUUID) {
    std::string::size_type start = rawUUID.find_last_of('-');
    if (start != std::string::npos) {
        return rawUUID.substr(start + 1, rawUUID.length());
    } else {
        return rawUUID;
    }
}

std::string CoreStub::schedulerModeToString(int mode) {
    std::string ret = "null"; //"SCHEDULER_MODE_NULL";
    switch (mode) {
        case 0:
            ret = "timeout"; //"SCHEDULER_MODE_TIMEOUT";
            break;
        case 1:
            ret = "timeslice"; //"SCHEDULER_MODE_TIMESLICE";
            break;
        case 2:
            ret = "exclusive"; //"SCHEDULER_MODE_EXCLUSIVE";
            break;
        case 3:
            ret = "debug"; //"SCHEDULER_MODE_DEBUG";
            break;
        default:
            break;
    }
    return ret;
}
std::string CoreStub::standbyModeToString(int mode) {
    std::string ret = "null"; //"STANDBY_MODE_NULL";
    switch (mode) {
        case 0:
            ret = "default"; //"STANDBY_MODE_DEFAULT";
            break;
        case 1:
            ret = "never"; //"STANDBY_MODE_NEVER";
            break;
        default:
            break;
    }
    return ret;
}

std::string CoreStub::deviceFunctionTypeEnumToString(xpum_device_function_type_t type) {
    switch (type) {
        case DEVICE_FUNCTION_TYPE_VIRTUAL:
            return "virtual";
        case DEVICE_FUNCTION_TYPE_PHYSICAL:
            return "physical";
        default:
            return "unknown";
    }
}

static std::string syslog_file_name = "/var/log/syslog";
static std::string messages_file_name = "/var/log/messages";
static int cpu_temperature_threshold = 85;
static int detect_number_of_last_logs = 500;

ComponentInfo component_driver;
std::vector<ComponentInfo> component_cpus;
std::vector<ComponentInfo> component_gpus;
std::unordered_set<std::string> error_gpus;
std::unordered_set<int> error_cpus;

static void updateErrorComponentInfoList(std::string bdf, int id, std::string status, int category, int severity, std::string time = "") {
    if (bdf != "") {
        for (auto& component_gpu : component_gpus) {
            if (extractLastNChars(component_gpu.bdf, 7) == extractLastNChars(bdf, 7) && component_gpu.status == "Pass") {
                component_gpu.status = status;
                component_gpu.category = category;
                component_gpu.severity = severity;
                component_gpu.time = time;
                error_gpus.insert(bdf);
                break;
            }
        }
    }

    if (id != -1) {
        for (auto& component_cpu : component_cpus)
            if (component_cpu.id == id && component_cpu.status == "Pass") {
                component_cpu.status = status;
                component_cpu.category = category;
                component_cpu.severity = severity;
                component_cpu.time = time;
                error_cpus.insert(id);
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
    for (auto& component_gpu : component_gpus) {
        if (line.find(extractLastNChars(component_gpu.bdf, 7)) != std::string::npos) {
            bdf = component_gpu.bdf;
            break;
        }
    }

    if (error_pattern.target_type == COMPONET_TYE_DRIVER) {
        if (bdf == "") {
            updateErrorComponentInfo(component_driver, line, error_pattern.error_category, error_pattern.error_severity, time);
        } else {
            updateErrorComponentInfoList(bdf, -1, line, error_pattern.error_category, error_pattern.error_severity, time);
        }
    } else if (error_pattern.target_type == COMPONET_TYE_CPU) {
        auto pos = line.find("CPU ");
        if (pos != std::string::npos) {
            std::string temp_str = line.substr(pos + 1);
            int cpu_id = std::stoi(temp_str);
            if (!component_cpus.empty())
                updateErrorComponentInfoList("", cpu_id / (processor_count / component_cpus.size()), line, error_pattern.error_category, error_pattern.error_severity, time);
        }
    } else if (bdf != "") {
        updateErrorComponentInfoList(bdf, -1, line, error_pattern.error_category, error_pattern.error_severity, time);
    } else {
        // kernel issue but not related with gpu driver, so not update driver info
        // updateErrorComponentInfo(component_driver, line, error_pattern.error_category, error_pattern.error_severity, time);
    }
}

static void scanErrorLogLinesByFile(std::string print_log_cmd, std::vector<ErrorPattern> error_patterns) {
    FILE* f = popen(print_log_cmd.c_str(), "r");
    char c_line[1024];
    std::smatch match;
    while (fgets(c_line, 1024, f) != NULL) {
        size_t len = strnlen(c_line, 1024);
        if (len > 0 && c_line[len-1] == '\n') {
            c_line[--len] = '\0';
        }
        // skip error components
        if (error_cpus.size() == component_cpus.size() || component_cpus.size() == 0) {
            error_patterns.erase(std::remove_if(error_patterns.begin(), error_patterns.end(),
                [](ErrorPattern e) { return e.target_type == COMPONET_TYE_CPU;}), error_patterns.end());
        }
        if (error_gpus.size() == component_gpus.size() || component_gpus.size() == 0) {
            error_patterns.erase(std::remove_if(error_patterns.begin(), error_patterns.end(),
                [](ErrorPattern e) { return e.target_type == COMPONET_TYE_GPU;}), error_patterns.end());
        }
        if (component_driver.severity > 0) {
            error_patterns.erase(std::remove_if(error_patterns.begin(), error_patterns.end(),
                [](ErrorPattern e) { return e.target_type == COMPONET_TYE_DRIVER;}), error_patterns.end());
        }
        if (error_patterns.empty()) {
            break;
        }

        std::string line(c_line);
        // skip useless los for speed up
        std::vector<std::string> targeted_words = {"i915", "drm", "mce", "mca", "caterr", "GUC",
                                            "initialized", "blocked", "Hardware", "perf",
                                            "memory", "HANG", "segfault", "panic",  "terminated",
                                            "traps", "catastrophic", "PCIe", "uc failed"};
        bool target_found = false;
        for (auto tw : targeted_words)
            if (findCaseInsensitive(line, tw) != std::string::npos) {
                target_found = true;
                break;
            }
        if (!target_found)
            continue;
        for (auto error_pattern : error_patterns) {
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

static void scanErrorLogLines(std::vector<ErrorPattern> error_patterns) {
    std::string print_log_cmd = "journalctl -q -b 0 --dmesg";
    scanErrorLogLinesByFile(print_log_cmd, error_patterns);

    std::ifstream syslog_file(syslog_file_name);
    if (syslog_file.good()) {
        print_log_cmd = "cat " + syslog_file_name;
        if (detect_number_of_last_logs > 0)
            print_log_cmd += " | tail -n " + std::to_string(detect_number_of_last_logs);
        scanErrorLogLinesByFile(print_log_cmd, error_patterns);
    }
    syslog_file.close();

    std::ifstream messages_file(messages_file_name);
    if (messages_file.good()) {
        print_log_cmd = "cat " + messages_file_name;
        if (detect_number_of_last_logs > 0)
            print_log_cmd += " | tail -n " + std::to_string(detect_number_of_last_logs);
        scanErrorLogLinesByFile(print_log_cmd, error_patterns);
    }
    messages_file.close();
}

static void readConfigFile() {
    std::string file_name = std::string(XPUM_CONFIG_DIR) + std::string("diagnostics.conf");
    struct stat buffer;
    if (stat(file_name.c_str(), &buffer) != 0) {
        char exe_path[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
        exe_path[len] = '\0';
        std::string current_file = exe_path;
#ifndef DAEMONLESS
        file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/xpum/config/" + std::string("diagnostics.conf");
        if (stat(file_name.c_str(), &buffer) != 0)
            file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/xpum/config/" + std::string("diagnostics.conf");
#else
        file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib/xpu-smi/config/" + std::string("diagnostics.conf");
        if (stat(file_name.c_str(), &buffer) != 0)
            file_name = current_file.substr(0, current_file.find_last_of('/')) + "/../lib64/xpu-smi/config/" + std::string("diagnostics.conf");
#endif
    }

    std::ifstream conf_file(file_name);
    if (conf_file.is_open()) {
        std::string line;
        while (getline(conf_file, line)) {
            line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
            if (line[0] == '#' || line.empty())
                continue;
            auto delimiter_pos = line.find("=");
            auto name = line.substr(0, delimiter_pos);
            auto value = line.substr(delimiter_pos + 1);
            if (value.find("#") != std::string::npos)
                value = value.substr(0, value.find("#"));
            if (name == "SYSLOG_FILE_NAME") {
                syslog_file_name = value;
            } else if (name == "MESSAGES_FILE_NAME") {
                messages_file_name = value;
            }else if (name == "CPU_TEMPERATURE_THRESHOLD") {
                cpu_temperature_threshold = atoi(value.c_str());
            } else if (name == "DETECT_NUMBER_OF_LAST_LOGS") {
                detect_number_of_last_logs = atoi(value.c_str());
            }
        }
    }
}

static void doPreCheckDriver() {
    component_driver = {COMPONET_TYE_DRIVER, "Pass", 0, 0, -1, "", ""};
    // GPU level-zero driver
    std::string level0_driver_error_info;
    const std::string level_zero_loader_lib_path{"libze_loader.so.1"};
    void *handle = dlopen(level_zero_loader_lib_path.c_str(), RTLD_LAZY);
    if (!handle) {
        level0_driver_error_info  = "Not found level zero library: libze_loader";
    }

    int (*ze_init) (int) = (int (*)(int))dlsym(handle, "zeInit");
    if (!ze_init) {
        level0_driver_error_info  = "Not found zeInit in libze_loader";
    }

    bool dependency_issue = false;
    pid_t pid = fork();
    if (pid == 0) {
        putenv(const_cast<char*>("ZES_ENABLE_SYSMAN=1"));
        putenv(const_cast<char*>("ZET_ENABLE_METRICS=1"));
        int init_status = ze_init(0);
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
    dlclose(handle);

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
        updateErrorComponentInfo(component_driver, "i915 not loaded", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL);
    } else if (!level0_driver_error_info.empty()) {
        updateErrorComponentInfo(component_driver, level0_driver_error_info, 
            ERROR_CATEGORY_UMD, dependency_issue? ERROR_SEVERITY_HIGH : ERROR_SEVERITY_CIRTICAL);
    }
}

static void doPreCheckGuCHuCWedgedPCIe(std::vector<std::string> gpu_ids, std::vector<std::string> gpu_bdfs) {
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
                updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, "GuC is disabled", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL);
            }
        }
        guc_info_file.close();
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
                    updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, "HuC is disabled", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_MEDIUM);
                else
                    updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, "HuC is not running", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL);

            }
        }
        huc_info_file.close();

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
                updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, "i915 wedged", ERROR_CATEGORY_KMD, ERROR_SEVERITY_CIRTICAL);
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
            updateErrorComponentInfoList(bdf, -1, "PCIe error", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL);
        }
    }
}

static void doPreCheck(bool onlyGPU) {
    bool has_privilege = (getuid() == 0);
    readConfigFile();
    char path[PATH_MAX];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;

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
            close(fd);
            uevent[cnt] = 0;
            std::string str(uevent);
            std::string key = "PCI_ID=8086:";
            auto pos = str.find(key); 
            if (pos != std::string::npos) {
                gpu_ids.push_back(std::string(pdirent->d_name).substr(4));
                std::string bdf_key = "PCI_SLOT_NAME=";
                auto bdf_pos = str.find(bdf_key); 
                if (bdf_pos != std::string::npos) {
                    gpu_bdfs.push_back(str.substr(bdf_pos + bdf_key.length(), 12));
                    component_gpus.push_back({COMPONET_TYE_GPU, has_privilege ? "Pass" : "Unknown", 0, 0, -1, gpu_bdfs.back(), ""});
                }
            }
        }
        closedir(pdir);
    }

    if (gpu_bdfs.empty()) {
        std::string cmd = "lspci|grep -i Display|grep -i Intel|cut -d ' ' -f 1";
        FILE* f = popen(cmd.c_str(), "r");
        char c_line[1024];
        int gpu_id = 0;
        while (fgets(c_line, 1024, f) != NULL) {
            std::string line(c_line);
            gpu_ids.push_back(std::to_string(gpu_id));
            gpu_bdfs.push_back(line.substr(0, 7));
            component_gpus.push_back({COMPONET_TYE_GPU, has_privilege ? "Pass" : "Unknown", 0, 0, -1, gpu_bdfs.back(), ""});
            gpu_id++;
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
                    if (val > cpu_temperature_threshold) {
                        component_cpus.push_back({COMPONET_TYE_CPU, "Temperature is high (" + std::to_string(val) + " Celsius Degree)", 
                            ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL, pk_id, "", ""});
                    } else {
                        component_cpus.push_back({COMPONET_TYE_CPU, has_privilege ? "Pass" : "Unknown", 0, 0, pk_id, "", ""});
                    }
                    pk_id += 1;
                }
            }
            closedir(pdir);
        }
    }

    doPreCheckDriver();
    scanErrorLogLines(error_patterns);
    doPreCheckGuCHuCWedgedPCIe(gpu_ids, gpu_bdfs);
}

std::unique_ptr<nlohmann::json> CoreStub::getPreCheckInfo(bool onlyGPU, bool rawJson) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    doPreCheck(onlyGPU);
    std::vector<nlohmann::json> component_json_list;

    auto component_driver_json = nlohmann::json();
    component_driver_json["type"] = componentTypeToStr(component_driver.type);
    if (rawJson) {
        component_driver_json["status"] = component_driver.status;
        if (component_driver.severity > 0)
            component_driver_json["severity"] = errorSeverityToStr(component_driver.severity);
    } else {
        std::vector<nlohmann::json> component_details;
        auto status = nlohmann::json();
        status["field_value"] = "Status: " + component_driver.status;
        component_details.push_back(status);
        if (component_driver.severity > 0) {
            auto severity = nlohmann::json();
            severity["field_value"] = "Severity: " + errorSeverityToStr(component_driver.severity);
            component_details.push_back(severity);
        }
        component_driver_json["error_details"] = component_details;
    }

    component_json_list.push_back(component_driver_json);

    std::vector<ComponentInfo> targets;
    if (onlyGPU) {
        targets = std::move(component_gpus);
    } else {
        targets = std::move(component_cpus);
        targets.insert(targets.end(), component_gpus.begin(), component_gpus.end());
    }
    // list info 
    for (auto& component : targets) {
        auto component_json = nlohmann::json();
        component_json["type"] = componentTypeToStr(component.type);
        if (rawJson) {
            if (component.type == COMPONET_TYE_CPU)
                component_json["id"] = component.id;
            else
                component_json["bdf"] = component.bdf;
            component_json["status"] = component.status;
            if (component.time != "")
                component_json["time"] = component.time;
            if (component.severity > 0)
                component_json["severity"] = errorSeverityToStr(component.severity);
        } else {
            std::vector<nlohmann::json> component_details;
            if (component.type == COMPONET_TYE_CPU) {
                auto id = nlohmann::json();
                id["field_value"] = "CPU ID: " + std::to_string(component.id);
                component_details.push_back(id);
            } else {
                auto bdf = nlohmann::json();
                bdf["field_value"] = "BDF: " + component.bdf;
                component_details.push_back(bdf);
            }
            auto status = nlohmann::json();
            status["field_value"] = "Status: " + component.status;
            component_details.push_back(status);
            if (component.time != "") {
                auto time = nlohmann::json();
                time["field_value"] = "Time: " + component.time;
                component_details.push_back(time);
            }
            if (component.severity > 0) {
                auto severity = nlohmann::json();
                severity["field_value"] = "Severity: " + errorSeverityToStr(component.severity);
                component_details.push_back(severity);
            }
            component_json["error_details"] = component_details;
        }
        component_json_list.push_back(component_json);
    }

    (*json)["component_list"] = component_json_list;
    (*json)["component_count"] = component_json_list.size();
    return json;
}
} // end namespace xpum::cli
