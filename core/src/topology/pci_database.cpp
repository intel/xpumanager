/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pci_database.cpp
 */

#include "pci_database.h"

#include <unistd.h>

#include <fstream>
#include <iostream>

#include "../include/xpum_structs.h"
#include "dlfcn.h"
#include "infrastructure/logger.h"
#include "infrastructure/xpum_config.h"
#include <sys/stat.h>

namespace xpum {

PciDatabase::PciDatabase() {
    XPUM_LOG_TRACE("PciDatabase()");
}

PciDatabase::~PciDatabase() {
    devices.clear();
}

PciDatabase &PciDatabase::instance() {
    static PciDatabase instance;
    std::unique_lock<std::mutex> lock(instance.mutex);

    if (!instance.bInitialized) {
        if (!instance.init()) {
            XPUM_LOG_ERROR("Failed to initialize PciDatabase, Device topology function does not work!");
        }
    }
    instance.bInitialized = true;
    return instance;
}

bool PciDatabase::init() {
    std::ifstream infile;
    std::string fileName, folder;

    folder = std::string(XPUM_CONFIG_DIR);
    fileName = folder + std::string(PCI_IDS_FILE);

    infile.open(fileName.data());

    if (infile.is_open()) {
        if (!parse_pci_device(infile)) {
            XPUM_LOG_DEBUG("PciDatabase::init()- parse_pci_device error.");
        }
        infile.close();
    } else {
        XPUM_LOG_DEBUG("PciDatabase::init()- open file {} error.", fileName);
        char exePath[XPUM_MAX_PATH_LEN];
        ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath));
        exePath[len] = '\0';
        std::string currentFile = exePath;
#ifndef DAEMONLESS
        folder = currentFile.substr(0, currentFile.find_last_of('/')) + "/../lib/xpum/config/";
        struct stat buffer;
        if (stat(folder.c_str(), &buffer) != 0)
            folder = currentFile.substr(0, currentFile.find_last_of('/')) + "/../lib64/xpum/config/";
#else
        folder = currentFile.substr(0, currentFile.find_last_of('/')) + "/../lib/xpu-smi/config/";
        struct stat buffer;
        if (stat(folder.c_str(), &buffer) != 0)
            folder = currentFile.substr(0, currentFile.find_last_of('/')) + "/../lib64/xpu-smi/config/";
#endif
        fileName = folder + std::string(PCI_IDS_FILE);
        infile.open(fileName.data());
        if (!infile.is_open()) {
            XPUM_LOG_DEBUG("PciDatabase::init()- open file {} error.", fileName);
        } else {
            if (!parse_pci_device(infile)) {
                XPUM_LOG_DEBUG("PciDatabase::init()- parse_pci_device error.");
            }
            infile.close();
        }
    }

    fileName = folder + std::string(PCI_IDS_CONFIG);
    infile.open(fileName.data());

    if (infile.is_open()) {
        parse_device_config(infile);
        infile.close();
    } else {
        XPUM_LOG_DEBUG("PciDatabase::init()- open file {} error.", fileName);
    }

    return true;
}

bool PciDatabase::parse_pci_device(std::ifstream &fstream) {
    bool bResult = false;
    int vendor_id = 0, device_id = 0, sub_vendor_id = 0, sub_device_id = 0;
    std::string verdor_name, device_name, sub_s_name;
    id_type idtype = ID_UNKNOWN;
    std::string info;

    while (std::getline(fstream, info)) {
        bool comment = false;
        int level = 0;
        std::size_t idx = 0;
        std::size_t len = info.length();

        if (len == 0) {
            continue;
        }
        while (idx < len) {
            if (is_blank_space(info.at(idx))) {
                idx++;
            } else if ((info.at(idx) == '#')) {
                comment = true;
                break;
            } else {
                break;
            }
        }
        if (comment) {
            continue;
        }

        idx = 0;
        while (idx < len) {
            if (info.at(idx) == '\t') {
                idx++;
                level++;
            } else {
                break;
            }
        }

        if (level == 0) {
            device_id = sub_vendor_id = sub_device_id = -1;
            bResult = parse_level_0(info, len, &idtype, &vendor_id, &idx);
            std::string name = info.substr(idx);
            verdor_name = name.erase(0, name.find_first_not_of(' '));
        } else if (level == 1) {
            device_id = sub_vendor_id = sub_device_id = -1;
            bResult = parse_level_1(info, len, &idtype, &device_id, &idx);
            std::string name = info.substr(idx);
            device_name = name.erase(0, name.find_first_not_of(' '));

            if (idtype != ID_KNOWN_D_CLASS) {
                add_switch_device(vendor_id, device_id, verdor_name, device_name,
                                  sub_vendor_id, sub_device_id, sub_s_name);
            }
            //device_name.erase();
        } else if (level == 2) {
            sub_vendor_id = sub_device_id = -1;
            bResult = parse_level_2(info, len, &idtype, &sub_vendor_id, &sub_device_id, &idx);
            std::string name = info.substr(idx);
            sub_s_name = name.erase(0, name.find_first_not_of(' '));
            if (idtype != ID_KNOWN_D_CLASS) {
                add_switch_device(vendor_id, device_id, verdor_name, device_name,
                                  sub_vendor_id, sub_device_id, sub_s_name);
            }
            //sub_s_name.erase();
        } else {
            // level 3 if not defined, the code should not be here, just ignore this line
            continue;
        }
    }

    return bResult;
}

