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

#include "local_functions.h"
#include "utility.h"

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

#define BUF_SIZE 128
bool getBdfListFromLspci(std::vector<std::string> &list) {
    const char *cmd = 
        "lspci|grep -i Display|grep -i Intel|cut -d ' ' -f 1";
    char buf[BUF_SIZE];
    FILE *pf = popen(cmd, "r");
    if (pf == NULL) {
        return false;
    }
    while (fgets(buf, BUF_SIZE, pf) != NULL) {
        buf[BUF_SIZE - 1] = 0;
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

}
