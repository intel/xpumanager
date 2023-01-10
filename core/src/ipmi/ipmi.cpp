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
 *  @file ipmi.cpp
 */

#include "ipmi.h"

#include <cassert>
#include <cstdio>
#include <cstring>

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
