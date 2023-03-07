/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file bsmc_interface.h
 */

#pragma once

#ifdef __linux__
#include <linux/ipmi.h>
#endif

#include <stdint.h>

#include "bsmc_ipmi_oem_cmd.h"
#include "bsmc_ipmi_storage_cmd.h"

namespace xpum {

#define REQUEST_HEADER_SIZE 5                /* SlotIPMB netfn, cmd and address */
#define RESPONSE_HEADER_SIZE sizeof(uint8_t) /* SlotIPMB completion code */
#define COMPLETION_CODE_SIZE sizeof(uint8_t) /* Response completion code */

#define CARD_FIRST_I2C_ADDR_OLD 0xb0
#define CARD_FIRST_I2C_ADDR 0xce
#define MAX_PCI_SLOT_COUNT 0x40

#define MAX_PCI_SLOT_COUNT_OPEN_BMC 0x08
#define MAX_PCI_SLOT_COUNT_OPEN_PURELY 0x40
#define DEVICE_ID_DATA_SIZE 16
#define OPEN_BMC_DEV_ID 0x23
#define PURELY_DEV_ID 0x22

/* SlotIPMB response completion code */
#define IPMI_CC_SUCCESS 0x00
#define IPMB_CC_BUS_ERROR 0x82
#define IPMB_CC_NAK_ON_WRITE 0x83
#define IPMB_CC_INVALID_PCIE_SLOT_NUM 0x85
#define IPMI_CC_BUSY 0xc0
#define IPMI_CC_INVALID_COMMAND 0xc1
#define IPMI_CC_INV_DATA_FIELD_IN_REQ 0xcc

/* Read sensor cmd completion code */
#define IPMB_CC_SENSOR_NOT_PRESENT 0xcb

#if (_WIN32 || _MSC_VER)
#ifndef IPMI_MAX_MSG_LENGTH
#define IPMI_MAX_MSG_LENGTH 272 /* multiple of 16 */
#endif
#endif
#if !(_MSC_VER < 1910)
#pragma pack(1)
#endif

typedef enum {
    IPMI = 0,
    PCI = 1,
    SERIAL = 2
} bsmc_interface_t;

int bsmc_interface_init(bsmc_interface_t iface);

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    uint8_t bus;
    uint8_t slot;
    uint8_t i2c_addr;
} ipmi_address_t;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
    ipmi_address_t ipmi_address;
    uint8_t netfn;
    uint8_t cmd;
    union {
        uint8_t data[IPMI_MAX_MSG_LENGTH - REQUEST_HEADER_SIZE];
        fw_update_start_req fw_update_start;
        fru_get_area_info_req_t fru_area_info;
        fru_read_data_req_t fru_read;
        read_sensor_req read_sensor;
        sel_get_entry_req_t sel_entry;
        sel_clear_req_t sel_clear;
        sel_set_time_req_t sel_set_time;
        icl_init_req icl_init;
        debug_req debug;
        card_set_info_req set_info;
    };
    unsigned short data_len;
} bsmc_req;

#if __linux__
typedef struct __attribute__((packed))
#else
typedef struct
#endif
{
#if __linux__
    uint8_t slot_ipmb_completion_code;
#endif
    union {
        struct {
            uint8_t completion_code;
            uint8_t data[IPMI_MAX_MSG_LENGTH -
                         RESPONSE_HEADER_SIZE -
                         COMPLETION_CODE_SIZE];
        };
        fw_update_start_res fw_update_start;
        fw_update_sync_res fw_update_sync;
        card_get_info_res card_get_info;
        fw_get_info_res fw_get_info;
        fru_get_area_info_resp_t fru_area_info;
        fru_read_data_resp_t fru_read;
        read_sensor_res read_sensor;
        sel_get_info_resp_t sel_info;
        sel_get_entry_resp_t sel_entry;
        sel_clear_resp_t sel_clear;
        sel_get_time_resp_t sel_get_time;
        icl_status_res icl_status;
        icl_data_res icl_data;
        icl_read_res icl_read;
        debug_res debug;
        transfer_size_detect_res size_detect_res;
    };
    unsigned short data_len;
} bsmc_res;

typedef struct {
    int (*init)();
    int (*cmd)(bsmc_req *req, bsmc_res *res);
    int (*validate_res)(bsmc_res res, uint16_t res_size);
    void (*oem_req_init)(bsmc_req *req, void *addr, uint8_t cmd);
} bsmc_hal_t;
} // namespace xpum
