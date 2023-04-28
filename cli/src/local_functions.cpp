/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file local_functions.cpp
 */


#include <string>
#include <vector>
#include <stdio.h>
#include <algorithm>
#include <regex>
#include <iomanip>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fstream>
#include <unordered_set>

#include "config.h"
#include "local_functions.h"
#include "utility.h"
#include "exit_code.h"

using namespace std;

namespace xpum::cli {

std::string componentTypeToStr(int component_type) {
    if (component_type == 0) {
        return "None";
    } else if (component_type == COMPONET_TYE_DRIVER) {
        return "Driver";
    } else if (component_type == COMPONET_TYE_GPU) {
        return "GPU";
    } else if (component_type == COMPONET_TYE_CPU) {
        return "CPU";
    } else {
        return "Unknown";
    }
}

std::string errorCategoryToStr(int category) {
    if (category == 0) {
        return "None";
    } else if (category == ERROR_CATEGORY_KMD) {
        return "Kernel Mode Driver";
    } else if (category == ERROR_CATEGORY_UMD) {
        return "User Mode Driver";
    } else if (category == ERROR_CATEGORY_HARDWARE) {
        return "Hardware";
    } else {
        return "Unknown";
    }
}

std::string errorSeverityToStr(int severity) {
    if (severity == 0) {
        return "None";
    } else if (severity == ERROR_SEVERITY_LOW) {
        return "Low";
    } else if (severity == ERROR_SEVERITY_MEDIUM) {
        return "Medium";
    } else if (severity == ERROR_SEVERITY_HIGH) {
        return "High";
    } else if (severity == ERROR_SEVERITY_CIRTICAL) {
        return "Critical";
    } else {
        return "Unknown";
    }
}

std::string extractLastNChars(std::string const &str, int n) {
    if ((int)str.size() < n) {
        return str;
    }
 
    return str.substr(str.size() - n);
}

size_t findCaseInsensitive(std::string data, std::string toSearch, size_t pos) {
    std::transform(data.begin(), data.end(), data.begin(), ::tolower);
    std::transform(toSearch.begin(), toSearch.end(), toSearch.begin(), ::tolower);
    return data.find(toSearch, pos);
}

void updateErrorComponentInfo(ComponentInfo& cinfo, std::string status, int category, int severity, std::string time) {
    if (cinfo.status == "Pass") {
        cinfo.status = status;
        cinfo.category = category;
        cinfo.severity = severity;
        cinfo.time = time;
    }
}

std::string zeInitResultToString(const int result) {
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

std::string to_firmware_hex_version(std::string hex_str) {
    std::string version;
    if (hex_str.size() < 16) {
        return version;
    }

    for (int i = 0; i < 8; i = i + 2) {
        int val = std::stoi(hex_str.substr(i, 2), 0, 16);
        version.push_back(char(val));
    }
    std::reverse(version.begin(), version.end());
    std::string hotfix = std::to_string(std::stoi(hex_str.substr(12, 4), 0, 16));
    std::string build = std::to_string(std::stoi(hex_str.substr(8, 4), 0, 16));
    version += "_" + hotfix + "." + build;
    return version;
}

uint32_t access_device_memory(std::string hex_base, std::string hex_val = "") {
    int fd = -1;
    void *map_base, *virt_addr; 
	uint32_t read_result = 0, writeval = 0;
	off_t target;
	target = strtoul(hex_base.c_str(), 0, 0);

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) {
        return -1;
    }
    
    map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target & ~MAP_MASK);
    
    virt_addr = (char *)map_base + (target & MAP_MASK);
	read_result = *((uint32_t *) virt_addr);	

	if(!hex_val.empty()) {
		writeval = std::stoi(hex_val, 0, 0);
        *((uint32_t *) virt_addr) = writeval;
        read_result = *((uint32_t *) virt_addr);
	}
	
    if (munmap(map_base, MAP_SIZE) == -1) {
        return -1;
    }

    close(fd);

    return read_result;
}

