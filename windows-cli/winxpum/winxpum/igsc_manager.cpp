/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file igsc_manager.cpp
 */

#include "igsc_manager.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "igsc_lib.h"
#include <excpt.h>

static const std::string igscMissingErrorInfo{"This feature requires the igsc library. Please make sure it was installed correctly."};
static const std::string igscEccMissingErrorInfo{"This feature requires the igsc-0.8.4 library or newer. Please make sure it was installed correctly."};

struct img {
    uint32_t size;
    uint8_t blob[0];
};

static struct img* image_read_from_file(const char* p_path) {
    FILE* fp = NULL;
    struct img* img = NULL;
    long file_size;
    char err_msg[64] = {0};
    errno = 0;

    if (fopen_s(&fp, p_path, "rb") != 0 || fp == NULL) {
        goto exit;
    }

    if (fseek(fp, 0L, SEEK_END) != 0) {
        goto exit;
    }

    file_size = ftell(fp);
    if (file_size < 0) {
        goto exit;
    }

    if (file_size > IGSC_MAX_IMAGE_SIZE) {
        goto exit;
    }

    if (fseek(fp, 0L, SEEK_SET) != 0) {
        goto exit;
    }

    img = (struct img*)malloc((size_t)file_size + sizeof(*img));
    if (img == NULL) {
        goto exit;
    }

    if (fread(img->blob, 1, (size_t)file_size, fp) != (size_t)file_size) {
        goto exit;
    }
    /* note: the size was already checked it ifts to 32bit */
    img->size = (uint32_t)file_size;

    fclose(fp);

    return img;

exit:
    free(img);
    if (fp) {
        fclose(fp);
    }

    return NULL;
}

static int firmware_check_hw_config(struct igsc_device_handle* handle, const struct img* img) {
    struct igsc_hw_config device_hw_config;
    struct igsc_hw_config image_hw_config;
    int ret;

    memset(&device_hw_config, 0, sizeof(device_hw_config));
    memset(&image_hw_config, 0, sizeof(image_hw_config));

    ret = igsc_device_hw_config(handle, &device_hw_config);
    if (ret != IGSC_SUCCESS && ret != IGSC_ERROR_NOT_SUPPORTED) {
        return ret;
    }

    ret = igsc_image_hw_config(img->blob, img->size, &image_hw_config);
    if (ret != IGSC_SUCCESS && ret != IGSC_ERROR_NOT_SUPPORTED) {
        return ret;
    }

    return igsc_hw_config_compatible(&image_hw_config, &device_hw_config);
}

static int firmware_update(const char* device_path, const char* image_path, bool allow_downgrade, bool force_update) {
    struct img* img = NULL;
    struct igsc_device_handle handle;
    struct igsc_fw_version device_fw_version;
    struct igsc_fw_version image_fw_version;
    char* device_path_found = NULL;
    igsc_progress_func_t progress_func = NULL;
    int ret;
    uint8_t cmp;
    struct igsc_fw_update_flags flags = {0};

    memset(&handle, 0, sizeof(handle));

    img = image_read_from_file(image_path);
    if (img == NULL) {
        ret = EXIT_FAILURE;
        goto exit;
    }

    memset(&image_fw_version, 0, sizeof(image_fw_version));
    ret = igsc_image_fw_version(img->blob, img->size, &image_fw_version);
    if (ret != IGSC_SUCCESS) {
        goto exit;
    }

    ret = igsc_device_init_by_device(&handle, device_path);
    if (ret) {
        goto exit;
    }

    memset(&device_fw_version, 0, sizeof(device_fw_version));
    ret = igsc_device_fw_version(&handle, &device_fw_version);
    if (ret != IGSC_SUCCESS) {
        goto exit;
    }

    cmp = igsc_fw_version_compare(&image_fw_version, &device_fw_version);
    switch (cmp) {
        case IGSC_VERSION_NEWER:
            break;
        case IGSC_VERSION_NOT_COMPATIBLE:
            ret = EXIT_FAILURE;
            goto exit;
        case IGSC_VERSION_OLDER:
            /* fall through */
        case IGSC_VERSION_EQUAL:
            if (!allow_downgrade) {
                ret = IGSC_ERROR_BAD_IMAGE;
                goto exit;
            }
            break;
        default:
            ret = EXIT_FAILURE;
            goto exit;
    }

    ret = firmware_check_hw_config(&handle, img);
    if (ret == IGSC_ERROR_INCOMPATIBLE) {
        ret = EXIT_FAILURE;
        goto exit;
    }
    if (ret != IGSC_SUCCESS) {
        ret = EXIT_FAILURE;
        goto exit;
    }

    if (force_update) {
        flags.force_update = 1;
    }
    ret = igsc_device_fw_update_ex(&handle, img->blob, img->size,
                                   progress_func, NULL, flags);
exit:
    (void)igsc_device_close(&handle);

    free(img);
    free(device_path_found);
    return ret;
}

