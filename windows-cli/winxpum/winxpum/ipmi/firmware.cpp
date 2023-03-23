/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file firmware.cpp
 */

#include <iostream>
#ifdef __linux__
#include <getopt.h>
#include <linux/pci.h>
#include <unistd.h>
#elif _WIN32
#if (_MSC_VER >= 1910)
#include <windows.h>
#elif (_MSC_VER < 1910)
#pragma warning(disable : 4244)
#pragma warning(disable : 4702)
UINT32 gMaxDataLen;
UINT8 gFwReqData[270];
UINT32 gFwReqSize;
#define SIZE_DETECT_RES 3
#define SIZE_FW_GET_INFO_RES 18
#define SIZE_FW_UPDATE_SYNC_RES 10
#endif
//#include <wingetopt.h>
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NRV_EC_BUILD_SYSSW
// -ev EC needs to find WEXITSTATUS
#include <sys/wait.h>
#endif
#include <sys/stat.h>
#include <time.h>

#include "file_util.h"
#include "pci.h"
#include "tool.h"
#include "..\amc\ipmi_amc_manager.h"
//#include "bsmc_ipmi_oem_cmd.h"
//#include "bsmc_interface.h"

#include <vector>
#include <string>
#include <sstream>
//using namespace xpum;

namespace xpum {

static void* amcManager;

static percent_callback_func_t percentCallback;

static int fw_update_device_count;
static int fw_update_device_index;

void setPercentCallbackAndContext(percent_callback_func_t callback, void *pAmcManager) {
    percentCallback = callback;
    amcManager = pAmcManager;
}

#define LINE_LENGTH 4096

#define MODULE_LIST_PATH "/proc/modules"

#define UPDATE_WAIT_TIME_US 10
#define BSMC_READY_TIMEOUT_S 5

#define FW_UPDATE_TYPE_STR(n) ( \
    (n == FW_UPDATE_TYPE_BSMC) ? "BSMC" : (n == FW_UPDATE_TYPE_CSMC_FULL) ? "CSMC FULL" : "unkown")

#define CHECK_FW_VERSION 0 // 0 - will not check the firmware version whereas 1 will check firmware version //

extern unsigned char gNetfn;
extern unsigned char gCmd;
extern uint8_t gData[267];
extern uint8_t gUpdateType;
extern unsigned short gSize;
extern uint8_t gReqData[300];

struct firmware_versions {
    fw_get_info_res bsmc;
    fw_get_info_res csmc_bootloader;
    fw_get_info_res csmc_service;
};

#ifdef _WIN32
static unsigned long getMin(unsigned long x, unsigned long y)
#else
static unsigned long min(unsigned long x, unsigned long y)
#endif
{
    return ((x < y) ? x : y);
}

static inline int shell(const char *cmd) {
#if defined(_WIN32) || defined(_MSC_VER)
    return system(cmd);
#else
    /*
     * WEXISTATUS translates system return code to exit code returned
     * by executed shell command.
     */
    int ret = system(cmd);
    return WEXITSTATUS(ret);
#endif
}

const char *get_product_name(uint8_t prod) {
    switch (prod) {
        case BOARD_PRODUCT_LCR:
            return "lcr";
        case BOARD_PRODUCT_SCR:
            return "scr";
        case BOARD_PRODUCT_SCR_PLUS:
            return "scr_plus";
    }
    return "Unknown";
}

const char *get_kernel_module_str(uint8_t prod) {
    switch (prod) {
        case BOARD_PRODUCT_LCR:
            return "nervana";
        case BOARD_PRODUCT_SCR:
        case BOARD_PRODUCT_SCR_PLUS:
        default:
            return "intel_nnp";
    }
}

/*
 * Send a buffer with bytes ordered from 0 to buffer size and check how many
 * ordered bytes were received by BSMC.
 */
