/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file psc.h
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace xpum {

#define PSCBIN_MAGIC_NUMBER 0x42435350

#define MAX_SOCKET_IDS (32)

#define MAX_INIS ((MAX_SOCKET_IDS)*2)

#define PSCBIN_VERSION_NULL 0
#define PSCBIN_VERSION_MIN 2
#define PSCBIN_VERSION_MAX 3

struct psc_identifier {
    uint32_t magic;
    uint32_t psc_format_version;
};

struct psc_item {
    uint32_t idx;
    uint32_t size;
};

struct psc_data {
    struct psc_identifier identifier;
    uint32_t form_factor;
    uint32_t cfg_version;
    uint32_t date;
    uint32_t time;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t data_size;
    struct psc_item brand_name;
    struct psc_item product_name;
    struct psc_item comment;
    struct psc_item ini_name[MAX_INIS];
    struct psc_item ini_bin[MAX_INIS];
    struct psc_item ext_data;
    struct psc_item cust_data;
    struct psc_item presence_data;
    uint32_t reserved3;
    uint32_t crc32c_hdr;
    uint8_t data[];
};

inline std::string getPscVersion(uint32_t cfg_version, uint32_t time) {
    char buffer[100];
    int cx;

    cx = snprintf(buffer, 100, "0x%04x.0x%04x", cfg_version, time);
    if (cx >= 0 && cx < 100) {
        return std::string(buffer);
    }
    return std::string();
}

#define TXCAL_PORT_COUNT (8)

#define TXCAL_BLOB_MAGIC_0 (0x54206558)
#define TXCAL_BLOB_MAGIC_1 (0x61432078)
#define TXCAL_BLOB_MAGIC_2 (0x6c42206c)
#define TXCAL_BLOB_MAGIC_3 (0x0000626f)

#define TXCAL_VERSION_CURRENT 1

struct txcal_settings {
    uint64_t guid;
    uint16_t port_settings[TXCAL_PORT_COUNT];
};

struct txcal_blob {
    uint32_t magic[4];
    uint32_t format_version;
    uint32_t cfg_version;
    uint32_t date;
    uint32_t time;
    uint32_t size;
    uint32_t num_settings;
    uint32_t crc32c_data;
    uint32_t crc32c_hdr;
    struct txcal_settings data[];
};

} // namespace xpum