static int image_fwdata_match_check(const char* image_path, struct igsc_device_handle* handle, struct igsc_device_info* dev_info, std::string& error_message) {
    struct img* img = NULL;
    struct igsc_fwdata_image* oimg = NULL;
    struct igsc_fwdata_version dev_version;
    struct igsc_fwdata_version img_version;
    int ret;
    img = image_read_from_file(image_path);
    if (img == NULL) {
        ret = EXIT_FAILURE;
        goto exit;
    }
    
    ret = igsc_image_fwdata_init(&oimg, img->blob, img->size);
    if (ret == IGSC_ERROR_BAD_IMAGE) {
        ret = EXIT_FAILURE;
        goto exit;
    }
    if (ret != IGSC_SUCCESS) {
        ret = EXIT_FAILURE;
        goto exit;
    }

    ret = igsc_image_fwdata_version(oimg, &img_version);
    if (ret != IGSC_SUCCESS) {
        goto exit;
    }

    ret = igsc_device_fwdata_version(handle, &dev_version);
    if (ret != IGSC_SUCCESS) {
        goto exit;
    }

    ret = igsc_image_fwdata_match_device(oimg, dev_info);
    if (ret != IGSC_SUCCESS) {
        error_message = "The image file is a right FW image file, but not proper for the target GPU.";
        goto exit;
    }
    uint8_t cmp;
    cmp = igsc_fwdata_version_compare(&img_version, &dev_version);
    switch (cmp) {
        case IGSC_FWDATA_VERSION_ACCEPT:
            break;
        case IGSC_FWDATA_VERSION_OLDER_VCN:
            break;
        case IGSC_FWDATA_VERSION_REJECT_DIFFERENT_PROJECT:
            error_message = "Firmware data version is not compatible with the installed one (project version)";
            ret = EXIT_FAILURE;
            goto exit;
        case IGSC_FWDATA_VERSION_REJECT_VCN:
            error_message = "Firmware data version is not compatible with the installed one (VCN version)";
            ret = EXIT_FAILURE;
            goto exit;
        case IGSC_FWDATA_VERSION_REJECT_OEM_MANUF_DATA_VERSION:
            error_message = "Firmware data version is not compatible with the installed one (OEM version)";
            ret = EXIT_FAILURE;
            goto exit;
        default:
            error_message = "Firmware data version error in comparison";
            ret = EXIT_FAILURE;
            goto exit;
    }
exit:
    igsc_image_fwdata_release(oimg);
    free(img);

    return ret;
}