static uint16_t detect_max_transfer_size(ipmi_address_t *addr) {
    unsigned short max_data_len = 30; /* works with BSMC v1.4.1.10+ */
    bsmc_req req;
    bsmc_res res;

    bsmc_hal->oem_req_init(&req, addr, IPMI_TRANSFER_SIZE_DETECT);

    /* Fill data with successive bytes starting from 0 */
    for (unsigned i = 0; i < sizeof(req.data); i++) {
        req.data[i] = i;
        gData[i] = req.data[i];
    }

    for (req.data_len = 32; req.data_len <= sizeof(req.data); req.data_len += 8) {
        gNetfn = IPMI_INTEL_OEM_NETFN;
        gCmd = IPMI_TRANSFER_SIZE_DETECT;

        if (bsmc_hal->cmd(&req, &res))
            break;

#if !(_WIN32) && !(__linux__)
        if (bsmc_hal->validate_res(res, SIZE_DETECT_RES))
#else
        if (bsmc_hal->validate_res(res, sizeof(res.size_detect_res)))
#endif
            break;

#if !(_WIN32) && !(__linux__)
        if (gMaxDataLen != req.data_len)
#else
        if (res.size_detect_res.received_bytes != req.data_len)
#endif
            break;

        max_data_len = req.data_len;
    }

    return max_data_len;
}

static int fw_get_info(ipmi_address_t *addr, fw_get_info_res *fw_info,
                       int chip_info_cmd) {
    bsmc_req req;
    bsmc_res res;

    if (!fw_info)
        return NRV_UNSPECIFIED_ERROR;

    if (chip_info_cmd != IPMI_FW_GET_INFO_CMD &&
        chip_info_cmd != IPMI_CSMC_BOOTLOADER_INFO_CMD &&
        chip_info_cmd != IPMI_CSMC_SERVICE_INFO_CMD)
        return NRV_UNSPECIFIED_ERROR;

    bsmc_hal->oem_req_init(&req, addr, chip_info_cmd);

    gNetfn = IPMI_INTEL_OEM_NETFN;
    gCmd = chip_info_cmd;

    if (bsmc_hal->cmd(&req, &res))
        return NRV_IPMI_ERROR;

    fw_info->completion_code = res.completion_code;
#if !(_WIN32) && !(__linux__)
    if (bsmc_hal->validate_res(res, SIZE_FW_GET_INFO_RES))
#else
    if (bsmc_hal->validate_res(res, sizeof(fw_get_info_res)))
#endif
        return NRV_IPMI_ERROR;

    *fw_info = res.fw_get_info;
    return NRV_SUCCESS;
}

static int get_fw_version(ipmi_address_t *addr, struct firmware_versions *fw_ver) {
    if (fw_get_info(addr, &fw_ver->bsmc, IPMI_FW_GET_INFO_CMD)) {
        XPUM_LOG_ERROR("Unable to get BSMC firmware info");
        return NRV_IPMI_ERROR;
    }

    return NRV_SUCCESS;
}

static int fw_update_start(ipmi_address_t *addr, uint8_t fw_update_type) {
    bsmc_req req;
    bsmc_res res;

    if (fw_update_type != FW_UPDATE_TYPE_BSMC &&
        fw_update_type != FW_UPDATE_TYPE_CSMC_FULL)
        return NRV_FIRMWARE_UPDATE_ERROR;

    bsmc_hal->oem_req_init(&req, addr, IPMI_FW_UPDATE_START_CMD);

    req.data_len = sizeof(fw_update_start_req);
    req.fw_update_start.fw_update_type = fw_update_type;

    gNetfn = IPMI_INTEL_OEM_NETFN;
    gCmd = IPMI_FW_UPDATE_START_CMD;
    gUpdateType = fw_update_type;

    if (bsmc_hal->cmd(&req, &res))
        return NRV_IPMI_ERROR;

    if (bsmc_hal->validate_res(res, COMPLETION_CODE_SIZE))
        return NRV_IPMI_ERROR;

    return NRV_SUCCESS;
}

