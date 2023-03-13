/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi.cpp
 */

#include "ipmi.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <stdlib.h>

#include "bsmc_ipmi_oem_cmd.h"
#include "hal.h"
#include "lcr.h"
#include "pci.h"
#include "scr.h"
#include "tool.h"
#include "sensor_reading.h"

#if !(__linux__) && (_MSC_VER < 1910)
#include <efi.h>
#include <unistd.h>
#pragma warning(disable : 4204)
#endif

namespace xpum {

static nrv_list g_list;
static bsmc_interface_t iface;

#define NNP_LEGACY_CODENAME "LCR"

//extern nnp_hal lcr_hal;
//extern nnp_hal scr_hal;
extern bsmc_hal_t *bsmc_hal;

extern unsigned char gNetfn;
extern unsigned char gCmd;

/*
nnp_hal *ops_table[NUM_BOARD_PRODUCTS] = {
    &lcr_hal,
    &scr_hal,
    &scr_hal,
};
*/

void do_sleep(int sleep_time_in_ms) {
#ifdef _WIN32
    Sleep(sleep_time_in_ms);
#else
    usleep(sleep_time_in_ms * 1000);
#endif
}

/*
nnp_hal *get_device_hal(nrv_card *card) {
    assert(card->board_product < NUM_BOARD_PRODUCTS);
    return ops_table[card->board_product];
}
*/

static inline bool check_codename(card_get_info_res *card_get_info, const char *codename) {
    const size_t size = sizeof(card_get_info->project_codename);
    return memcmp(card_get_info->project_codename, codename, size) == 0;
}

static int get_device_id(nrv_card *card, unsigned char *data) {
    bsmc_req req{};
    bsmc_res res{};

    bsmc_hal->oem_req_init(&req, &card->ipmi_address, IPMI_CARD_GET_INFO_CMD);

    gNetfn = IPMI_GET_DEVID_OEM_NETFN;
    gCmd = IPMI_CARD_GET_INFO_CMD;

    if (bsmc_hal->cmd(&req, &res))
        return NRV_IPMI_ERROR;

    *data = res.completion_code;

    return NRV_SUCCESS;
}

static int card_detect(nrv_card *card) {
    bsmc_req req{};
    bsmc_res res{};
    bsmc_hal->oem_req_init(&req, &card->ipmi_address, IPMI_CARD_GET_INFO_CMD);

    gNetfn = IPMI_INTEL_OEM_NETFN;
    gCmd = IPMI_CARD_GET_INFO_CMD;

    if (bsmc_hal->cmd(&req, &res))
        return NRV_IPMI_ERROR;

    if (bsmc_hal->validate_res(res, CARD_GET_INFO_RES_MIN_SIZE))
        return NRV_IPMI_ERROR;

    /* check for nervana card magic string. Old LCR firmware will report
     * "LCR" string, newer firmware will report "NNP" for all products */
    if (!check_codename(&res.card_get_info, NNP_PROJECT_CODENAME) &&
        !check_codename(&res.card_get_info, NNP_LEGACY_CODENAME)) {
        return NRV_IPMI_ERROR;
    }

    memcpy(card->project_codename, res.card_get_info.project_codename,
           sizeof(card->project_codename));

    if (res.card_get_info.protocol > VERSION_PROTOCOL_1)
        XPUM_LOG_WARN(
            "Unsupported protocol version. Please match "
            "XPUM version to actual firmware version");

    if (res.card_get_info.board_product >= NUM_BOARD_PRODUCTS) {
        XPUM_LOG_WARN("Unknown card at Bus:{}, PCI Slot:{}, I2C Addr:0x{}",
                      card->ipmi_address.bus, card->ipmi_address.slot,
                      card->ipmi_address.i2c_addr);
        return NRV_IPMI_ERROR;
    }

    card->board_product = res.card_get_info.board_product;
    card->board_revision = res.card_get_info.board_revision;
    card->board_sku = res.card_get_info.board_sku;
    card->pci_address = res.card_get_info.pci_address;

    /* Check if resposne contains bar0_address */
    if (res.data_len == sizeof(card_get_info_res) && res.card_get_info.bar0_address)
        get_pci_device_by_bar0_address(res.card_get_info.bar0_address, &card->pci_address);

    card->pci_address_valid = false;
    card->ipmi_address_valid = false;
    card->sensors_initialized = false;

    if (iface == IPMI)
        card->ipmi_address_valid = true;
    if (check_pci_device(&card->pci_address))
        card->pci_address_valid = true;

    return NRV_SUCCESS;
}


static int init_card_list() {
    int err = 0;
    err = bsmc_interface_init(iface);
    if (err)
        return err;

    nrv_card card{};
    unsigned char devid = 0;
    uint8_t slot_count = 0;

    card.ipmi_address = (ipmi_address_t){
        .bus = 0,
        .slot = 0,
        .i2c_addr = CARD_FIRST_I2C_ADDR_OLD,
    };

    err = get_device_id(&card, &devid);
    if (err) {
        XPUM_LOG_ERROR("Error in getting device id");
    }

    if (devid == OPEN_BMC_DEV_ID) {
        XPUM_LOG_DEBUG("OPEN BMC platform found");
        slot_count = MAX_PCI_SLOT_COUNT_OPEN_BMC;
    } else if (devid == PURELY_DEV_ID) {
        XPUM_LOG_DEBUG("PURELY platform found");
        slot_count = MAX_PCI_SLOT_COUNT_OPEN_PURELY;
    } else {
        XPUM_LOG_DEBUG("UNKNOWN platform found");
        slot_count = MAX_PCI_SLOT_COUNT;
    }

    uint8_t slaveAddrs[]{CARD_FIRST_I2C_ADDR_OLD, CARD_FIRST_I2C_ADDR};

    for (auto i2c_addr : slaveAddrs) {
        for (uint8_t slot = 0; slot < slot_count; slot++) {
            card.ipmi_address = (ipmi_address_t){
                .bus = 0,
                .slot = slot,
                .i2c_addr = i2c_addr,
            };

            err = card_detect(&card);
            if (err)
                continue;
            // get sdr list
            get_sdr_list(card);
            g_list.card[g_list.count] = card;
            g_list.card[g_list.count].id = g_list.count;

            g_list.count++;
        }
    }

    if (g_list.count)
        return NRV_SUCCESS;

    return NRV_NO_CARD_DETECTED;
}

/* Returns 0 if there is at least one card in system. */
int get_card_list(nrv_list *out_list, int select) {
    int err = 0;
    if (!g_list.count) {
        err = init_card_list();
        if (err) {
            if (err == NRV_NO_CARD_DETECTED) {
                XPUM_LOG_ERROR("No available AMC card in system.");
            }
            return err;
        }
    }

    if (select == CARD_SELECT_ALL) {
        *out_list = g_list;
    } else {
        if (select < g_list.count) {
            out_list->card[0] = g_list.card[select];
            out_list->count++;
        } else {
            XPUM_LOG_ERROR("Card {} does not exist.", select);
            err = NRV_NO_SPECIFIED_CARD_DETECTED;
        }
    }
    return err;
}

int set_bsmc_interface(char *iface_str) {
    if (iface_str == NULL)
        return NRV_INVALID_ARGUMENT;
    else if (!strcmp(iface_str, "ipmi"))
        iface = IPMI;
    else
        return NRV_INVALID_ARGUMENT;
    return NRV_SUCCESS;
}

inline int get_total_ipmi_card_count() {
    return g_list.count;
}

void clean_data() {
    g_list.count = 0;
}
} // namespace xpum
