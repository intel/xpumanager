#include "pci_database.h"

#include "logger.h"
#include "xpum_config.h"

#include <assert.h>
#include <fstream>
#include <iostream>
#include <unistd.h>

PciDatabase::PciDatabase() {
    LOG_INFO("PciDatabase()");
}

PciDatabase::~PciDatabase() {
    LOG_INFO("~PciDatabase()");
    switch_config.clear();
    switch_device.clear();
}

PciDatabase &PciDatabase::instance() {
    static PciDatabase instance;
    std::unique_lock<std::mutex> lock(instance.mutex);

    if (!instance.bInitialized) {
        instance.bInitialized = instance.init();
    }

    return instance;
}

bool PciDatabase::init() {
    std::ifstream infile;
    std::string fileName, folder;
    char exePath[MAX_PATH];

    ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath));
    exePath[len] = '\0';

    if (len > 0 && len != sizeof(exePath)) {
        folder = std::string(exePath);
        folder = folder.substr(0, folder.find_last_of('/'));
        fileName = folder + "/../" + std::string(PCI_IDS_DIR) + std::string(PCI_IDS_CONFIG);
        infile.open(fileName.data());

<<<<<<< HEAD
        if (infile.is_open()) {
            parse_switch_config(infile);
            infile.close();
            
        } else {
            Logger::instance().error("PciDatabase::init()- open file[" + fileName + "] error.");
=======
        if (!infile.is_open()) {
            LOG_ERROR("PciDatabase::init()- open file[{}] error.", fileName);
            return true;
>>>>>>> bb8b231b416c0876872e05eee64f9b2502a7d767
        }
        
    }

    if (folder.empty()) {
        fileName = std::string(PCI_IDS_DIR_BAK) + std::string(PCI_IDS_FILE);
    } else {
        fileName = folder + "/../" + std::string(PCI_IDS_DIR) + std::string(PCI_IDS_FILE);
    }

    infile.open(fileName.data());

    if (!infile.is_open()) {
<<<<<<< HEAD
        Logger::instance().error("PciDatabase::init()- open file[" + fileName + "] error.");
        fileName = std::string(PCI_IDS_DIR_BAK) + std::string(PCI_IDS_FILE);

        infile.open(fileName.data());

        if (!infile.is_open()) {
            Logger::instance().error("PciDatabase::init()- open file[" + fileName + "] error.");
            return false;
        }
=======
        LOG_ERROR("PciDatabase::init()- open file[{}] error.", fileName);
        return false;
>>>>>>> bb8b231b416c0876872e05eee64f9b2502a7d767
    }

    if (!parse_pci_device(infile)) {
        LOG_ERROR("PciDatabase::init()- parse_pci_device error.");
    }

    infile.close();

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

            if(idtype != ID_KNOWN_D_CLASS){
                add_switch_device(vendor_id, device_id, verdor_name, device_name,
                                  sub_vendor_id, sub_device_id, sub_s_name);
            }
            device_name.erase();
        } else if (level == 2) {
            sub_vendor_id = sub_device_id = -1;
            bResult = parse_level_2(info, len, &idtype, &sub_vendor_id, &sub_device_id, &idx);
            std::string name = info.substr(idx);
            sub_s_name = name.erase(0, name.find_first_not_of(' '));
            if(idtype != ID_KNOWN_D_CLASS){
                add_switch_device(vendor_id, device_id, verdor_name, device_name,
                                  sub_vendor_id, sub_device_id, sub_s_name);
            }
            sub_s_name.erase();
        } else {
            assert(0);
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
        assert(0);
    } break;
    }

    return bResult;
}

bool PciDatabase::is_blank_space(const char c) {
    return ((c == ' ') || (c == '\t'));
}

void PciDatabase::parse_switch_config(std::ifstream &fstream) {
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
                if (info.at(start) == '0') {
                    switch_config[std::make_pair(vendor_id, device_id)] = false;
                } else if (info.at(start) == '1') {
                    switch_config[std::make_pair(vendor_id, device_id)] = true;
                } else {
                    assert(0);
                    LOG_ERROR("PciDatabase::parse_switch_config() error- unknow value.");
                }
            }
        }
    }
}

void PciDatabase::add_switch_device(int32_t vendor_id, int32_t device_id, std::string &verdor_name,
                                    std::string &device_name, int32_t sub_v_id, int32_t sub_d_id, std::string &sub_s_name) {
    std::string switch_string = std::string(" Switch ");
    bool bAdd = false;
    if(vendor_id >= 0 && device_id >= 0){
        switch_config_map::iterator it = switch_config.find(std::make_pair(vendor_id, device_id));
        if(it != switch_config.end()) {
            if(it->second) {
                bAdd = true;
            } else {
                return;
            }
        }
    }

    PciDevice device = 
            {vendor_id, device_id, verdor_name, device_name, sub_v_id, sub_d_id, sub_s_name};
    
    if(bAdd) {
        Logger::instance().info(device.tostring());
        switch_device[std::make_pair(vendor_id, device_id)] = device;
        return;
    }

    if(sub_v_id>=0 && sub_d_id>=0 && !sub_s_name.empty()){
        if (sub_s_name.find(switch_string) != std::string::npos) {
            Logger::instance().info(device.tostring());
            switch_device[std::make_pair(vendor_id, device_id)] = device;
        }
    } else if(vendor_id>=0 && device_id>=0 && !device_name.empty()) {
        if (device_name.find(switch_string) != std::string::npos) {
            Logger::instance().info(device.tostring());
            switch_device[std::make_pair(vendor_id, device_id)] = device;
        }
    } else {
        assert(0);
    }    
}

bool PciDatabase::isSwitchDevice(int32_t vendor_id, int32_t device_id) {
    std::unique_lock<std::mutex> lock(mutex);

    pci_device_map::iterator it = switch_device.find(std::make_pair(vendor_id, device_id));

    if(it != switch_device.end()) {
        return true;
    }
    
    return false;
}