static int fw_update_sync(ipmi_address_t *addr, uint32_t *offset, uint32_t *size,
                          uint8_t *status) {
    int retries = 30; /* Retry for 3 seconds*/
    bsmc_req req;
    bsmc_res res;

    bsmc_hal->oem_req_init(&req, addr, IPMI_FW_UPDATE_SYNC_CMD);

    /* BSMC needs some time for flash preparation with disabled interrupts */
retry:
    gNetfn = IPMI_INTEL_OEM_NETFN;
    gCmd = IPMI_FW_UPDATE_SYNC_CMD;
    if (bsmc_hal->cmd(&req, &res) ||
#if !(__linux__) && (_MSC_VER < 1910)
        bsmc_hal->validate_res(res, SIZE_FW_UPDATE_SYNC_RES)) {
#else
        bsmc_hal->validate_res(res, sizeof(fw_update_sync_res))) {
#endif
        if (retries) {
            do_sleep(WAIT_100_MS);
            retries--;
            goto retry;
        }
        return NRV_IPMI_ERROR;
    }

    *status = res.fw_update_sync.status;
    if (*status == IPMI_FW_UPDATE_READ ||
        *status == IPMI_FW_UPDATE_GET_FILE_SIZE) {
        *offset = res.fw_update_sync.offset;
        *size = res.fw_update_sync.size;
    }

    return NRV_SUCCESS;
}

static int fw_update_transfer(ipmi_address_t *addr, unsigned short max_data_len,
                              const uint8_t *data, size_t data_size, uint8_t *status) {
    XPUM_LOG_INFO("Start transfer");
    bsmc_req req;
    bsmc_res res;
    int err;
    uint32_t offset = 0;
    uint32_t size = 0;

#if !(__linux__) && (_MSC_VER < 1910)
    UINT32 Index;
#endif

    bsmc_hal->oem_req_init(&req, addr, IPMI_FW_UPDATE_SEND_DATA_CMD);

    while (true) {
        /* Check FW update status */
        err = fw_update_sync(addr, &offset, &size, status);
        if (err) {
            XPUM_LOG_INFO("Retry with slave addr 0xce");
            // retry with new I2C addr
            addr->i2c_addr = CARD_FIRST_I2C_ADDR;
            err = fw_update_sync(addr, &offset, &size, status);
            if (err){
                XPUM_LOG_ERROR("Fail to fw_update_sync, err {}", err);
                goto exit;
            }
        }

        if (*status == IPMI_FW_UPDATE_WAIT) {
            do_sleep(UPDATE_WAIT_TIME_US);
            continue;
        } else if (*status == IPMI_FW_UPDATE_GET_FILE_SIZE) {
            req.data_len = size;
            memcpy(req.data, &data_size, size);
            gNetfn = IPMI_INTEL_OEM_NETFN;
            gCmd = IPMI_FW_UPDATE_SEND_DATA_CMD;
            err = bsmc_hal->cmd(&req, &res);
            if (err){
                XPUM_LOG_ERROR("Fail to do command IPMI_FW_UPDATE_GET_FILE_SIZE, err {}", err);
                goto exit;
            }
            continue;
        } else if (*status != IPMI_FW_UPDATE_READ) {
            XPUM_LOG_ERROR("go to exit, status {}", *status);
            goto exit;
        }

        if ((offset + size) > data_size) {
            XPUM_LOG_ERROR("Unexpected end of firmware image");
            err = NRV_INVALID_FIRMWARE_IMAGE;
            goto exit;
        }

        while (size > 0) {
#ifdef _WIN32
            req.data_len = getMin(size, max_data_len);
#else
            req.data_len = min(size, max_data_len);
#endif
            memcpy(req.data, &data[offset], req.data_len);

#if !(__linux__) && (_MSC_VER < 1910)
            gFwReqSize = (UINT32)req.data_len;
            for (Index = 0; Index < gFwReqSize; Index++) {
                gFwReqData[Index] = req.data[Index];
            }
#endif

            gNetfn = IPMI_INTEL_OEM_NETFN;
            gCmd = IPMI_FW_UPDATE_SEND_DATA_CMD;
            gSize = req.data_len;
            for (int index = 0; index < gSize; index++)
                gReqData[index] = req.data[index];

            err = bsmc_hal->cmd(&req, &res);
            if (err) {
                XPUM_LOG_ERROR("Error during send data, err {}", err);
                err = NRV_FIRMWARE_UPDATE_ERROR;
                goto exit;
            }

            if (bsmc_hal->validate_res(res, COMPLETION_CODE_SIZE)) {
                XPUM_LOG_ERROR("Error validate ipmi response");
                err = NRV_FIRMWARE_UPDATE_ERROR;
                goto exit;
            }

            size -= req.data_len;
            offset += req.data_len;
        }

        if (percentCallback) {
            int percent = (fw_update_device_index * 100 + (offset * 100) / data_size) / fw_update_device_count;
            percentCallback(percent, amcManager);
        }

        //log_progress_next();
    }

exit:
    return err;
}