bool getFirmwareVersion(FirmwareVersion& fw_version, std::string bdf) {
    std::string cmd = "lspci -vvv -s " + bdf + " | egrep \"size=[0-9]{1,2}M\" 2>/dev/null";
    std::string region_base;
    FILE* f = popen(cmd.c_str(), "r");
    char c_line[1024];
    while (fgets(c_line, 1024, f) != NULL) {
        std::string line(c_line);
        if (line.find("Region") == std::string::npos) {
            continue;
        }

        if (line.find("disabled") != std::string::npos) {
            std::string enable_commad = "setpci -s " + bdf + " COMMAND=0x02";
            if (!system(enable_commad.c_str()))
                return false;
        }

        std::regex reg("[0-9a-fA-F]{12,16}");
        std::smatch match;
        if(std::regex_search(line, match, reg)) {
            region_base = match.str();
        }
    }
    pclose(f);
    
    if (region_base.empty()) {
        return false;
    }

    std::string base = "0x" + region_base;
    std::string regin_id_offset = "0x102084";
    std::string spi_address_offset = "0x102080";
    std::string spi_read_offset = "0x102040";
    std::string regin_offset = "0x1000";
    uint32_t multiply_address = 0x4;

    if (access_device_memory(add_two_hex_string(base, regin_id_offset), "0xc") != 0xc) {
        return false;
    }

    bool find_fpt_header = false;
    std::string current_header;
    int current_length = -1;
    std::string current_version;

    for (int i = 1; i <= 32; i++) {
        if (!fw_version.gfx_fw_version.empty() && !fw_version.gfx_data_fw_version.empty()) {
            return true;
        }

        std::string temp = to_hex_string((i - 1) * multiply_address);
        access_device_memory(add_two_hex_string(base, spi_address_offset), add_two_hex_string(regin_offset, temp));
        uint32_t val = access_device_memory(add_two_hex_string(base, spi_read_offset));
        std::string line = to_hex_string(val, 8).substr(2);
        // "$FPT"
        if (val == 0x54504624) {
            find_fpt_header = true;
        }

        if (find_fpt_header) {
            if (val == 0x56584647) {  //  "GFXV" GFX Code Version
                current_header = "GFXV";
                current_length = 0;
                current_version = "";
            } else if (val == 0x56444D4F) {  // "OMDV" OEM Manufacturing Data Version
                current_header = "OMDV";
                current_length = 0;
                current_version = "";                
            } else if (current_header == "GFXV") {
                if (current_length == 0)
                    current_length = val;
                else {
                    current_version += line;
                    current_length -= 4;
                    if (current_length == 0 && fw_version.gfx_fw_version.empty())
                        fw_version.gfx_fw_version = to_firmware_hex_version(current_version);
                }
            } else if (current_header == "OMDV") {
                if (current_length == 0)
                    current_length = val;
                else {
                    current_version += line;
                    current_length -= 4;
                    if (current_length == 0 && fw_version.gfx_data_fw_version.empty())
                        fw_version.gfx_data_fw_version = to_hex_string(val);
                }
            }
        }
    }
    if (fw_version.gfx_fw_version.empty() && fw_version.gfx_data_fw_version.empty())
        return false;
    else
        return true;
}

#define BUF_SIZE 256
bool getBdfListFromLspci(std::vector<std::string> &list) {
    const char *cmd = 
        "lspci|grep -i Display|grep -i Intel|cut -d ' ' -f 1";
    char buf[BUF_SIZE];
    FILE *pf = popen(cmd, "r");
    if (pf == NULL) {
        return false;
    }
    while (fgets(buf, BUF_SIZE, pf) != NULL) {
        string bdf(buf);
        if (bdf.length() > 0) {
            if (bdf.at(bdf.length() - 1) == '\n') {
                bdf.pop_back();
            }
            list.push_back(bdf);
        }
    }
    int ret = pclose(pf);
    if (ret != -1 && WEXITSTATUS(ret) == 0) {
        return true;
    } else {
        return false;
    }
}

bool getPciDeviceData(PciDeviceData &data, const std::string &bdf) {
    std::string cmd = "lspci -Dx -s " + bdf;
    char buf[BUF_SIZE];
    FILE *pf = popen(cmd.c_str(), "r");
    if (pf == NULL) {
        return false;
    }
    bool found = false;
    int line_num = 1;
    while (fgets(buf, BUF_SIZE, pf) != NULL) {
        string line(buf);
        if (line.length() < 15) {
            break;
        }
        if (line_num == 1) {
            if (line.at(line.length() - 1) == '\n') {
                line.pop_back();
            }
            size_t pos = line.rfind(':');
            if (pos == std::string::npos || pos >= line.length() - 8) {
                break;
            }
            data.name = line.substr(pos + 2);
        } else {
#define NUM_OF_BYTES 5
            unsigned bytes[NUM_OF_BYTES];
            if (std::sscanf(line.c_str(), "%x: %x %x %x %x", 
                        &bytes[0], &bytes[1], &bytes[2], &bytes[3], 
                        &bytes[4]) != NUM_OF_BYTES) {
                break;
            }
            uint16_t vendor = bytes[2] << 8 | bytes[1];
            uint16_t device = bytes[4] << 8 | bytes[3];
            std::stringstream ss;
            ss << std::string("0x") << std::hex << vendor; 
            data.vendorId = ss.str();
            ss.str("");
            ss << std::string("0x") << std::hex << device;
            data.pciDeviceId = ss.str();
            found = true;
            break;
        }
        line_num++;
    }
    int ret = pclose(pf);
    if (ret != -1 && WEXITSTATUS(ret) == 0) {
        return found;
    } else {
        return false;
    }

}

