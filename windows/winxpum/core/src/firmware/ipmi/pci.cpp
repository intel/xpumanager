/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pci.cpp
 */

/*
 * INTEL CONFIDENTIAL
 * Copyright 2018 Intel Corporation
 *
 * This software and the related documents are Intel copyrighted materials,
 * and your use of them is governed by the express license under which they
 * were provided to you (License). Unless the License provides otherwise,
 * you may not use, modify, copy, publish, distribute, disclose or
 * transmit this software or the related documents without
 * Intel's prior written permission.
 *
 * This software and the related documents are provided as is,
 * with no express or implied warranties, other than those
 * that are expressly stated in the License.
 */
#include "pch.h"
#ifdef _WIN32
#include <io.h>
#pragma warning(disable : 4996)
#else
#include <linux/pci.h>
#include <unistd.h>
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file_util.h"
#include "pci.h"
#include "pci_address.h"
#include "tool.h"

using namespace std;
namespace xpum {

#ifndef F_OK
#define F_OK 0
#endif

#define LINE_LENGTH 4096

#define PROC_PCI_DEVICES_FILE "/proc/bus/pci/devices"

#define NRV_PCI_VENDOR "0x8086"
#define NRV_PCI_VENDOR_ID 0x8086
#define PCI_DEVICE_ID_LCR 0x09d1
#define PCI_DEVICE_ID_SCR 0x4200
#define PCI_DEVICE_ID_SCR_MAX 0x4203
#define PCI_DEVICE_ID_SCRPLUS 0x4204
#define PCI_DEVICE_ID_SCRPLUS_MAX 0x420f

#define NEW_PCI_DEVICE_ID 0x2020

#define PCI_ID_SIZE 6

#define SYSFS_PCI_DEVICE_PATH_FORMAT "/sys/bus/pci/devices/" BDF_FORMAT
#define SYSFS_PCI_DEVICE_VALUE_FORMAT SYSFS_PCI_DEVICE_PATH_FORMAT "/%s"
#define SYSFS_PCI_VENDOR "vendor"
#define SYSFS_PCI_DEVICE "device"
#define SYSFS_PCI_RESET "reset"
#define PATH_MAX 1024

typedef struct {
    pci_address_t address;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t bar0;
} pci_properties_t;

static bool is_pci_crest_device_id(uint32_t device_id) {
    if (device_id == PCI_DEVICE_ID_LCR ||
        (device_id >= PCI_DEVICE_ID_SCR && device_id <= PCI_DEVICE_ID_SCR_MAX) ||
        (device_id >= PCI_DEVICE_ID_SCRPLUS && device_id <= PCI_DEVICE_ID_SCRPLUS_MAX) || device_id == NEW_PCI_DEVICE_ID) {
        return true;
    }
    return false;
}

/* Return true if PCI device is nervana device */
bool check_pci_device(const pci_address_t *address) {
    int path_size = 0;
    char path[PATH_MAX] = {0};
    char *buffer;
    uint32_t pci_id = 0;
    size_t buffer_size = PCI_ID_SIZE;

    assert(address);

#ifdef _WIN32
    path_size = sprintf_s(path, sizeof(path), SYSFS_PCI_DEVICE_PATH_FORMAT,
                          address->bus, address->device, address->function);
    if (path_size < 0)
        return false;

    if (_access(path, F_OK)) {
        XPUM_LOG_WARN("PCI device %02x:%02x.%1x does not exist\n",
                    address->bus, address->device, address->function);
        return false;
    }
#else
    path_size = sprintf(path, SYSFS_PCI_DEVICE_PATH_FORMAT,
                        address->bus, address->device, address->function);
    if (path_size < 0)
        return false;

    if (access(path, F_OK)) {
        XPUM_LOG_WARN("PCI device {}:{}.{} does not exist",
                      address->bus, address->device, address->function);
        return false;
    }
#endif

#ifdef _WIN32
    path_size = sprintf_s(path, sizeof(path), SYSFS_PCI_DEVICE_VALUE_FORMAT,
                          address->bus, address->device, address->function,
                          SYSFS_PCI_VENDOR);
#else
    path_size = sprintf(path, SYSFS_PCI_DEVICE_VALUE_FORMAT,
                        address->bus, address->device, address->function,
                        SYSFS_PCI_VENDOR);
#endif
    if (path_size < 0)
        return false;

    if (!compare_with_file(path, (uint8_t *)NRV_PCI_VENDOR, PCI_ID_SIZE)) {
        XPUM_LOG_WARN("PCI device {}:{}.{} has different vendor ID",
                      address->bus, address->device, address->function);
        return false;
    }

#ifdef _WIN32
    path_size = sprintf_s(path, sizeof(path), SYSFS_PCI_DEVICE_VALUE_FORMAT,
                          address->bus, address->device, address->function,
                          SYSFS_PCI_DEVICE);
    if (path_size < 0)
        return false;

    buffer = (char *)read_file(path, &buffer_size);
    if (sscanf_s(buffer, "0x%x", &pci_id) != 1) {
        XPUM_LOG_ERROR("ERROR: Failed to parse device ID\n");
        free(buffer);
        return false;
    }
#else
    path_size = sprintf(path, SYSFS_PCI_DEVICE_VALUE_FORMAT,
                        address->bus, address->device, address->function,
                        SYSFS_PCI_DEVICE);
    if (path_size < 0)
        return false;

    buffer = (char *)read_file(path, &buffer_size);
    if (buffer != nullptr && sscanf(buffer, "0x%x", &pci_id) != 1) {
        XPUM_LOG_ERROR("ERROR: Failed to parse device ID");
        free(buffer);
        return false;
    }
#endif

    free(buffer);

    if (is_pci_crest_device_id(pci_id)) {
        return true;
    } else {
        XPUM_LOG_WARN("PCI device {}:{}.{} has different device ID",
                      address->bus, address->device, address->function);
        return false;
    }
}

int reset_pci_device(const pci_address_t *address) {
    int path_size = 0;
    char path[PATH_MAX] = {0};

    assert(address);

#ifdef _WIN32
    path_size = sprintf_s(path, sizeof(path), SYSFS_PCI_DEVICE_VALUE_FORMAT,
                          address->bus, address->device, address->function,
                          SYSFS_PCI_RESET);
#else
    path_size = sprintf(path, SYSFS_PCI_DEVICE_VALUE_FORMAT,
                        address->bus, address->device, address->function,
                        SYSFS_PCI_RESET);
#endif
    if (path_size < 0)
        return NRV_PCI_ERROR;

    if (!write_file(path, (uint8_t *)"1", sizeof("1")))
        return NRV_PCI_ERROR;

    return NRV_SUCCESS;
}

static bool get_pci_properties_from_proc_line(char *line, pci_properties_t *properties) {
    char *line_tok;
    unsigned devfn;
    pci_properties_t prop;

    assert(properties);

    /* Get first section that contains bus and devfn */
    line_tok = strtok(line, "\t ");
    if (!line_tok)
        return false;

    if (sscanf(line_tok, "%02x%02x", (unsigned *)&prop.address.bus, &devfn) != 2)
        return false;

    prop.address.device = PCI_SLOT(devfn);
    prop.address.function = PCI_FUNC(devfn);

    /* Get second section that contains device and vendor id */
    line_tok = strtok(NULL, "\t ");
    if (!line_tok)
        return false;

    if (sscanf(line_tok, "%04x%04x", &prop.vendor_id, &prop.device_id) != 2)
        return false;

    /* Get fourth section that contains bar0 address */
    for (int i = 0; i < 2; i++)
        line_tok = strtok(NULL, "\t ");
    if (!line_tok)
        return false;

    if (sscanf(line_tok, "%08x", &prop.bar0) != 1)
        return false;

    *properties = prop;

    return true;
}

/* Return 0 on success */
int get_pci_device_list(pci_address_t *list, int list_size, int *out_count) {
    char line[LINE_LENGTH];
    FILE *pci_devices_fd;
    int count = 0;

    assert(list);
    assert(out_count);

    pci_devices_fd = fopen(PROC_PCI_DEVICES_FILE, "rb");
    if (!pci_devices_fd) {
        XPUM_LOG_ERROR("Unable to open {}. errno: {}({})",
                       PROC_PCI_DEVICES_FILE, errno, strerror(errno));
        return NRV_FIRMWARE_UPDATE_ERROR;
    }

    while (fgets(line, LINE_LENGTH, pci_devices_fd)) {
        pci_properties_t prop;

        if (!get_pci_properties_from_proc_line(line, &prop))
            continue;

        /* Check if vendor IP matches with Intel vendor ID */
        if (prop.vendor_id != NRV_PCI_VENDOR_ID)
            continue;

        /* Check if device ID matches with nervana device ID */
        if (!is_pci_crest_device_id(prop.device_id))
            continue;

        if (!check_pci_device(&prop.address))
            continue;

        if (count >= list_size) {
            break;
        }

        /* Add found pci address to list */
        list[count] = prop.address;
        count++;
    }

    fclose(pci_devices_fd);
    *out_count = count;

    return NRV_SUCCESS;
}

int get_pci_device_by_bar0_address(uint32_t bar0_address, pci_address_t *pci_address) {
    char line[LINE_LENGTH];
    FILE *pci_devices_fd;
    bool found = false;

    assert(pci_address);

    pci_devices_fd = fopen(PROC_PCI_DEVICES_FILE, "rb");
    if (!pci_devices_fd) {
        XPUM_LOG_ERROR("Unable to open {}. errno: {}({})",
                       PROC_PCI_DEVICES_FILE, errno, strerror(errno));
        return NRV_FIRMWARE_UPDATE_ERROR;
    }

    while (fgets(line, LINE_LENGTH, pci_devices_fd)) {
        pci_properties_t prop;

        if (!get_pci_properties_from_proc_line(line, &prop))
            continue;

        /* Check if BAR0 address matches with one received from BSMC */
        if (prop.bar0 != bar0_address)
            continue;

        /* Check if vendor IP matches with Intel vendor ID */
        if (prop.vendor_id != NRV_PCI_VENDOR_ID)
            continue;

        /* Check if device ID matches with nervana device ID */
        if (!is_pci_crest_device_id(prop.device_id))
            continue;

        if (!check_pci_device(&prop.address))
            continue;

        *pci_address = prop.address;
        found = true;
        break;
    }

    fclose(pci_devices_fd);

    if (found)
        return NRV_SUCCESS;

    return NRV_PCI_ERROR;
}
} // namespace xpum