bool PciDatabase::parse_level_0(const std::string &info, int len,
                                id_type *type, int *vendor_id, std::size_t *idx) {
    bool bResult = false;
    int min_level_0_len = 5;
    if (info.at(0) == 'C' && len >= 2 && is_blank_space(info.at(1))) {
        // known device classes, subclasses and programming interfaces
        if (len > min_level_0_len) {
            *vendor_id = std::stoi(info.substr(2, 2).c_str(), nullptr, 16);
            if (*vendor_id >= 0) {
                *type = ID_KNOWN_D_CLASS;
                *idx = min_level_0_len;
                bResult = true;
            }
        }
    } else if (info.at(0) >= 'A' && info.at(0) <= 'Z' && len >= 2 && is_blank_space(info.at(1))) {
        *type = ID_UNKNOWN;
        bResult = true;
    } else {
        if (len > min_level_0_len) {
            *vendor_id = std::stoi(info.substr(0, 4).c_str(), nullptr, 16);
            if (*vendor_id >= 0 && is_blank_space(info.at(4))) {
                *type = ID_VENDOR;
                *idx = min_level_0_len;
                bResult = true;
            }
        }
    }
    return bResult;
}

bool PciDatabase::parse_level_1(const std::string &info, int len, id_type *type,
                                int *device_id, std::size_t *idx) {
    bool bResult = false;
    int start = 1;
    int min_level_1_len;
    switch (*type) {
        case ID_UNKNOWN: {
            bResult = true;
        } break;
        case ID_KNOWN_D_CLASS: {
            min_level_1_len = start + 3;
            *device_id = std::stoi(info.substr(start, 2).c_str(), nullptr, 16);
            if (*device_id >= 0 && is_blank_space(info.at(start + 2))) {
                *idx = min_level_1_len;
                bResult = true;
            }
        } break;
        case ID_VENDOR:
        case ID_DEVICE:
        case ID_SUB_SYS: {
            min_level_1_len = start + 5;
            if (len > min_level_1_len) {
                *device_id = std::stoi(info.substr(start, 4).c_str(), nullptr, 16);
                if (*device_id >= 0 && is_blank_space(info.at(start + 4))) {
                    *type = ID_DEVICE;
                    *idx = min_level_1_len;
                    bResult = true;
                }
            }
        } break;
    }
    return bResult;
}

bool PciDatabase::parse_level_2(const std::string &info, int len, id_type *type,
                                int *sub_vendor_id, int *sub_device_id, std::size_t *idx) {
    bool bResult = false;
    int start = 2;
    int min_level_2_len;
    switch (*type) {
        case ID_UNKNOWN: {
            bResult = true;
        } break;
        case ID_KNOWN_D_CLASS: {
            min_level_2_len = 5;
            bResult = true;
        } break;
        case ID_DEVICE:
        case ID_SUB_SYS: {
            min_level_2_len = 12;
            if (len > min_level_2_len) {
                *sub_vendor_id = std::stoi(info.substr(start, 4), nullptr, 16);
                if (*sub_vendor_id >= 0 && is_blank_space(info.at(start + 4))) {
                    start = start + 5;
                    *sub_device_id = std::stoi(info.substr(start, 4), nullptr, 16);
                    if (*sub_device_id >= 0 && is_blank_space(info.at(start + 4))) {
                        *type = ID_SUB_SYS;
                        bResult = true;
                        *idx = min_level_2_len;
                    }
                }
            }
        } break;
        case ID_VENDOR: {
            XPUM_LOG_DEBUG("PciDatabase::parse_level_2() error- unknow device.");
            bResult = true;
        } break;
    }

    return bResult;
}