static int pci_reset_devices(pci_address_t *address, int pci_address_count) {
    int err = NRV_SUCCESS;

    for (int i = 0; i < pci_address_count; i++) {
        err = reset_pci_device(&address[i]);
        if (err)
            return err;
    }

    return err;
}

static int discover_pci_address_list(nrv_list *cards, pci_address_t *pci_address, int *pci_address_count) {
    int err = NRV_SUCCESS;
    nrv_card *card;
    bsmc_req req;
    bsmc_res res;
    int count = 0;

    *pci_address_count = 0;
    /* Check if ipmi cards contains PCI address */
    for (int i = 0; i < cards->count; i++) {
        card = &cards->card[i];
        if (!card->pci_address_valid) {
            bsmc_hal->oem_req_init(&req, &card->ipmi_address, IPMI_CARD_GET_INFO_CMD);
            gNetfn = IPMI_INTEL_OEM_NETFN;
            gCmd = IPMI_CARD_GET_INFO_CMD;
            if (bsmc_hal->cmd(&req, &res))
                err = NRV_IPMI_ERROR;

            if (bsmc_hal->validate_res(res, CARD_GET_INFO_RES_MIN_SIZE))
                err = NRV_IPMI_ERROR;

            if (err)
                break;

            /* Check if resposne contains bar0_address */
            if (res.data_len == sizeof(card_get_info_res) && res.card_get_info.bar0_address)
                get_pci_device_by_bar0_address(res.card_get_info.bar0_address, &card->pci_address);

            if (check_pci_device(&card->pci_address))
                card->pci_address_valid = true;
            else
                return NRV_IPMI_ERROR;
        }

        pci_address[i] = card->pci_address;
        count++;
    }
    *pci_address_count = count;

    return err;
}

static int check_image(const char *file) {
    struct stat st;

    if (stat(file, &st)) {
        XPUM_LOG_ERROR("Firmware Image {} does not exist", file);
        return NRV_INVALID_FIRMWARE_IMAGE;
    }

    if (!(st.st_mode & S_IFREG)) {
        XPUM_LOG_ERROR("Firmware Image {} is not regular file", file);
        return NRV_INVALID_FIRMWARE_IMAGE;
    }

    return NRV_SUCCESS;
}

static int fw_update(nrv_card *card, const uint8_t *data, size_t data_size, fw_get_info_res *version,
                     uint8_t fw_update_type) {
    uint8_t chip_status = 0;
    int err;

    XPUM_LOG_INFO("Initializing {} firmware update",
                  FW_UPDATE_TYPE_STR(fw_update_type));
    XPUM_LOG_INFO("Actual {} firmware version {}.{}.{}.{}", FW_UPDATE_TYPE_STR(fw_update_type),
                  std::to_string(version->major), std::to_string(version->minor), std::to_string(version->patch), std::to_string(version->build));

    if (fw_update_start(&card->ipmi_address, fw_update_type)) {
        XPUM_LOG_ERROR("{} firmware update initialization failed",
                       FW_UPDATE_TYPE_STR(fw_update_type));
        return NRV_FIRMWARE_UPDATE_ERROR;
    }

    XPUM_LOG_INFO("Updating {} on card {}", FW_UPDATE_TYPE_STR(fw_update_type), card->id);

    err = fw_update_transfer(&card->ipmi_address, card->max_transfer_len, data, data_size, &chip_status);
    if (err)
        return err;

    switch (chip_status) {
        case IPMI_FW_UPDATE_COMPLETE:
            XPUM_LOG_INFO("{} image transfer completed",
                          FW_UPDATE_TYPE_STR(fw_update_type));
            break;
        case IPMI_FW_UPDATE_FAIL:
            XPUM_LOG_ERROR("{} firmware update failed",
                           FW_UPDATE_TYPE_STR(fw_update_type));
            err = NRV_FIRMWARE_UPDATE_ERROR;
            break;
        case IPMI_FW_UPDATE_SIGNATURE_FAIL:
            XPUM_LOG_ERROR("{} firmware signature verification failed",
                           FW_UPDATE_TYPE_STR(fw_update_type));
            err = NRV_FIRMWARE_VERIFICATION_ERROR;
            break;
        case IPMI_FW_UPDATE_IMAGE_TO_LARGE_FAIL:
            XPUM_LOG_ERROR("{} firmware image too large",
                           FW_UPDATE_TYPE_STR(fw_update_type));
            err = NRV_FIRMWARE_UPDATE_ERROR;
            break;
        case IPMI_FW_UPDATE_NO_IMAGE_SIZE_FAIL:
            XPUM_LOG_ERROR("{} firmware image has invalid header",
                           FW_UPDATE_TYPE_STR(fw_update_type));
            err = NRV_FIRMWARE_UPDATE_ERROR;
            break;
        case IPMI_FW_UPDATE_PACKET_TO_LARGE_FAIL:
            XPUM_LOG_ERROR("{} firmware packet transfer is too large",
                           FW_UPDATE_TYPE_STR(fw_update_type));
            err = NRV_FIRMWARE_UPDATE_ERROR;
            break;
        case IPMI_FW_UPDATE_TO_MANY_RETRIES_FAIL:
            XPUM_LOG_ERROR("{} firmware transfer too many retries",
                           FW_UPDATE_TYPE_STR(fw_update_type));
            err = NRV_FIRMWARE_UPDATE_ERROR;
            break;
        case IPMI_FW_UPDATE_WRITE_TO_FLASH_FAIL:
            XPUM_LOG_ERROR("{} firmware write to flash failed",
                           FW_UPDATE_TYPE_STR(fw_update_type));
            err = NRV_FIRMWARE_UPDATE_ERROR;
            break;
        default:
            XPUM_LOG_ERROR("{} unknown chip status received: {}",
                           FW_UPDATE_TYPE_STR(fw_update_type), chip_status);
            err = NRV_FIRMWARE_UPDATE_ERROR;
            break;
    }

    return err;
}