bool getPciPath(std::vector<string> &pciPath, const std::string &bdf) {
    //An example of the output of the cmd belew is:
    //0000:4a:02.0/4b:00.0/4c:01.0/4d:00.0
    //And the function would return a vector (pciPath) of full BDF
    string cmd = "lspci -DPPs " + bdf + "|cut -d ' ' -f 1";
    char path[PATH_MAX];
    FILE *pf = popen(cmd.c_str(), "r");
    if (pf == NULL) {
        return false;
    } 
    bool found = false;
    string domain;
    string node;
    if (fgets(path, PATH_MAX, pf) != NULL) {
        int len = strnlen(path, PATH_MAX);
        if (path[len - 1] == '\n') {
            path[len - 1] = 0;
        }
        char *tok = strtok(path, "/");
        while (tok != NULL) {
            node = tok;
            if (isBDF(node) == true) {
                pciPath.push_back(node);
                domain.assign(node, 0, 4);
                found = true;
            } else if (isShortBDF(node) == true) {
                if (domain.length() == 4) {
                    pciPath.push_back(domain + ":" + node);
                } else {
                    pciPath.push_back(node);
                }
            } 
            tok = strtok(NULL, "/");
        }
    }
    int ret = pclose(pf);
    if (ret != -1 && WEXITSTATUS(ret) == 0) {
        return found;
    } else {
        return false;
    }
}

/**
 * @brief 
 * The following code is used for diag precheck
 */

static int cpu_temperature_threshold = 85;

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

    // kernel issue but not related with gpu driver will not update driver info
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
            if (findCaseInsensitive(line, tw) != std::string::npos) {
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
            if (findCaseInsensitive(ep.pattern, key) != std::string::npos) {
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

static void readConfigFile() {
    std::string file_name = std::string(XPUM_CONFIG_DIR) + std::string("diagnostics.conf");
    struct stat buffer;
    if (stat(file_name.c_str(), &buffer) != 0) {
        char exe_path[PATH_MAX];
        ssize_t len = ::readlink("/proc/self/exe", exe_path, sizeof(exe_path));
        if (len == PATH_MAX) {
            len -= 1;
        }
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
            line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
            if (line[0] == '#' || line.empty())
                continue;
            auto delimiter_pos = line.find("=");
            auto name = line.substr(0, delimiter_pos);
            auto value = line.substr(delimiter_pos + 1);
            if (value.find("#") != std::string::npos)
                value = value.substr(0, value.find("#"));
            if (name == "CPU_TEMPERATURE_THRESHOLD") {
                cpu_temperature_threshold = atoi(value.c_str());
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
                updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, "GuC is disabled", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_CIRTICAL);
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
                        updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, "HuC is disabled", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_HIGH);
                    else
                        updateErrorComponentInfoList(gpu_bdfs[gpu_id_index], -1, "HuC is not running", ERROR_CATEGORY_HARDWARE, ERROR_SEVERITY_HIGH);
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

static bool isPhysicalFunctionDevice(std::string pci_addr) {
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

static void doPreCheck(bool onlyGPU, std::string sinceTime) {
    bool has_privilege = (getuid() == 0);
    readConfigFile();
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
                        component_gpus.push_back({COMPONET_TYE_GPU, has_privilege ? "Pass" : "Unknown", 0, 0, -1, bdf, ""});
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
                component_gpus.push_back({COMPONET_TYE_GPU, has_privilege ? "Pass" : "Unknown", 0, 0, -1, bdf, ""});
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
    doPreCheckGuCHuCWedgedPCIe(gpu_ids, gpu_bdfs, is_atsm_platform);
    scanErrorLogLines(error_patterns, sinceTime);
}

std::unique_ptr<nlohmann::json> getPreCheckInfo(bool onlyGPU, bool rawJson, std::string sinceTime) {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    if (sinceTime.size() > 0) {
        std::string test_cmd = "journalctl --since \"" + sinceTime + "\" -n 1 >/dev/null 2>&1";
        if (system(test_cmd.c_str()) != 0) {
            (*json)["error"] = "invalid since time";
            (*json)["errno"] = XPUM_CLI_ERROR_DIAGNOSTIC_PRECHECK_SINCE_TIME;
            return json;
        }
    }
    doPreCheck(onlyGPU, sinceTime);
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

}
