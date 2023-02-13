/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file tool.h
 */

#pragma once
#include <assert.h>
#include <stdbool.h>
#include <vector>

#include "bsmc_interface.h"
#include "hal.h"
#include "infrastructure/logger.h"
#include "ipmi_interface.h"
#include "sdr.h"

namespace xpum {
#define NERVANA_MAX_NEIGHBOURS 16
#define MAX_CARD_NO 32
#define CSV_MAX_BUFFSIZE 2048
#define ERRNO_SIZE_MAX 1024

#define VERSION "0.0.0.0"

#define IS_DEBUG() (DEBUG == 1)

#define WAIT_100_MS 100
#define WAIT_1_S 1000

typedef struct {
    char *buf;
    int buf_len;
} csv_buffer;

typedef struct {
    int node_id;
    bool initialized;
    int peer_node[NERVANA_MAX_NEIGHBOURS];
    uint8_t peer_icg[NERVANA_MAX_NEIGHBOURS];
} icl_info_t;

typedef struct {
    int id;
    ipmi_address_t ipmi_address;
    pci_address_t pci_address;
    bool ipmi_address_valid;
    bool pci_address_valid;
    uint8_t project_codename[3];
    uint8_t board_product;
    uint8_t board_revision;
    uint8_t board_sku;
    uint16_t max_transfer_len;
    icl_info_t icl_info;
    bool sensor_filtered[SENSOR_COUNT];
    bool sensors_initialized;
    std::vector<ipmi_buf> sdr_list;
} nrv_card;

typedef struct {
    nrv_card card[MAX_CARD_NO];
    int count;
} nrv_list;

struct nrv_portgroup {
    char name[16];
    int first_port;
    int num_ports;
};

extern bsmc_hal_t *bsmc_hal;
#define CARD_SELECT_ALL (-1)

/* nnptool return codes */
#define NRV_SUCCESS 0
#define NRV_UNSPECIFIED_ERROR 1
#define NRV_REBOOT_NEEDED 3
#define NRV_NO_SPECIFIED_CARD_DETECTED 4
#define NRV_IPMI_ERROR 5
#define NRV_INVALID_FRU 6
#define NRV_FIRMWARE_UPDATE_ERROR 7
#define NRV_INVALID_FIRMWARE_IMAGE 8
#define NRV_FIRMWARE_VERIFICATION_ERROR 9
#define NRV_PCI_ERROR 10
#define NRV_NO_CARD_DETECTED 11
#define NRV_INVALID_ARGUMENT 12
#define NRV_NET_ERROR 13
#define NRV_NO_SPI_INTERFACE 14
#define NRV_COMMAND_NOT_EXIST 127

int set_bsmc_interface(char *iface_str);
int get_card_list(nrv_list *out_list, int select);
int get_total_ipmi_card_count();
int get_sensor(ipmi_address_t *ipmi_address, uint8_t sensor_id,
               read_sensor_res *out_sensor);
//nnp_hal* get_device_hal(nrv_card *card);
void print_ipmi(ipmi_address_t *ipmi_address, bool is_valid);
int cmd_firmware(int argc, char **argv);
int cmd_fruinfo(int argc, char **argv);
int cmd_discover(int argc, char **argv);
int cmd_info(int argc, char **argv);
int cmd_sensor(int argc, char **argv);
int cmd_log(int argc, char **argv);
int cmd_net(int argc, char **argv);
int cmd_version(int argc, char **argv);
int cmd_modes(int argc, char **argv);
void do_sleep(int sleep_time_in_ms);
void clean_data();

} // namespace xpum