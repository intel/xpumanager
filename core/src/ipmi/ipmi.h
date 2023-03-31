/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file ipmi.h
 */

#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "xpum_structs.h"

namespace xpum {

// return code
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
#define NRV_IPMI_ERROR_FW_UPDATE_FAIL 15
#define NRV_IPMI_ERROR_FW_UPDATE_SIGNATURE_FAIL 16
#define NRV_IPMI_ERROR_FW_UPDATE_IMAGE_TO_LARGE_FAIL 17
#define NRV_IPMI_ERROR_FW_UPDATE_NO_IMAGE_SIZE_FAIL 18
#define NRV_IPMI_ERROR_FW_UPDATE_PACKET_TO_LARGE_FAIL 19
#define NRV_IPMI_ERROR_FW_UPDATE_TO_MANY_RETRIES_FAIL 20
#define NRV_IPMI_ERROR_FW_UPDATE_WRITE_TO_FLASH_FAIL 21
#define NRV_COMMAND_NOT_EXIST 127

typedef void (*percent_callback_func_t)(uint32_t percent, void *pAmcManager);

int cmd_firmware(const char *file, unsigned int versions[4]);

int cmd_get_amc_firmware_versions(int buf[][4], int *count);

void setPercentCallbackAndContext(percent_callback_func_t callback, void *pAmcManager);

std::vector<xpum_sensor_reading_t> read_sensor();

int get_sn_number(uint8_t baseboardSlot, uint8_t riserSlot, std::string &sn_number);

std::string getIpmiErrorString(int errorCode);
} // namespace xpum