static int wait_for_bsmc(ipmi_address_t *addr, fw_get_info_res prev_ver) {
    int retries = BSMC_READY_TIMEOUT_S;
    fw_get_info_res curr_ver;

    while (retries) {
        if (fw_get_info(addr, &curr_ver, IPMI_FW_GET_INFO_CMD)) {
            XPUM_LOG_ERROR("Unable to get BSMC firmware info");
            return NRV_IPMI_ERROR;
        }

        /*
         * If PCI reset is done properly than BSMC execute chip reset.
         * After sucessfull fw update BSMC chip reset switch partition.
         */
        if (curr_ver.partition != prev_ver.partition)
            return NRV_SUCCESS;

        do_sleep(WAIT_1_S);
        retries--;
    }

    return NRV_REBOOT_NEEDED;
}

static int cmd_firmware_update(nrv_list cards, uint8_t *bsmc_data, size_t bsmc_size) {
    int err = NRV_SUCCESS;
    struct firmware_versions prev_ver[MAX_CARD_NO] = {{{0}}};
    pci_address_t pci_address[MAX_CARD_NO];
    int pci_address_count = 0;
    bool reset_failed = false;
    fw_update_device_index = 0;
    fw_update_device_count = cards.count;
    if (percentCallback) {
        percentCallback(0, amcManager);
    }

#if __linux__
    if (discover_pci_address_list(&cards, pci_address, &pci_address_count)) {
        err = get_pci_device_list(pci_address, MAX_CARD_NO,
                                  &pci_address_count);
        if (err)
            return err;
    }
#endif

#if 0
	err = check_if_nervana_is_unloaded(cards.card[0].board_product);
	if (err)
		return err;
#endif

    /* BSMC firmware update */
    //TODO for all cards:
    for (int i = 0; i < cards.count; i++) {
        //if ( cards.count > 0 ) { //only one card now
        nrv_card *card = &cards.card[i];

        fw_update_device_index = i;

        err = get_fw_version(&card->ipmi_address, &prev_ver[i]);
        if (err)
            goto exit;

        card->max_transfer_len = detect_max_transfer_size(&card->ipmi_address);

        if (bsmc_data) {
            err = fw_update(card, bsmc_data, bsmc_size, &prev_ver[i].bsmc,
                            FW_UPDATE_TYPE_BSMC);
            if (err)
                goto exit;
        }
    }

#if 0
	/* CSMC firmware update */
	if (spi_update_count > 0) {
		err = csmc_spi_update(csmc_data, csmc_size, spi_update_cards, spi_update_count);
		if (err)
			goto exit;
	}
#endif

    /*if (rediscover_pci_addr) { // Re-validate PCI address list
            if (discover_pci_address_list(&cards, pci_address, &pci_address_count)) {
                    err = get_pci_device_list(pci_address, sizeof(pci_address),
                            &pci_address_count);
                    if (err)
                            pci_address_count = 0;
            }
    }

    if (pci_address_count == 0) {
            log_verbose("Mismatch between number of PCI devices and IPMI devices!\n");
            err = fw_update_ipmi_reset(cards);
            do_sleep(WAIT_1_S); // Wait for BSMC to complete reset
            if (err)
                    reset_failed = true;
    }*/

    if (!err) {
        XPUM_LOG_INFO("fw_update is successful");
    } else {
        /* PCI reset on all known cards */
        err = pci_reset_devices(pci_address, pci_address_count);
        if (err)
            goto exit;
    }

    /* Firmware update completion check */
    //TODO for all cards:
    for (int i = 0; i < cards.count; i++) {
        /*
    if ( cards.count > 0 ) {
        int i = 0;
        */
        struct firmware_versions curr_ver = {{0}};

        XPUM_LOG_INFO("card {} i2c_addr is: 0x{:x}", i, cards.card[i].ipmi_address.i2c_addr);

        if (bsmc_data) {
            err = wait_for_bsmc(&cards.card[i].ipmi_address, prev_ver[i].bsmc);
            if (err) {
                XPUM_LOG_INFO("card {} wait_for_bsmc retry with i2c_addr 0x{:x}", i, CARD_FIRST_I2C_ADDR);
                cards.card[i].ipmi_address.i2c_addr = CARD_FIRST_I2C_ADDR;
                err = wait_for_bsmc(&cards.card[i].ipmi_address, prev_ver[i].bsmc);
            }
            if (err == NRV_REBOOT_NEEDED) {
                XPUM_LOG_INFO("card {} wait_for_bsmc return error NRV_REBOOT_NEEDED", i);
                reset_failed = true;
            } else if (err) {
                XPUM_LOG_ERROR("card {} wait_for_bsmc fail with i2c_addr 0x{:x}", i, cards.card[i].ipmi_address.i2c_addr);
                goto exit;
            }
        }

        err = get_fw_version(&cards.card[i].ipmi_address, &curr_ver);
        if (err){
            XPUM_LOG_ERROR("card {} get_fw_version fail with i2c_addr 0x{:x}, err {}", i, cards.card[i].ipmi_address.i2c_addr, err);
            goto exit;
        }

        if (bsmc_data && !reset_failed)
            XPUM_LOG_INFO("BSMC updated on card {} to version {}.{}.{}.{}",
                          std::to_string(cards.card[i].id), std::to_string(curr_ver.bsmc.major),
                          std::to_string(curr_ver.bsmc.minor), std::to_string(curr_ver.bsmc.patch),
                          std::to_string(curr_ver.bsmc.build));
    }
exit:
    // clean discovered cards, since fw update may change I2C addr
    clean_data();
    if (err)
        XPUM_LOG_ERROR("Failed to update firmware");
    if (reset_failed) {
        XPUM_LOG_WARN("PLEASE do HOST POWER CYCLE to complete update process");
        return NRV_REBOOT_NEEDED;
    }
    return err;
}

