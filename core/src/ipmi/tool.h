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
 *  @file tool.h
 */

#pragma once
#include <assert.h>
#include <stdbool.h>
#include <vector>

#include "bsmc_interface.h"
#include "hal.h"
#ifndef XPUM_AMCFW_LIB_BUILD
#include "infrastructure/logger.h"
#endif
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

#ifdef XPUM_AMCFW_LIB_BUILD
#define XPUM_LOG_INFO(...)
#define XPUM_LOG_WARN(...)
#define XPUM_LOG_ERROR(...)
#define XPUM_LOG_DEBUG(...)
#define XPUM_LOG_TRACE(...)
#define XPUM_LOG_FATAL(...)
#endif