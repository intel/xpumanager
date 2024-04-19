/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware_manager.cpp
 */

#include "pch.h"
#include "firmware_manager.h"
#include "../infrastructure/logger.h"
#include "core/core.h"

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

namespace xpum {

    void FirmwareManager::init() {
        p_amc_manager = std::make_shared<IpmiAmcManager>();
    };

    using namespace gfx_fw_status;



    std::string FirmwareManager::transGfxFwStatusToString(GfxFwStatus status) {
        switch (status) {
        case RESET:
            return "reset";
        case INIT:
            return "init";
        case RECOVERY:
            return "recovery";
        case TEST:
            return "test";
        case FW_DISABLED:
            return "fw_disabled";
        case NORMAL:
            return "normal";
        case DISABLE_WAIT:
            return "disable_wait";
        case OP_STATE_TRANS:
            return "op_state_trans";
        case INVALID_CPU_PLUGGED_IN:
            return "invalid_cpu_plugged_in";
        default:
            return "unknown";
        }
    }

    GfxFwStatus FirmwareManager::getGfxFwStatus(xpum_device_id_t deviceId) {
        // TODO
        return GfxFwStatus::UNKNOWN;
    }

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

        if (!force_update) {
            ret = firmware_check_hw_config(&handle, img);
            if (ret == IGSC_ERROR_INCOMPATIBLE) {
                ret = EXIT_FAILURE;
                goto exit;
            }
            if (ret != IGSC_SUCCESS) {
                ret = EXIT_FAILURE;
                goto exit;
            }
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

    bool FirmwareManager::updateFWVersionProps() {
        std::unique_lock<std::mutex> lock(m_prop);
        if (isFWVerPropUpdated == true) {
            return true;
        }
        std::vector<std::shared_ptr<Device>> devices;
        Core::instance().getDeviceManager()->getDeviceList(devices);
        for (auto device : devices) {
            Property prop;
            if (device->getProperty(
                    XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop) 
                == false) {
                continue;
            }
            std::string ver = getDeviceGSCVersion(prop.getValue());
            device->addProperty(
                Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION,
                         ver));
            ver = getDeviceGSCDataVersion(prop.getValue());
            device->addProperty(
                Property(XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION,
                         ver));
        }
        isFWVerPropUpdated = true;
        return true;
    }

