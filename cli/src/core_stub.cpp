/*
 *  Copyright (C) 2021-2022 Intel Corporation
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

#include "xpum_structs.h"

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

static std::string dmesg_log_file_name = "/var/log/dmesg";
static std::string syslog_file_name = "/var/log/syslog";
static std::string messages_file_name = "/var/log/messages";
static int cpu_temperature_threshold = 85;
static int detect_number_of_last_logs = 500;

std::string i915_error_log_lines;
std::string cpu_error_log_lines;
std::string gpu_error_log_lines;
std::map<std::string, bool> i915_error_log_headers;
std::map<std::string, bool> cpu_error_log_headers;
std::map<std::string, bool> gpu_error_log_headers;

static void updateErrorLogLine(std::string line, std::string pattern, std::string log_source) {
    if (pattern == ".*ERROR.*i915.*") {
        if (i915_error_log_lines.size() > 0)
            i915_error_log_lines += "\n";
        if (i915_error_log_headers[log_source] == false) {
            i915_error_log_headers[log_source] = true;
            i915_error_log_lines += "Check " + log_source + ":\n";
        }
        i915_error_log_lines += "  " + line;
    } else if (pattern == ".*(mce|mca).*err.*" || pattern == ".*caterr.*") {
        if (cpu_error_log_lines.size() > 0)
            cpu_error_log_lines += "\n";
        if (cpu_error_log_headers[log_source] == false) {
            cpu_error_log_headers[log_source] = true;
            cpu_error_log_lines += "Check " + log_source + ":\n";
        }
        cpu_error_log_lines += "  " + line;
    } else {
        if (gpu_error_log_lines.size() > 0)
            gpu_error_log_lines += "\n";
        if (gpu_error_log_headers[log_source] == false) {
            gpu_error_log_headers[log_source] = true;
            gpu_error_log_lines += "Check " + log_source + ":\n";
        }
        gpu_error_log_lines += "  " + line;
    }
}

static size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos = 0) {
    std::transform(data.begin(), data.end(), data.begin(), ::tolower);
    std::transform(toSearch.begin(), toSearch.end(), toSearch.begin(), ::tolower);
    return data.find(toSearch, pos);
}

static void getErrorLogLinesByFile(std::string print_log_cmd, std::map<std::string, std::string> patterns, std::string log_source) {
    FILE* f = popen(print_log_cmd.c_str(), "r");
    char c_line[1024];
    std::smatch match;
    while (fgets(c_line, 1024, f) != NULL) {
        size_t len = strlen(c_line);
        if (len > 0 && c_line[len-1] == '\n') {
            c_line[--len] = '\0';
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
        for (auto pattern : patterns) {
            std::regex re(pattern.first, std::regex_constants::icase);
            if(std::regex_search(line, match, re)) {
                if (pattern.second.size() > 0 && line.find(pattern.second) != std::string::npos)
                    continue;
                updateErrorLogLine(line, pattern.first, log_source);
            }
        }
    }
    pclose(f);
}

static void getErrorLogLines(std::map<std::string, std::string> patterns) {
    i915_error_log_lines.clear();
    cpu_error_log_lines.clear();
    gpu_error_log_lines.clear();

    std::string print_log_cmd = "cat " + dmesg_log_file_name;
    if (detect_number_of_last_logs > 0)
        print_log_cmd += " | tail -n " + std::to_string(detect_number_of_last_logs);
    std::ifstream dmesg_log_file(dmesg_log_file_name);
    if (!dmesg_log_file.good()) {
        print_log_cmd = "dmesg | tail -n 500";
    }

    getErrorLogLinesByFile(print_log_cmd, patterns, "dmesg");
    dmesg_log_file.close();

    std::ifstream syslog_file(syslog_file_name);
    if (syslog_file.good()) {
        print_log_cmd = "cat " + syslog_file_name;
        if (detect_number_of_last_logs > 0)
            print_log_cmd += " | tail -n " + std::to_string(detect_number_of_last_logs);
        getErrorLogLinesByFile(print_log_cmd, patterns, syslog_file_name);
    }
    syslog_file.close();

    std::ifstream messages_file(messages_file_name);
    if (messages_file.good()) {
        print_log_cmd = "cat " + messages_file_name;
        if (detect_number_of_last_logs > 0)
            print_log_cmd += " | tail -n " + std::to_string(detect_number_of_last_logs);
        getErrorLogLinesByFile(print_log_cmd, patterns, messages_file_name);
    }
    messages_file.close();
}

static std::string zeInitResultToString(const int result) {
    if (result == 0) {
        return "ZE_RESULT_SUCCESS";
    } else if (result == 1) {
        return "ZE_RESULT_NOT_READY";
    } else if (result == 2) {
        return "[0x78000001] ZE_RESULT_ERROR_UNINITIALIZED.\nPlease check if you have root privileges.";
    } else if (result == 3) {
        return "[0x70020000] ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE.\nMaybe the metrics libraries aren't ready.";
    } else {
        throw std::runtime_error("Generic error with ze_result_t value: " +
            std::to_string(static_cast<int>(result)));
    }
}

static void readConfigFile() {
    char exe_path[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
    exe_path[len] = '\0';
    std::string current_file(exe_path);
    std::string config_folder = current_file.substr(0, current_file.find_last_of('/')) + "/../config/";
    std::string file_name = config_folder + std::string("diagnostics.conf");
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
            if (name == "DMESG_LOG_FILE_NAME") {
                dmesg_log_file_name = value;
            } else if (name == "SYSLOG_FILE_NAME") {
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

std::unique_ptr<nlohmann::json> CoreStub::getPreCheckInfo() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    char path[PATH_MAX];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;

    std::vector<std::string> gpu_ids;
    std::vector<std::string> gpu_bdfs;
    // GPU count
    std::map<std::string, int> gpu_counts;
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
                gpu_counts[str.substr(pos + key.length(), 4)] += 1;
                std::string bdf_key = "PCI_SLOT_NAME=0000:";
                auto bdf_pos = str.find(bdf_key); 
                if (bdf_pos != std::string::npos) {
                    gpu_bdfs.push_back(str.substr(bdf_pos + bdf_key.length(), 7));
                }
            }
        }
        closedir(pdir);
    }

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
        } 
    } else {
        level0_driver_error_info = "Failed to init level zero due to GPU driver";
    }
    dlclose(handle);

    // GPU i915 driver
    std::string gpu_basic_info;
    for (auto gpu_count : gpu_counts) {
        if (!gpu_basic_info.empty())
            gpu_basic_info += ", ";
        std::string item = std::to_string(gpu_count.second) + " (0x" + gpu_count.first + ")";
        for (auto& c : item)
            c = tolower(c);
        gpu_basic_info += item;
    }
    (*json)["gpu_basic_info"] = gpu_basic_info.empty() ? "Not found" : gpu_basic_info;
    
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

    uid_t uid = getuid();
    if (uid != 0) {
        if (!is_i915_loaded || !level0_driver_error_info.empty()) {
            (*json)["gpu_driver_info"] = (is_i915_loaded ? "" : "i915 not loaded\n") + level0_driver_error_info;
        } else {
            (*json)["gpu_driver_info"] = "Unknown [Permission denied]";
        }
        (*json)["gpu_status_info"] = (*json)["cpu_status_info"] = "Unknown [Permission denied]";
        return json;
    }

    readConfigFile();
    getErrorLogLines({
        // i915 error
        {".*ERROR.*i915.*", ""},
        // cpu error
        {".*(mce|mca).*err.*", ""},
        {".*caterr.*", ""},
        // gpu error
        {".*ERROR.*drm.*", "i915"},
        {".*ERROR.*GUC.*", ""},
        {".*(GuC initialization failed).*", ""},
        {".*(Enabling uc failed).*", ""},
        {".*(LMEM not initialized by firmware).*", ""},
        {".*(task).*(blocked for more than).*(seconds).*", ""},
        {".*([Hardware Error]).*(Hardware error from APEI Generic Hardware Error Source).*", ""},
        {".*(kmemleak).*(new suspected memory leaks).*", ""},
        {".*(perf: interrupt took too long).*(lowering kernel.perf_event_max_sample_rate).*", ""},
        {".*(Out of memory).*", ""},
        {".*(GPU HANG).*", ""},
        {".*(segfault).*(lib).*", ""},
        {".*(Kernel panic).*", ""},
        {".*(gdm-x-session).*(Server terminated with error).*", ""},
        {".*(traps:).*", ""},
        {".*(IO: IOMMU catastrophic error).*", ""},
        {".*(PCIe error).*", ""} });

    if (is_i915_loaded && i915_error_log_lines.empty() && level0_driver_error_info.empty()) {
        (*json)["gpu_driver_info"] = "Pass";
    } else {
        std::string gpu_driver_info = is_i915_loaded ? "" : "i915 not loaded";
        if (!i915_error_log_lines.empty())
            gpu_driver_info += (gpu_driver_info.empty() ? "" : "\n") + i915_error_log_lines;
        if (!level0_driver_error_info.empty())
            gpu_driver_info += (gpu_driver_info.empty() ? "" : "\n") + level0_driver_error_info;
        (*json)["gpu_driver_info"] = gpu_driver_info;
    }

    // GPU GuC HuC i915 wedged
    std::string guc_status_infos;
    std::string huc_status_infos;
    std::string i915_wedged_infos;

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
                if (guc_status_infos.size() > 0)
                    guc_status_infos += "\n";
                guc_status_infos += "GPU card" + gpu_id + " GuC is disabled";
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
                if (huc_status_infos.size() > 0)
                    huc_status_infos += "\n";
                if (is_huc_disabled)
                    huc_status_infos += "GPU card" + gpu_id + " HuC is disabled";
                else
                    huc_status_infos += "GPU card" + gpu_id + " HuC is not running";
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
                if (i915_wedged_infos.size() > 0)
                    i915_wedged_infos += "\n";
                i915_wedged_infos += "GPU card" + gpu_id + " i915 wedged";
            }
        }
        i915_wedged_file.close();
    }

    // PCIe error
    std::string pcie_error_infos;
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
            if (pcie_error_infos.size() > 0) 
                pcie_error_infos += "\n";
            pcie_error_infos += "GPU " + bdf + " pcie error  detected: \n   ." + std::string(c_line);
        }
    }

    if (guc_status_infos.empty() && huc_status_infos.empty() 
        && i915_wedged_infos.empty() && pcie_error_infos.empty() && gpu_error_log_lines.empty()) {
        (*json)["gpu_status_info"] = "Pass";
    } else {
        std::string gpu_status_info;
        if (!guc_status_infos.empty())
            gpu_status_info += (gpu_status_info.empty() ? "" : "\n") + guc_status_infos;
        if (!huc_status_infos.empty())
            gpu_status_info += (gpu_status_info.empty() ? "" : "\n") + huc_status_infos;
        if (!i915_wedged_infos.empty())
            gpu_status_info += (gpu_status_info.empty() ? "" : "\n") + i915_wedged_infos;
        if (!pcie_error_infos.empty())
            gpu_status_info += (gpu_status_info.empty() ? "" : "\n") + pcie_error_infos;
        if (!gpu_error_log_lines.empty())
            gpu_status_info += (gpu_status_info.empty() ? "" : "\n") + gpu_error_log_lines;
        (*json)["gpu_status_info"] = gpu_status_info;
    }

    // CPU Status
    bool is_cpu_temperature_normal = true;
    std::string cpu_temperature_infos;
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
            if (strcmp(thermal_type, "x86_pkg_temp")) {
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
                    is_cpu_temperature_normal = false;
                    if (cpu_temperature_infos.size() > 0)
                        cpu_temperature_infos += "\n";    
                    cpu_temperature_infos += "CPU " + std::to_string(pk_id) + " temperature is high (" + std::to_string(val) + " Celsius Degree)";
                }
                pk_id += 1;
            }
        }
        closedir(pdir);
    }
    
    if (is_cpu_temperature_normal && cpu_error_log_lines.empty())
        (*json)["cpu_status_info"] = "Pass";
    else {
        std::string cpu_status_info;
        if (!cpu_temperature_infos.empty())
            cpu_status_info += (cpu_status_info.empty() ? "" : "\n") + cpu_temperature_infos;
        if (!cpu_error_log_lines.empty())
            cpu_status_info += (cpu_status_info.empty() ? "" : "\n") + cpu_error_log_lines;
        (*json)["cpu_status_info"] = cpu_status_info;
    }

    return json;
}
} // end namespace xpum::cli
