/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file igsc_manager.cpp
 */

#include "igsc_manager.h"
#include "igsc_lib.h"
#include <iostream>
#include <sstream>
#include <fstream>

static std::string print_fw_version(const struct igsc_fw_version* fw_version) {
    std::stringstream ss;
    ss << fw_version->project[0];
    ss << fw_version->project[1];
    ss << fw_version->project[2];
    ss << fw_version->project[3];
    ss << "_";
    ss << fw_version->hotfix;
    ss << ".";
    ss << fw_version->build;
    return ss.str();
}

static std::string print_fwdata_version(const struct igsc_fwdata_version* fwdata_version) {
    std::stringstream ss;
    ss << fwdata_version->major_version;
    ss << ".";
    ss << fwdata_version->oem_manuf_data_version;
    ss << ".";
    ss << fwdata_version->major_vcn;
    return ss.str();
}

int IGSC_Manager::init() {
    int ret;
    struct igsc_device_iterator* iter;
    struct igsc_device_info info;
    struct igsc_device_handle handle;
    unsigned int ndevices = 0;

    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_iterator_create(&iter);
    if (ret != IGSC_SUCCESS) {
        std::cout << "IGSC init failed" << std::endl;
        return -1;
    }
    info.name[0] = '\0';
    while ((ret = igsc_device_iterator_next(iter, &info)) == IGSC_SUCCESS) {
        ret = igsc_device_init_by_device_info(&handle, &info);
        if (ret != IGSC_SUCCESS) {
            /* make sure we have a printable name */
            info.name[0] = '\0';
            continue;
        }
        /*
        printf("Device [%d] '%s': %04hx:%04hx %04hx:%04hx %04hu:%02x:%02x.%02x\n",
               ndevices,
               info.name,
               info.vendor_id, info.device_id,
               info.subsys_vendor_id, info.subsys_device_id,
               info.domain, info.bus, info.dev, info.func);
        */
        char bdf_array[20] = {};
        sprintf_s(bdf_array, "%04hu:%02x:%02x.%01x", info.domain, info.bus, info.dev, info.func);
        std::string bdf = std::string(bdf_array);
        bdf_to_devicepath[bdf] = std::string(info.name);
        ndevices++;
        /* make sure we have a printable name */
        info.name[0] = '\0';
        (void)igsc_device_close(&handle);
    }
    igsc_device_iterator_destroy(iter);
    initialized = true;
    return 0;
}

std::string IGSC_Manager::getDeviceGSCVersion(std::string bdf) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }
    struct igsc_fw_version fw_version;
    struct igsc_device_handle handle;
    int ret;
    std::string device_path_str = bdf_to_devicepath[bdf];
    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, device_path_str.c_str());
    if (ret != IGSC_SUCCESS) {
        igsc_device_close(&handle);
        return "unknown";
    }

    memset(&fw_version, 0, sizeof(fw_version));
    ret = igsc_device_fw_version(&handle, &fw_version);
    if (ret != IGSC_SUCCESS) {
        igsc_device_close(&handle);
        return "unknown";
    }

    auto version = print_fw_version(&fw_version);
    igsc_device_close(&handle);
    return version;
}

std::string IGSC_Manager::getDeviceGSCDataVersion(std::string bdf) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }
    struct igsc_fwdata_version fwdata_version;
    struct igsc_device_handle handle;
    int ret;
    std::string device_path_str = bdf_to_devicepath[bdf];
    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, device_path_str.c_str());
    if (ret != IGSC_SUCCESS) {
        igsc_device_close(&handle);
        return "unknown";
    }

    memset(&fwdata_version, 0, sizeof(fwdata_version));
    ret = igsc_device_fwdata_version(&handle, &fwdata_version);
    if (ret != IGSC_SUCCESS) {
        igsc_device_close(&handle);
        return "unknown";
    }

    auto version = print_fwdata_version(&fwdata_version);
    igsc_device_close(&handle);
    return version;
}

int IGSC_Manager::runFlashGSC(std::string& bdf, std::string& image_file) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }

    return 0;
}

int IGSC_Manager::runFlashGSCData(std::string bdf, std::string& image_file) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }

    return 0;
}

bool IGSC_Manager::isValidGSCImage(std::string image_file) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }
    uint8_t type;
    int ret;
    auto buffer = readImageContent(image_file.c_str());
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS) {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_GFX_FW;
}

bool IGSC_Manager::isValidGSCDataImage(std::string image_file) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }
    uint8_t type;
    int ret;
    auto buffer = readImageContent(image_file.c_str());
    ret = igsc_image_get_type((const uint8_t*)buffer.data(), buffer.size(), &type);
    if (ret != IGSC_SUCCESS) {
        return false;
    }
    return type == IGSC_IMAGE_TYPE_FW_DATA;
}

std::vector<char> IGSC_Manager::readImageContent(const char* filePath) {
    struct stat s;
    if (stat(filePath, &s) != 0 || !(s.st_mode & S_IFREG))
        return std::vector<char>();
    std::ifstream is(std::string(filePath), std::ifstream::binary);
    if (!is) {
        return std::vector<char>();
    }
    // get length of file:
    is.seekg(0, is.end);
    int length = is.tellg();
    is.seekg(0, is.beg);

    std::vector<char> buffer(length);

    is.read(buffer.data(), length);
    is.close();
    return buffer;
}

bool IGSC_Manager::setDeviceEccState(std::string bdf, uint8_t req_state, uint8_t* cur_state, uint8_t* pen_state) {
    if (!initialized) {
        if (init() < 0) {
            return false;
        }
    }
    *cur_state = (*cur_state) & 0xFF;
    *pen_state = (*pen_state) & 0xFF;

    struct igsc_device_handle handle;
    int ret;
    std::string device_path_str = bdf_to_devicepath[bdf];
    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, device_path_str.c_str());
    if (ret != IGSC_SUCCESS) {
        igsc_device_close(&handle);
        return false;
    }

    ret = igsc_ecc_config_set(&handle, (uint8_t)req_state, cur_state, pen_state);
    return ret == IGSC_SUCCESS;
}

bool IGSC_Manager::getDeviceEccState(std::string bdf, uint8_t* cur_state, uint8_t* pen_state) {
    if (!initialized) {
        if (init() < 0) {
            return false;
        }
    }

    *cur_state = (*cur_state) & 0xFF;
    *pen_state = (*pen_state) & 0xFF;

    struct igsc_device_handle handle;
    int ret;
    std::string device_path_str = bdf_to_devicepath[bdf];

    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, device_path_str.c_str());
    if (ret != IGSC_SUCCESS) {
        igsc_device_close(&handle);
        return false;
    }

    ret = igsc_ecc_config_get(&handle, cur_state, pen_state);
    return ret == IGSC_SUCCESS;
}