bool PciDatabase::is_blank_space(const char c) {
    return ((c == ' ') || (c == '\t'));
}

void PciDatabase::parse_device_config(std::ifstream &fstream) {
    int vendor_id = 0, device_id = 0;
    std::string info;

    while (std::getline(fstream, info)) {
        bool comment = false;
        std::size_t start = 0, pos = 0;
        std::size_t len = info.length();

        if (len == 0) {
            continue;
        }
        while (start < len) {
            if (is_blank_space(info.at(start))) {
                start++;
            } else if ((info.at(start) == '#')) {
                comment = true;
                break;
            } else {
                break;
            }
        }
        if (comment) {
            continue;
        }

        start = 0;
        vendor_id = std::stoi(info.substr(start), &pos, 16);
        start += pos + 1;
        pos = 0;
        if (start < len) {
            device_id = std::stoi(info.substr(start), &pos, 16);
            start += pos + 1;
            if (start < len) {
                PcieDevice device = {DV_UNKNOW, false, vendor_id, device_id, 0, 0};

                if (info.at(start) == '0') {
                    int ret = devices.erase(std::make_pair(vendor_id, device_id));
                    XPUM_LOG_TRACE("PciDatabase::parse_switch_config()- remove d_id:v_id = [{}:{}] count:{}", vendor_id, device_id, ret);
                } else if (info.at(start) == '1') {
                    device.type = DV_SWITCH;
                    devices[std::make_pair(vendor_id, device_id)] = device;
                } else if (info.at(start) == '2') {
                    start++;
                    device.type = DV_GRAPHIC;

                    while (start < len) {
                        if (is_blank_space(info.at(start))) {
                            start++;
                        } else {
                            break;
                        }
                    }

                    if (info.at(start) != '0') {
                        device.grouped = true;
                    }
                    start++;
                    while (start < len) {
                        if (is_blank_space(info.at(start))) {
                            start++;
                        } else {
                            break;
                        }
                    }
                    if (start < len) {
                        device.device_name = info.substr(start);
                        XPUM_LOG_TRACE("PciDatabase::parse_switch_config()- device_name:{}", device.device_name);
                    }
                    devices[std::make_pair(vendor_id, device_id)] = device;
                } else {
                    XPUM_LOG_DEBUG("PciDatabase::parse_switch_config() error- unknow value.");
                }
            }
        }
    }
}

void PciDatabase::add_switch_device(int32_t vendor_id, int32_t device_id, std::string &verdor_name,
                                    std::string &device_name, int32_t sub_v_id, int32_t sub_d_id, std::string &sub_s_name) {
    std::string switch_string = std::string(" Switch ");
    PcieDevice device =
        {DV_SWITCH, false, vendor_id, device_id, sub_v_id, sub_d_id};

    if (sub_v_id >= 0 && sub_d_id >= 0 && !sub_s_name.empty()) {
        if (sub_s_name.find(switch_string) != std::string::npos) {
            XPUM_LOG_DEBUG("PciDatabase::add_switch_device {}", device.tostring());
            devices[std::make_pair(vendor_id, device_id)] = device;
        }
    } else if (vendor_id >= 0 && device_id >= 0 && !device_name.empty()) {
        if (device_name.find(switch_string) != std::string::npos) {
            XPUM_LOG_DEBUG("PciDatabase::add_switch_device {}", device.tostring());
            devices[std::make_pair(vendor_id, device_id)] = device;
        }
    } else {
        XPUM_LOG_DEBUG("PciDatabase::add_switch_device() error- unknow device {}.", device.tostring());
    }
}

const PcieDevice *PciDatabase::getDevice(int32_t vendor_id, int32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);

    device_map::iterator it = devices.find(std::make_pair(vendor_id, device_id));

    if (it != devices.end()) {
        return &it->second;
    }

    return nullptr;
}

} // end namespace xpum