    std::string FirmwareManager::getDeviceGSCVersion(std::string bdf) {
        if (initIgsc() == false) {
            return "unknown";
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

    std::string FirmwareManager::getDeviceGSCDataVersion(std::string bdf) {
        if (initIgsc() == false) {
            return "unknown";
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

    void FirmwareManager::getFlashResult(xpum_device_id_t deviceId, xpum_firmware_flash_task_result_t* result) {
        std::unique_lock<std::mutex> lock(m_results);
        if (result->type == XPUM_DEVICE_FIRMWARE_AMC) {
            auto ipmi_enabled = p_amc_manager->preInit();
            GetAmcFirmwareFlashResultParam param;
            p_amc_manager->getAMCFirmwareFlashResult(param);

            if (param.result.result == XPUM_DEVICE_FIRMWARE_FLASH_ONGOING) {
                result->result = XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
                result->percentage = param.result.percentage;
            }
            if (param.result.result == XPUM_DEVICE_FIRMWARE_FLASH_ERROR) {
                result->result = XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
            }
            if (param.result.type == XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED) {
                result->result = XPUM_DEVICE_FIRMWARE_FLASH_UNSUPPORTED;
            }
            if (param.result.result == XPUM_DEVICE_FIRMWARE_FLASH_OK) {
                result->result = XPUM_DEVICE_FIRMWARE_FLASH_OK;
                result->percentage = param.result.percentage;
            }
            return;
        }
        for (int i = 0; i < flash_results.size(); i++) {
            std::future_status status = flash_results[i].wait_for(std::chrono::milliseconds(0));
            if (status != std::future_status::ready) {
                result->result = XPUM_DEVICE_FIRMWARE_FLASH_ONGOING;
                return;
            }
        }
        bool error = false;
        for (int i = 0; i < flash_results.size(); i++) {
            if (flash_results[i].get() == xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR) {
                error = true;
                break;
            }
        }
        result->percentage = 100;
        if (error == true) {
            result->result = XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
        } else {
            result->result = XPUM_DEVICE_FIRMWARE_FLASH_OK;
        }
        
        flash_results.clear();
    }

    xpum_result_t FirmwareManager::runGSCFirmwareFlash(xpum_device_id_t deviceId, const char* filePath, bool force) {
        flashFwErrMsg.clear();
        if (initIgsc() != true) {
            return XPUM_NOT_INITIALIZED;
        }
        auto devices = getSiblingDevices(deviceId);
        if (devices.size() == 0) {
            devices.push_back(deviceId);
        }
        std::unique_lock<std::mutex> lock(m_results);
        for (int i = 0; i < flash_results.size(); i++) {
            std::future_status status = flash_results[i].wait_for(std::chrono::milliseconds(0));
            if (status != std::future_status::ready) {
                return XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
            }
        }
        flash_results.clear();
        for (auto id : devices) {
            Property prop;
            if (Core::instance().getDeviceManager()->getDevice(std::to_string(id))->
                getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop) == false) {
                continue;
            }
            std::string bdf = prop.getValue();
            std::string img_file(filePath);
            if (!force && isFwImageAndDeviceCompatible(bdf, img_file) == false) {
                flashFwErrMsg = "The image file is a right FW image file, but not proper for the target GPU.";
                return XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
            }
            flash_results.push_back(std::async(std::launch::async, [this, bdf, img_file, force] {
                XPUM_LOG_DEBUG("runGSCFirmwareFlash: start ansyc FW update");
                std::string device_path_str = bdf_to_devicepath[bdf];
                int ret = firmware_update(device_path_str.c_str(), img_file.c_str(), true, force);
                XPUM_LOG_DEBUG("runGSCFirmwareFlash: ansyc FW update done with ret = {}", ret);
                if (ret == 0) {
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
                } else {
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                }
            }));
        }
        return XPUM_OK;
    }
    
    xpum_result_t FirmwareManager::runFwDataFlash(xpum_device_id_t deviceId, const char* filePath) {
        flashFwErrMsg.clear();
        if (initIgsc() != true) {
            return XPUM_NOT_INITIALIZED;
        }
        auto devices = getSiblingDevices(deviceId);
        if (devices.size() == 0) {
            devices.push_back(deviceId);
        }
        std::unique_lock<std::mutex> lock(m_results);
        for (int i = 0; i < flash_results.size(); i++) {
            std::future_status status = flash_results[i].wait_for(std::chrono::milliseconds(0));
            if (status != std::future_status::ready) {
                return XPUM_UPDATE_FIRMWARE_TASK_RUNNING;
            }
        }
        flash_results.clear();
        for (auto id : devices) {
            Property prop;
            if (Core::instance().getDeviceManager()->getDevice(std::to_string(id))->
                getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop) == false) {
                continue;
            }
            std::string bdf = prop.getValue();
            std::string img_file(filePath);
            std::string err;
            if (isFwDataImageAndDeviceCompatible(bdf, img_file, err) == false) {
                flashFwErrMsg = err;
                return XPUM_UPDATE_FIRMWARE_FW_IMAGE_NOT_COMPATIBLE_WITH_DEVICE;
            }
            flash_results.push_back(std::async(std::launch::async, [this, bdf, img_file] {
                std::string device_path_str = bdf_to_devicepath[bdf];
                bool allow_downgrade = true;
                const char* image_path = img_file.c_str();
                const char* device_path = device_path_str.c_str();
                struct igsc_device_info dev_info;
                struct igsc_device_handle handle;
                int ret;
                memset(&dev_info, 0, sizeof(dev_info));
                memset(&handle, 0, sizeof(handle));
                ret = igsc_device_init_by_device(&handle, device_path);
                if (ret != IGSC_SUCCESS) {
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                }
                ret = igsc_device_get_device_info(&handle, &dev_info);
                if (ret != IGSC_SUCCESS) {
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                }
                ret = fwdata_update(image_path, &handle, &dev_info, allow_downgrade);
                igsc_device_close(&handle);

                if (ret == 0) {
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_OK;
                } else {
                    return xpum_firmware_flash_result_t::XPUM_DEVICE_FIRMWARE_FLASH_ERROR;
                }
            }));
        }
        return XPUM_OK;
    }

    xpum_result_t FirmwareManager::runAMCFlash(const char* filePath, const char* user, const char* password) {
        FlashAmcFirmwareParam param;
        auto ipmi_enabled = p_amc_manager->preInit();
        param.file = std::string(filePath);

        if (user != nullptr) {
            param.username = std::string(user);
        } else {
            param.username = std::string("");
        }

        if (password != nullptr) {
            param.password = std::string(password);
        } else {
            param.password = std::string("");
        }

        param.callback = [this]() {
        };
        p_amc_manager->flashAMCFirmware(param);
        return XPUM_OK;
    }

    xpum_result_t FirmwareManager::getSimpleEccState(xpum_device_id_t deviceId, uint8_t& current, uint8_t& pending) {
        uint8_t cur = 0xFF;
        uint8_t pen = 0xFF;
        current = cur;
        pending = pen;
        __try {
            if (initIgsc() != true) {
                return XPUM_NOT_INITIALIZED;
            }

            Property prop;
            if (Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop) == false) {
                return XPUM_RESULT_DEVICE_NOT_FOUND;
            }
            std::string bdf = prop.getValue();
            int ret;
            
            struct igsc_device_handle handle;
            std::string device_path_str = bdf_to_devicepath[bdf];

            memset(&handle, 0, sizeof(handle));
            ret = igsc_device_init_by_device(&handle, device_path_str.c_str());
            if (ret != IGSC_SUCCESS) {
                igsc_device_close(&handle);
                return XPUM_RESULT_DEVICE_NOT_FOUND;
            }

            ret = igsc_ecc_config_get(&handle, &cur, &pen);
            if (ret != IGSC_SUCCESS) {
                return XPUM_GENERIC_ERROR;           
            } else {
                current = cur;
                pending = pen;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            XPUM_LOG_WARN(igscEccMissingErrorInfo);
            return XPUM_GENERIC_ERROR;   
        }
        return XPUM_OK;
    }

    xpum_result_t FirmwareManager::setSimpleEccState(xpum_device_id_t deviceId, uint8_t req, uint8_t& current, uint8_t& pending) {
        __try {
            if (initIgsc() != true) {
                return XPUM_NOT_INITIALIZED;
            }

            Property prop;
            if (Core::instance().getDeviceManager()->getDevice(std::to_string(deviceId))->getProperty(XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS, prop) == false) {
                return XPUM_RESULT_DEVICE_NOT_FOUND;
            }
            std::string bdf = prop.getValue();
            int ret;

            struct igsc_device_handle handle;
            std::string device_path_str = bdf_to_devicepath[bdf];

            memset(&handle, 0, sizeof(handle));
            ret = igsc_device_init_by_device(&handle, device_path_str.c_str());
            if (ret != IGSC_SUCCESS) {
                igsc_device_close(&handle);
                return XPUM_RESULT_DEVICE_NOT_FOUND;
            }
            uint8_t cur, pen;
            ret = igsc_ecc_config_set(&handle, req, &cur , &pen);
            if (ret != IGSC_SUCCESS) {
                return XPUM_GENERIC_ERROR;
            } else {
                current = cur;
                pending = pen;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            XPUM_LOG_WARN(igscEccMissingErrorInfo);
            return XPUM_GENERIC_ERROR;
        }
        return XPUM_OK;
    }

    bool FirmwareManager::initIgsc() {
        std::unique_lock<std::mutex> lock(m_igsc);
        if (isIgscInit == true) {
            return true;
        }
        __try {
            int ret;
            struct igsc_device_iterator* iter;
            struct igsc_device_info info;
            struct igsc_device_handle handle;

            memset(&handle, 0, sizeof(handle));
            ret = igsc_device_iterator_create(&iter);
            if (ret != IGSC_SUCCESS) {
                return false;
            }
            info.name[0] = '\0';
            while ((ret = igsc_device_iterator_next(iter, &info)) == IGSC_SUCCESS) {
                ret = igsc_device_init_by_device_info(&handle, &info);
                if (ret != IGSC_SUCCESS) {
                    /* make sure we have a printable name */
                    info.name[0] = '\0';
                    continue;
                }
                char bdf_array[20] = {};
                sprintf_s(bdf_array, "%04hu:%02x:%02x.%01x", info.domain, info.bus, info.dev, info.func);
                std::string bdf = std::string(bdf_array);
                bdf_to_devicepath[bdf] = std::string(info.name);
                isIgscInit = true;
                /* make sure we have a printable name */
                info.name[0] = '\0';
                (void)igsc_device_close(&handle);
            }
            igsc_device_iterator_destroy(iter);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            XPUM_LOG_DEBUG(igscMissingErrorInfo);
            isIgscInit = false;
        }
        return isIgscInit;
    }

    xpum_result_t FirmwareManager::getAmcFwVersions(std::vector<std::string> *versions, const char* user, const char* password) {
        auto ipmi_enabled = p_amc_manager->preInit();
        GetAmcFirmwareVersionsParam param;
        if (user != nullptr) {
            param.username = std::string(user);
        } else {
            param.username = std::string("");
        }
        
        if (password != nullptr) {
            param.password = std::string(password);
        } else {
            param.password = std::string("");
        }
        
        p_amc_manager->getAmcFirmwareVersions(param);
        if (param.errCode != XPUM_OK) {
            versions->clear();
            return XPUM_GENERIC_ERROR;
        }

        versions->clear();
        for (auto ver :param.versions) {
            versions->push_back(ver);
        }
        return XPUM_OK;
    }

    bool FirmwareManager::isFwImageAndDeviceCompatible(std::string bdf, std::string image_file) {
        if (initIgsc() == false) {
            return false;
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
            igsc_device_close(&handle);
            return false;
        }

        bool match = firmware_check_hw_config(&handle, img) == IGSC_SUCCESS;
        igsc_device_close(&handle);
        free(img);
        return match;
    }

    bool FirmwareManager::isFwDataImageAndDeviceCompatible(std::string bdf, std::string image_file, std::string& error_message) {
        if (this->initIgsc() == false) {
            return false;
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

    bool FirmwareManager::loadSibingDevices() {
       
        uint32_t driverCount = 0;
        ze_result_t status = zeDriverGet(&driverCount, nullptr);
        if (status != ZE_RESULT_SUCCESS) {
            return false;
        }

        std::vector<ze_driver_handle_t> drivers(driverCount);
        status = zeDriverGet(&driverCount, drivers.data());
        if (status != ZE_RESULT_SUCCESS) {
            return false;
        }

        if (driverCount == 0) {
            return false;
        }
        std::vector<zes_device_handle_t> zes_device_handles;
        ze_driver_handle_t ze_driver_handle = drivers[0];
        uint32_t deviceCount = 0;
        status = zeDeviceGet(ze_driver_handle, &deviceCount, nullptr);
        if (status != ZE_RESULT_SUCCESS) {
            return false;
        }
        std::vector<ze_device_handle_t> devices(deviceCount);
        status = zeDeviceGet(ze_driver_handle, &deviceCount, devices.data());
        if (status != ZE_RESULT_SUCCESS) {
            return false;
        }

        for (int deviceIdx = 0; deviceIdx < (int)deviceCount; deviceIdx++) {
            zes_device_handles.push_back((zes_device_handle_t)devices[deviceIdx]);

            ze_device_properties_t ze_device_properties;
            ze_device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            ze_device_properties.pNext = nullptr;
            status = zeDeviceGetProperties(devices[deviceIdx], &ze_device_properties);
            if (status != ZE_RESULT_SUCCESS) {
                return false;
            }
            uint32_t deviceId = ze_device_properties.deviceId;
            zes_pci_properties_t pci_props = {};
            status = zesDevicePciGetProperties(zes_device_handles[deviceIdx], &pci_props);
            if (status != ZE_RESULT_SUCCESS) {
                return false;
            }

            if (deviceIdx > 0 && deviceId == 0x56c1) {
                ze_device_properties_t ze_pre_device_properties;
                ze_pre_device_properties.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
                ze_pre_device_properties.pNext = nullptr;
                status = zeDeviceGetProperties(devices[deviceIdx - 1], &ze_pre_device_properties);
                if (status != ZE_RESULT_SUCCESS) {
                    return false;
                }
                uint32_t preDeviceId = ze_pre_device_properties.deviceId;
                zes_pci_properties_t pre_pci_props = {};
                status = zesDevicePciGetProperties(zes_device_handles[deviceIdx - 1], &pre_pci_props);
                if (status != ZE_RESULT_SUCCESS) {
                    return false;
                }
                if (preDeviceId == 0x56c1 && std::abs((int)(pci_props.address.bus - pre_pci_props.address.bus)) <= 5) {
                    sibling_devices[deviceIdx - 1] = {deviceIdx - 1, deviceIdx};
                    sibling_devices[deviceIdx] = {deviceIdx - 1, deviceIdx};
                }
            }
        }
        return true;
    }

    std::vector<int> FirmwareManager::getSiblingDevices(int deviceId) {
        XPUM_LOG_DEBUG("getSiblingDevices: deviceId = {}", deviceId);
        std::unique_lock<std::mutex> lock(m_sblg);
        std::vector<int> ret;
        if (isSiblingDeviceReady == false) {
            isSiblingDeviceReady = loadSibingDevices();
        }
        for (int id : sibling_devices[deviceId]) {
            XPUM_LOG_DEBUG("getSiblingDevices: deviceId = {} add {} to ret", deviceId, id);
            ret.push_back(id);
        }
        return ret;
    }

} // end namespace xpum