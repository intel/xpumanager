/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *
 *  Intel Simplified Software License (Version August 2021)
 *
 *  Use and Redistribution.  You may use and redistribute the software (the “Software”), without modification, provided the following conditions are met:
 *
 *  * Redistributions must reproduce the above copyright notice and the
 *    following terms of use in the Software and in the documentation and/or other materials provided with the distribution.
 *  * Neither the name of Intel nor the names of its suppliers may be used to
 *    endorse or promote products derived from this Software without specific
 *    prior written permission.
 *  * No reverse engineering, decompilation, or disassembly of this Software
 *    is permitted.
 *
 *  No other licenses.  Except as provided in the preceding section, Intel grants no licenses or other rights by implication, estoppel or otherwise to, patent, copyright, trademark, trade name, service mark or other intellectual property licenses or rights of Intel.
 *
 *  Third party software.  The Software may contain Third Party Software. “Third Party Software” is open source software, third party software, or other Intel software that may be identified in the Software itself or in the files (if any) listed in the “third-party-software.txt” or similarly named text file included with the Software. Third Party Software, even if included with the distribution of the Software, may be governed by separate license terms, including without limitation, open source software license terms, third party software license terms, and other Intel software license terms. Those separate license terms solely govern your use of the Third Party Software, and nothing in this license limits any rights under, or grants rights that supersede, the terms of the applicable license terms.
 *
 *  DISCLAIMER.  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT ARE DISCLAIMED. THIS SOFTWARE IS NOT INTENDED FOR USE IN SYSTEMS OR APPLICATIONS WHERE FAILURE OF THE SOFTWARE MAY CAUSE PERSONAL INJURY OR DEATH AND YOU AGREE THAT YOU ARE FULLY RESPONSIBLE FOR ANY CLAIMS, COSTS, DAMAGES, EXPENSES, AND ATTORNEYS’ FEES ARISING OUT OF ANY SUCH USE, EVEN IF ANY CLAIM ALLEGES THAT INTEL WAS NEGLIGENT REGARDING THE DESIGN OR MANUFACTURE OF THE SOFTWARE.
 *
 *  LIMITATION OF LIABILITY. IN NO EVENT WILL INTEL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  No support.  Intel may make changes to the Software, at any time without notice, and is not obligated to support, update or provide training for the Software.
 *
 *  Termination. Your right to use the Software is terminated in the event of your breach of this license.
 *
 *  Feedback.  Should you provide Intel with comments, modifications, corrections, enhancements or other input (“Feedback”) related to the Software, Intel will be free to use, disclose, reproduce, license or otherwise distribute or exploit the Feedback in its sole discretion without any obligations or restrictions of any kind, including without limitation, intellectual property rights or licensing obligations.
 *
 *  Compliance with laws.  You agree to comply with all relevant laws and regulations governing your use, transfer, import or export (or prohibition thereof) of the Software.
 *
 *  Governing law.  All disputes will be governed by the laws of the United States of America and the State of Delaware without reference to conflict of law principles and subject to the exclusive jurisdiction of the state or federal courts sitting in the State of Delaware, and each party agrees that it submits to the personal jurisdiction and venue of those courts and waives any objections. The United Nations Convention on Contracts for the International Sale of Goods (1980) is specifically excluded and will not apply to the Software.
 *
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