static int cmd_firmware_info(nrv_list cards, unsigned int versions[4]) {
    int err = NRV_UNSPECIFIED_ERROR;
    struct firmware_versions fw_ver = {{0}};

    //TODO for all card: for (int i = 0; i < cards.count; i++) {
    if (cards.count > 0) {
        int i = 0;
        err = get_fw_version(&cards.card[i].ipmi_address, &fw_ver);
        if (err)
            return err;

        XPUM_LOG_INFO("BSMC firmware version: {}.{}.{}.{}",
                      std::to_string(fw_ver.bsmc.major), std::to_string(fw_ver.bsmc.minor), std::to_string(fw_ver.bsmc.patch),
                      std::to_string(fw_ver.bsmc.build));
        versions[0] = fw_ver.bsmc.major;
        versions[1] = fw_ver.bsmc.minor;
        versions[2] = fw_ver.bsmc.patch;
        versions[3] = fw_ver.bsmc.build;

        /*
        if (fw_ver.csmc_bootloader.completion_code == IPMI_CC_SUCCESS) {
            log_info("  CSMC bootloader firmware version: %u.%u.%u.%u\n",
                     fw_ver.csmc_bootloader.major, fw_ver.csmc_bootloader.minor,
                     fw_ver.csmc_bootloader.patch, fw_ver.csmc_bootloader.build);
        } else {
            log_warning("CSMC bootloader version not available\n");
        }

        if (fw_ver.csmc_bootloader.completion_code == IPMI_CC_SUCCESS) {
            log_info("  CSMC service firmware version: %u.%u.%u.%u\n",
                     fw_ver.csmc_service.major, fw_ver.csmc_service.minor,
                     fw_ver.csmc_service.patch, fw_ver.csmc_service.build);
        } else {
            log_warning("CSMC service version not available\n");
        }
        */
    }

    return err;
}

