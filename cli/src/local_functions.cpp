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
#include <algorithm>
#include <iostream>

#include "config.h"
#include "local_functions.h"
#include "utility.h"
#include "exit_code.h"

using namespace std;

namespace xpum::cli {

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
        close(fd);
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
            if (!system(enable_commad.c_str())) {
                pclose(f);
                return false;   
            }
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

//Get PCI name from lspci and then get device ID and vendor ID from sysfs
//It returns true if one of data is available
bool getPciDeviceData(PciDeviceData &data, const std::string &bdf) {
    bool ret = false;
    std::string cmd = "lspci -D -s " + bdf + " 2>&1|cut -d ':' -f 4";
    char buf[BUF_SIZE];
    FILE *pf = popen(cmd.c_str(), "r");
    if (pf == NULL) {
        return false;
    }
    if (fgets(buf, BUF_SIZE, pf) != NULL) {
        if (strnlen(buf, BUF_SIZE) > 0) {
            std::string line(buf+1);
            if (line.length() > 0) {
                if (line.at(line.length() - 1) == '\n') {
                    line.pop_back();
                }
                data.name = line;
                ret = true;
            }
        }
    }
    int r = pclose(pf);
    if (r == -1 || WEXITSTATUS(r) != 0) {
        data.name.clear();
        ret = false;
    }
    std::string fileName = "/sys/bus/pci/devices/" + bdf + "/device";
    int fd = open(fileName.c_str(), O_RDONLY);
    if (fd < 0) {
        return ret;
    }
    int szRead = read(fd, buf, BUF_SIZE);
    close (fd);
    if (szRead <= 0 || szRead >= BUF_SIZE) {
        return ret;
    }
    //Remove the last '\n'
    buf[szRead - 1] = 0;
    data.pciDeviceId = buf;
    fileName = "/sys/bus/pci/devices/" + bdf + "/vendor";
    fd = open(fileName.c_str(), O_RDONLY);
    if (fd < 0) {
        return ret;
    }
    szRead = read(fd, buf, BUF_SIZE);
    close (fd);
    if (szRead <= 0 || szRead >= BUF_SIZE) {
        return ret;
    }
    buf[szRead - 1] = 0;
    data.vendorId = buf;
    return true;
}

bool getPciPath(std::vector<string> &pciPath, const std::string &bdf) {
    //An example of the output of the cmd belew is:
    //0000:4a:02.0/4b:00.0/4c:01.0/4d:00.0
    //And the function would return a vector (pciPath) of full BDF
    string cmd = "lspci -DPPs " + bdf + " 2>&1|cut -d ' ' -f 1";
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

bool isATSM3(std::string &pciId) {
    std::transform(pciId.begin(), pciId.end(), pciId.begin(), ::tolower);
    return pciId.find("56c1") != std::string::npos;
}

bool isATSM1(std::string &pciId) {
    std::transform(pciId.begin(), pciId.end(), pciId.begin(), ::tolower);
    return pciId.find("56c0") != std::string::npos;
}

bool isSG1(std::string &pciId) {
    std::transform(pciId.begin(), pciId.end(), pciId.begin(), ::tolower);
    return pciId.find("4907") != std::string::npos;
}

bool isATSMPlatform(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str.find("56c0") != std::string::npos || str.find("56c1") != std::string::npos;
}

bool isOamPlatform(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str.find("bd5") != std::string::npos || str.find("bd6") != std::string::npos ||
    str.find("bd7") != std::string::npos || str.find("bd8") != std::string::npos ||
    str.find("b69") != std::string::npos;
}

bool isDriversAutoprobeEnabled(const std::string &bdfAddress) {
    bool res = false;
    std::stringstream path, content;
    path << "/sys/bus/pci/devices/" << bdfAddress << "/sriov_drivers_autoprobe";
    std::ifstream ifs(path.str(), std::ios::in);
    content << ifs.rdbuf();
    try {
        res = std::stoi(content.str());
    } catch (const std::exception &err) {
        // Just prevent core dump
    }
    return res;
}

static int execCommand(const std::string& command, std::string &result) {
    int exitcode = 0;
    std::array<char, 1048576> buffer{};

    FILE* pipe = popen(command.c_str(), "r");
    if (pipe != nullptr) {
        try {
            std::size_t bytesread;
            while ((bytesread = std::fread(buffer.data(), sizeof(buffer.at(0)), sizeof(buffer), pipe)) != 0) {
                result = std::string(buffer.data(), bytesread);
            }
        } catch (...) {
            pclose(pipe);
        }
        int ret = pclose(pipe);
        exitcode = WEXITSTATUS(ret);
    }
    return exitcode;
}

bool isATSMPlatformFromSysFile() {
    char path[PATH_MAX];
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;

    bool is_atsm_platform = true;
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
                close(fd);
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
            }
        }
        closedir(pdir);
    }
    return is_atsm_platform;
}

