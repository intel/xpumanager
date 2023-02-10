/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file fru.cpp
 */


#include "bsmc_interface.h"
#include "tool.h"
#include "fru.h"
#include <bitset>
#include <iostream>

namespace xpum {

extern unsigned char gNetfn;
extern unsigned char gCmd;
extern uint8_t gDeviceId;
extern uint8_t gOffsetLsb;
extern uint8_t gOffsetMsb;
extern uint8_t gReadCount;

int get_fru_data_size(ipmi_address_t *ipmi_address) {
    bsmc_req req;
    bsmc_res res;
    req.ipmi_address = *ipmi_address;
    req.netfn = IPMI_STORAGE_NETFN;
    req.cmd = IPMI_FRU_GET_INFO;
    req.fru_area_info.device_id = 0;
    req.data_len = sizeof(fru_get_area_info_req_t);
    gNetfn = IPMI_STORAGE_NETFN;
    gCmd = IPMI_FRU_GET_INFO;

    if (bsmc_hal->cmd(&req, &res)) {
        return -1;
    }
    if (bsmc_hal->validate_res(res, sizeof(fru_read_data_resp_t))) {
        return -1;
    }
    return (res.fru_area_info.fru_size_msb << 8) + res.fru_area_info.fru_size_lsb;
}

int get_fru_data(ipmi_address_t *ipmi_address, uint16_t fru_size, uint8_t *out_data) {
	bsmc_req req;
	bsmc_res res;

	req.ipmi_address = *ipmi_address;
	req.netfn = IPMI_STORAGE_NETFN;
	req.cmd = IPMI_FRU_READ_DATA;
	req.data_len = sizeof(fru_read_data_req_t);

	req.fru_read.device_id = 0;
	req.fru_read.read_count = 0x1e;

	gNetfn = IPMI_STORAGE_NETFN;
	gCmd = IPMI_FRU_READ_DATA;

	/* Read FRU data from 0 offset */
	for (uint16_t offset = 0; offset < fru_size; offset += res.fru_read.bytes_read) {
		req.fru_read.offset_lsb = 0xff & offset;
		req.fru_read.offset_msb = offset >> 8;
		/* Prevent size exceeded */
		if (offset + req.fru_read.read_count > fru_size)
			req.fru_read.read_count = fru_size - offset;

        gDeviceId = 0;
		gOffsetLsb = req.fru_read.offset_lsb;
		gOffsetMsb = req.fru_read.offset_msb;
		gReadCount = req.fru_read.read_count;

		if (bsmc_hal->cmd(&req, &res))
			return NRV_IPMI_ERROR;

		if (bsmc_hal->validate_res(res, sizeof(fru_read_data_resp_t)))
		        return NRV_IPMI_ERROR;

		if (res.fru_read.bytes_read != req.fru_read.read_count) {
			return NRV_IPMI_ERROR;
		}

		memcpy((out_data + offset), res.fru_read.read_data, res.fru_read.bytes_read);
	}
	return NRV_SUCCESS;
}

int parse_sn_from_fru_data(std::vector<uint8_t> const &fru_data, std::string& sn_number) {
    if (fru_data[0] != 0x01) {
        return NRV_INVALID_FRU;
    }
    const uint8_t board_area_offset = fru_data[3] * 8;
    const uint8_t fixed_board_area_begining = 6;
    uint8_t current_offset = board_area_offset + fixed_board_area_begining;
    // Skip 2 field, then reached board serial number field
    for (int i = 0; i < 3; i++) {
        uint8_t type_length_byte = fru_data[current_offset];
        if (!(type_length_byte & 0xc0)) {
            return NRV_INVALID_FRU;
        }
        uint8_t field_size = type_length_byte & 0x3f;
        if (i == 2) {
            sn_number = std::string((const char*)fru_data.data() + current_offset + 1);
        }
        current_offset += field_size + 1;
    }

    return NRV_SUCCESS;    
}

int get_sn_number(uint8_t baseboardSlot, uint8_t riserSlot, std::string &sn_number) {
    int res_list_card = NRV_SUCCESS;
    int card_id = CARD_SELECT_ALL;
    nrv_list cards;
    res_list_card = get_card_list(&cards, card_id);
    if (res_list_card) {
        return res_list_card;
    }

    std::vector<uint8_t> fru_data;

    uint8_t compactSlotNumber = (baseboardSlot & 0x07) + ((riserSlot & 0x07) << 3);

    nrv_card &target = cards.card[0];

    for (int i = 0; i < cards.count; i++) {
        if (cards.card[i].ipmi_address.slot == compactSlotNumber) {
            target = cards.card[i];
        }
    }
    if (target.ipmi_address.slot != compactSlotNumber) {
        // Invalid baseboard slot number or riser slot number
        return NRV_INVALID_FRU;
    }

    int fru_data_size = get_fru_data_size(&target.ipmi_address);
    if (fru_data_size < 0) {
        return NRV_INVALID_FRU;
    }
    fru_data.resize(fru_data_size);
    if (int err = get_fru_data(&target.ipmi_address, fru_data_size, fru_data.data())) {
        return err;
    }
    return parse_sn_from_fru_data(fru_data, sn_number);
}

} // namespace xpum