int cmd_test_update_sync() {
    int err;
    uint32_t offset = 0;
    uint32_t size = 0;
    ipmi_address_t addr;
    uint8_t status;

    nrv_list cards{};

    int card_id = CARD_SELECT_ALL;

    err = get_card_list(&cards, card_id);
    addr = cards.card[0].ipmi_address;
    err = fw_update_sync(&addr, &offset, &size, &status);
    return err;
}

int cmd_probe() {
    int card_id = CARD_SELECT_ALL;

    nrv_list cards{};

    int err = get_card_list(&cards, card_id);

    if (err)
        return err;

    unsigned int versions[4];

    err = cmd_firmware_info(cards, versions);

    return err;
}

int cmd_get_amc_firmware_versions(int buf[][4], int *count) {
    int err = NRV_SUCCESS;
    int card_id = CARD_SELECT_ALL;

    nrv_list cards{};
    err = get_card_list(&cards, card_id);
    if (err)
        return err;

    if (buf == nullptr) {
        *count = cards.count;
        return 0;
    }

    if (*count < cards.count) {
        // buffer too small
        return -1;
    }

    *count = 0;
    //std::cout << "card count" << cards.count << "\n";
    for (int i = 0; i < cards.count; i++) {
        struct firmware_versions fw_ver = {{0}};
        err = get_fw_version(&cards.card[i].ipmi_address, &fw_ver);
        if (err)
            continue;

        buf[i][0] = fw_ver.bsmc.major;
        buf[i][1] = fw_ver.bsmc.minor;
        buf[i][2] = fw_ver.bsmc.patch;
        buf[i][3] = fw_ver.bsmc.build;
        (*count)++;
    }
    return 0;
}

int cmd_firmware(const char* file, unsigned int versions[4]) {
#ifdef XPUM_FIRMWARE_MOCK
   // return cmd_firmware_mock(file, versions);
#endif
    const char *bsmc_file = file;
    uint8_t *bsmc_data = NULL;
    size_t bsmc_size = 0;
    int err = NRV_SUCCESS;
    int card_id = CARD_SELECT_ALL;

    nrv_list cards{};

    err = get_card_list(&cards, card_id);
    if (err)
        goto exit;

    //#if IS_DEBUG()
    if (bsmc_file) {
        // check image version before update //
#if CHECK_FW_VERSION
        err = check_image_version(cards, bsmc_file);
        if (err != NRV_SUCCESS) {
            log_info("Firmware update discarded as version of image either older or same as version of firmware.\n");
            goto exit;
        } else {
            bsmc_data = 0;
        }
#else
        bsmc_data = 0;
#endif
        err = check_image(bsmc_file);
        if (err)
            goto exit;
        bsmc_data = read_file(bsmc_file, &bsmc_size);
        if (!bsmc_data) {
            err = NRV_INVALID_FIRMWARE_IMAGE;
            goto exit;
        }
    }

    if (bsmc_data) {
        err = cmd_firmware_update(cards, bsmc_data, bsmc_size);
    } else {
        err = cmd_firmware_info(cards, versions);
    }

exit:
    if (bsmc_data)
        free(bsmc_data);
    return err;
}
} // namespace xpum
