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
