/* 
 *  Copyright (C) 2021-2025 Intel Corporation
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
#include <sstream>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <igsc_lib.h>

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
    return str.find("56c0") != std::string::npos || str.find("56c1") != std::string::npos || str.find("56c2") != std::string::npos;
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

bool getUEvent(UEvent &uevent, const char *d_name) {
    bool ret = false;
    char buf[1024];
    char path[PATH_MAX];
    if (d_name == NULL) {
        return false;
    }
    snprintf(path, PATH_MAX, "/sys/class/drm/%s/device/uevent", d_name);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return false;
    }
    int cnt = read(fd, buf, 1024);
    if (cnt < 0 || cnt >= 1024) {
        close(fd);
        return false;
    }
    buf[cnt] = 0;
    std::string str(buf);
    std::string key = "PCI_ID=8086:";
    auto pos = str.find(key); 
    if (pos != std::string::npos) {
        uevent.pciId = str.substr(pos + key.length(), 4);
    } else {
        goto RTN;
    }
    key = "PCI_SLOT_NAME=";
    pos = str.find(key);
    if (pos != std::string::npos) {
        uevent.bdf = str.substr(pos + key.length(), 12);
    } else {
        goto RTN;
    }
    ret = true;

RTN:
    close(fd);
    return ret;
}

bool isATSMPlatformFromSysFile() {
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    bool is_atsm_platform = true;
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
            UEvent uevent;
            if (getUEvent(uevent, pdirent->d_name) == true) {
                is_atsm_platform = isATSMPlatform(uevent.pciId);
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
    std::string devType = isFileExists("/sys/module/xe/srcversion")?"xe":"i915";
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
                if (value.find("intel_iommu=") != std::string::npos || value.find(devType + ".max_vfs=") != std::string::npos) {
                    hasTargetParam = true;
                }
            }
        }
    }
    ifs.close();
    if (hasTargetParam) {
        (*json)["error"] = "intel_iommu or " + devType + ".max_vfs is already exists in GRUB command line in /etc/default/grub, please make sure the parameters are correct and take effect manually";
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
        targetLine->insert(pos, " intel_iommu=on "+devType+".max_vfs=31");
    else
        targetLine->insert(pos, " intel_iommu=on iommu=pt " + devType + ".force_probe=* " + devType + ".max_vfs=63 " + devType + ".enable_iaf=0");
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

static std::vector<std::string> getBdfAddrFromIgsc() {
    std::vector<std::string> res;
    std::string output;
    int ret = execCommand("igsc list-devices 2>&1", output);
    if (!ret) {
        std::regex bdfPattern("[0-9a-f]{4}\\:[0-9a-f]{2}\\:[0-9a-f]{2}\\.[0-9a-f]");
        auto begin = std::sregex_iterator(output.begin(), output.end(), bdfPattern);
        auto end = std::sregex_iterator();
        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::smatch match = *i;
            res.push_back(match.str());
        }
    }
    return res;
}

static bool unloadDriver(std::string &error) {
    std::string _tmp;
    int ret = execCommand("systemctl status gdm 2>&1", _tmp);
    if (!ret) {
        ret = execCommand("systemctl stop gdm 2>&1", error);
        if (ret) {
            error = "Fail to stop gdm, error message: " + error;
            return false;
        }
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::vector<std::string> bdfAddrList = getBdfAddrFromIgsc();

    for (auto bdfAddr : bdfAddrList) {
        std::string unbindPath = "/sys/bus/pci/drivers/i915/unbind";
        std::fstream unbind(unbindPath, std::ios_base::out);
        if (!unbind) {
            error = "Fail to open " + unbindPath;
            return false;
        }
        if (!(unbind << bdfAddr)) {
            error = "Fail to write auto to " + unbindPath;
            return false;
        }

        std::string powerControlPath = "/sys/bus/pci/devices/" + bdfAddr + "/power/control";
        std::fstream powerControl(powerControlPath, std::ios_base::out);
        if (powerControl && !(powerControl << "auto")) {
            error = "Fail to write auto to " + powerControlPath;
            return false;
        }

        std::string resetPath = "/sys/bus/pci/devices/" + bdfAddr + "/reset";
        std::fstream reset(resetPath, std::ios_base::out);
        if (!reset) {
            error = "Fail to open " + resetPath;
            return false;
        }
        if (!(reset << 1)) {
            error = "Fail to write 1 to " + resetPath;
            return false;
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
    ret = execCommand("rmmod i915 2>&1", error);
    if (ret) {
        error = "Fail to rmmod i915, error message: " + error;
        return false;
    }
    return true;
}

bool recoverable() {
    std::string i915driverPath = "/sys/bus/pci/drivers/i915";
    std::string xeConfigPath = "/sys/kernel/config/xe";
    std::regex bdfPattern("[0-9a-f]{4}\\:[0-9a-f]{2}\\:[0-9a-f]{2}\\.[0-9a-f]");
    DIR *pdir = NULL;
    struct dirent *pdirent = NULL;
    bool all_flex = true;
    std::string deviceIdStr;

    pdir = opendir(i915driverPath.c_str());
    if (pdir) {
        while ((pdirent = readdir(pdir))) {
            if (std::regex_match(pdirent->d_name, bdfPattern)) {
                std::string bdfAddr = pdirent->d_name;
                std::string deviceIdPath = i915driverPath + "/" + bdfAddr + "/device";
                std::fstream deviceId(deviceIdPath, std::ios_base::in);
                std::string tmp;
                if (deviceId && deviceId >> tmp) {
                    if (isATSMPlatform(tmp)) {
                        if (deviceIdStr.empty()) {
                            deviceIdStr = tmp;
                        } else if (deviceIdStr == tmp) {
                        } else {
                            // different model in same node
                            all_flex = false;
                        }
                    } else {
                        all_flex = false;
                    }
                }
            }
        }
        closedir(pdir);
    }
    else {
        //Check for Survivability mode in XE
        pdir = opendir(xeConfigPath.c_str());
        all_flex = false;
        if (pdir) {
            while ((pdirent = readdir(pdir)) != NULL) {
                if (std::regex_match(pdirent->d_name, bdfPattern)) {
                    std::string bdfAddr = pdirent->d_name;
                    std::string survivabilityPath = xeConfigPath + "/" + bdfAddr + "/survivability_mode";
                    std::fstream deviceId(survivabilityPath, std::ios_base::in);
                    std::string mode;
                    if (deviceId && deviceId >> mode) {
                        if ("1" == mode)
                            all_flex = true;
                    }
                }
            }
            closedir(pdir);
        }
    }
    return all_flex;
}

bool setSurvivabilityMode(bool enable, std::string &error, bool &modified) {
    std::string SURVIVABILITY_MODE_PATH = "/sys/module/i915/parameters/survivability_mode";
    modified = false;

    std::string val;
    std::fstream modeFile(SURVIVABILITY_MODE_PATH, std::ios_base::in);
    if (!modeFile) {
        //Check for XE Driver before returning error for i915 case
        DIR* xeConfigDir = opendir("/sys/kernel/config/xe");
        if (xeConfigDir) {
            closedir(xeConfigDir);
            return true;
        }
        error = "Driver installed doesn't support survivability mode. Or please run recovery with superuser.";
        return false;
    }
    if (!(modeFile >> val)) {
        error = "Fail to read survivability_mode value from: " + SURVIVABILITY_MODE_PATH + ".";
        return false;
    }

    if (enable) {
        if (val == "Y") {
            return true;
        } else {
            if (!unloadDriver(error)) {
                return false;
            }
            int ret = execCommand("modprobe i915 survivability_mode=1 2>&1", error);
            if (ret) {
                error = "Fail to enter suvivability mode, error message: " + error + ".";
                return false;
            } else {
                modified = true;
                return true;
            }
        }
    } else {
        if (val == "Y") {
            int ret = execCommand("rmmod i915 2>&1", error);
            if (ret) {
                error = "Fail to unload driver, error message: " + error + ".";
                return false;
            }
            ret = execCommand("modprobe i915 2>&1", error);
            if (ret) {
                error = "Fail to leave suvivability mode, error message: " + error + ".";
                return false;
            } else {
                modified = true;
                return true;
            }
        } else {
            return true;
        }
    }
}
}