static int fwdata_update(const char* image_path, struct igsc_device_handle* handle, struct igsc_device_info* dev_info, bool allow_downgrade) {
    struct img* img = NULL;
    struct igsc_fwdata_image* oimg = NULL;
    struct igsc_fwdata_version dev_version;
    struct igsc_fwdata_version img_version;
    igsc_progress_func_t progress_func = NULL;
    uint8_t cmp;
    bool update = false;
    int ret;

    img = image_read_from_file(image_path);
    if (img == NULL) {
        ret = EXIT_FAILURE;
        goto exit;
    }

    ret = igsc_image_fwdata_init(&oimg, img->blob, img->size);
    if (ret == IGSC_ERROR_BAD_IMAGE) {
        ret = EXIT_FAILURE;
        goto exit;
    }

    if (ret != IGSC_SUCCESS) {
        ret = EXIT_FAILURE;
        goto exit;
    }

    ret = igsc_image_fwdata_version(oimg, &img_version);
    if (ret != IGSC_SUCCESS) {
        goto exit;
    }

    ret = igsc_device_fwdata_version(handle, &dev_version);
    if (ret != IGSC_SUCCESS) {
        goto exit;
    }

    ret = igsc_image_fwdata_match_device(oimg, dev_info);
    if (ret == IGSC_ERROR_DEVICE_NOT_FOUND) {
        goto exit;
    } else if (ret != IGSC_SUCCESS) {
        goto exit;
    }

    cmp = igsc_fwdata_version_compare(&img_version, &dev_version);
    switch (cmp) {
        case IGSC_FWDATA_VERSION_ACCEPT:
            update = true;
            break;
        case IGSC_FWDATA_VERSION_OLDER_VCN:
            update = allow_downgrade;
            break;
        case IGSC_FWDATA_VERSION_REJECT_DIFFERENT_PROJECT:
            ret = EXIT_FAILURE;
            goto exit;
        case IGSC_FWDATA_VERSION_REJECT_VCN:
            ret = EXIT_FAILURE;
            goto exit;
        case IGSC_FWDATA_VERSION_REJECT_OEM_MANUF_DATA_VERSION:
            ret = EXIT_FAILURE;
            goto exit;
        default:
            ret = EXIT_FAILURE;
            goto exit;
    }

    if (!update) {
        goto exit;
    }
    ret = igsc_device_fwdata_image_update(handle, oimg, progress_func, NULL);

exit:
    igsc_image_fwdata_release(oimg);
    free(img);

    return ret;
}

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
    __try {
        int ret;
        struct igsc_device_iterator* iter;
        struct igsc_device_info info;
        struct igsc_device_handle handle;
        unsigned int ndevices = 0;

        memset(&handle, 0, sizeof(handle));
        ret = igsc_device_iterator_create(&iter);
        if (ret != IGSC_SUCCESS) {
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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cout << igscMissingErrorInfo << std::endl;
        exit(-1);
    }
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

bool IGSC_Manager::isFwImageAndDeviceCompatible(std::string bdf, std::string image_file) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }

    struct igsc_device_handle handle;
    int ret;
    std::string device_path_str = bdf_to_devicepath[bdf];
    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, device_path_str.c_str());
    if (ret != IGSC_SUCCESS) {
        return false;
    }
    struct img* img = NULL;
    img = image_read_from_file(image_file.c_str());
    if (img == NULL) {
        return false;
    }

    bool match = firmware_check_hw_config(&handle, img) == IGSC_SUCCESS;
    igsc_device_close(&handle);
    return match;
}

bool IGSC_Manager::isFwDataImageAndDeviceCompatible(std::string bdf, std::string image_file, std::string& error_message) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }

    std::string device_path_str = bdf_to_devicepath[bdf];
    const char* image_path = image_file.c_str();
    const char* device_path = device_path_str.c_str();
    struct igsc_device_info dev_info;
    struct igsc_device_handle handle;
    int ret;
    memset(&dev_info, 0, sizeof(dev_info));
    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, device_path);
    if (ret != IGSC_SUCCESS)
        return ret;
    ret = igsc_device_get_device_info(&handle, &dev_info);
    if (ret != IGSC_SUCCESS)
        return ret;

    bool match = image_fwdata_match_check(image_path, &handle, &dev_info, error_message) == IGSC_SUCCESS;
    igsc_device_close(&handle);
    return match;
}

int IGSC_Manager::runFlashGSC(std::string bdf, std::string image_file) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }

    std::string device_path_str = bdf_to_devicepath[bdf];
    return firmware_update(device_path_str.c_str(), image_file.c_str(), true, false);
}

int IGSC_Manager::runFlashGSCData(std::string bdf, std::string image_file) {
    if (!initialized) {
        if (init() < 0) {
            std::cout << "IGSC init failed" << std::endl;
            exit(-1);
        }
    }
    std::string device_path_str = bdf_to_devicepath[bdf];
    bool allow_downgrade = true;
    const char* image_path = image_file.c_str();
    const char* device_path = device_path_str.c_str();
    struct igsc_device_info dev_info;
    struct igsc_device_handle handle;
    int ret;
    memset(&dev_info, 0, sizeof(dev_info));
    memset(&handle, 0, sizeof(handle));
    ret = igsc_device_init_by_device(&handle, device_path);
    if (ret != IGSC_SUCCESS)
        return ret;
    ret = igsc_device_get_device_info(&handle, &dev_info);
    if (ret != IGSC_SUCCESS)
        return ret;
    ret = fwdata_update(image_path, &handle, &dev_info, allow_downgrade);
    igsc_device_close(&handle);
    return ret;
}

bool IGSC_Manager::setDeviceEccState(std::string bdf, uint8_t req_state, uint8_t* cur_state, uint8_t* pen_state) {
    __try {
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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cout << igscEccMissingErrorInfo << std::endl;
        exit(-1);
    }
}

bool IGSC_Manager::getDeviceEccState(std::string bdf, uint8_t* cur_state, uint8_t* pen_state) {
    __try {
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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::cout << igscEccMissingErrorInfo << std::endl;
        exit(-1);      
    }
}