std::unique_ptr<nlohmann::json> addKernelParam() {
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    std::string grubPath = "/etc/default/grub";
    std::ifstream ifs(grubPath);
    if (!ifs.is_open()) {
        (*json)["error"] = "Fail to open grub file.";
        (*json)["errno"] = XPUM_CLI_ERROR_VGPU_ADD_KERNEL_PARAM_FAILED;
        return json;
    }
    bool hasTargetParam = false;
    std::string line;
    std::vector<std::string> buffer;
    while (std::getline(ifs, line)) {
        buffer.push_back(line);
        line = trim(line, " \t");
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream lineStream(line);
        std::string key, value;
        if (std::getline(lineStream, key, '=') && std::getline(lineStream, value)) {
            if (key.compare("GRUB_CMDLINE_LINUX_DEFAULT") == 0 || key.compare("GRUB_CMDLINE_LINUX") == 0) {
                if (value.find("intel_iommu=") != std::string::npos || value.find("i915.max_vfs=") != std::string::npos) {
                    hasTargetParam = true;
                }
            }
        }
    }
    ifs.close();
    if (hasTargetParam) {
        (*json)["error"] = "intel_iommu or i915.max_vfs is already exists in GRUB command line in /etc/default/grub, please make sure the parameters are correct and take effect manually";
        (*json)["errno"] = XPUM_CLI_ERROR_VGPU_ADD_KERNEL_PARAM_FAILED;
        return json;
    }
    
    auto targetLine = std::find_if(buffer.begin(), buffer.end(), [](std::string &s) {
        return s.find("GRUB_CMDLINE_LINUX") != std::string::npos && s.find("GRUB_CMDLINE_LINUX_DEFAULT") == std::string::npos;
    });
    if (targetLine == buffer.end()) {
        (*json)["error"] = "Invalid grub default file";
        (*json)["errno"] = XPUM_CLI_ERROR_VGPU_ADD_KERNEL_PARAM_FAILED;
        return json;
    }
    auto pos = targetLine->find_last_of("\"");
    if (pos == std::string::npos) {
        (*json)["error"] = "Invalid grub default file";
        (*json)["errno"] = XPUM_CLI_ERROR_VGPU_ADD_KERNEL_PARAM_FAILED;
        return json;
    }
    if (isATSMPlatformFromSysFile())
        targetLine->insert(pos, " intel_iommu=on i915.max_vfs=31");
    else
        targetLine->insert(pos, " intel_iommu=on iommu=pt i915.force_probe=* i915.max_vfs=63 i915.enable_iaf=0");
    std::ofstream ofs("/etc/default/grub", std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        (*json)["error"] = "Fail to open grub file.";
        (*json)["errno"] = XPUM_CLI_ERROR_VGPU_ADD_KERNEL_PARAM_FAILED;
        return json;
    }
    for (auto str: buffer) {
        ofs << str << std::endl;
    }
    ofs.flush();
    ofs.close();
    
    std::string cmdStr = "grub2-mkconfig -o /boot/grub2/grub.cfg", cmdRes;
    auto osRelease = getOsRelease();
    if (osRelease == LINUX_OS_RELEASE_UNKNOWN) {
        (*json)["error"] = "Unsupported Linux OS release";
        (*json)["errno"] = XPUM_CLI_ERROR_VGPU_ADD_KERNEL_PARAM_FAILED;
        return json;
    }
    if (osRelease == LINUX_OS_RELEASE_UBUNTU || osRelease == LINUX_OS_RELEASE_DEBIAN) {
        /*
         *  Refer: https://manpages.ubuntu.com/manpages/jammy/man8/update-grub.8.html
         *  Refer: https://manpages.debian.org/buster/grub2-common/update-grub.8.en.html
         */
        cmdStr = "update-grub";
    } else if (osRelease == LINUX_OS_RELEASE_CENTOS && isFileExists("/boot/efi/EFI/centos/grub.cfg")) {
        cmdStr = "grub2-mkconfig -o /boot/efi/EFI/centos/grub.cfg";
    } else if (osRelease == LINUX_OS_RELEASE_SLES && isFileExists("/boot/efi/EFI/sles/grub.cfg")) {
        cmdStr = "grub2-mkconfig -o /boot/efi/EFI/sles/grub.cfg";
    } else if (osRelease == LINUX_OS_RELEASE_RHEL && isFileExists("/boot/efi/EFI/rhel/grub.cfg")) {
        /*
         *  Refer: https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/7/html/system_administrators_guide/ch-working_with_the_grub_2_boot_loader
         */
        cmdStr = "grub2-mkconfig -o /boot/efi/EFI/rhel/grub.cfg";
    } else if (osRelease == LINUX_OS_RELEASE_OPEN_EULER && isFileExists("/boot/efi/EFI/openEuler/grub.cfg")) {
        cmdStr = "grub2-mkconfig -o /boot/efi/EFI/openEuler/grub.cfg";
    } 
    if (execCommand(cmdStr, cmdRes) != 0) {
        (*json)["error"] = "Fail to update grub.";
        (*json)["errno"] = XPUM_CLI_ERROR_VGPU_ADD_KERNEL_PARAM_FAILED;
        return json;
    }
    return json;
}

}
