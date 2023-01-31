/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file lcr.h
 */

#pragma once
#include "tool.h"

namespace xpum {

#define LCR_NUM_ICG_ICLS 8
#define LCR_NUM_NORTH_ICG_ICLS LCR_NUM_ICG_ICLS
#define LCR_NUM_SOUTH_ICG_ICLS (LCR_NUM_ICG_ICLS / 2)
#define LCR_ICG_NORTH_NUMBER 0
#define LCR_ICG_SOUTH_NUMBER 1
#define LCR_TOTAL_NUM_ICLS (LCR_NUM_NORTH_ICG_ICLS + LCR_NUM_SOUTH_ICG_ICLS)
#define LCR_TOTAL_NUM_PGS 2
#define LCR_ICC_ROUTE_SIZE (3 * sizeof(uint32_t))

extern struct nrv_portgroup lcr_pgs[LCR_TOTAL_NUM_PGS];

int lcr_net_set_routes(ipmi_address_t *ipmi_address, int self, unsigned portgroup, int num_nodes,
                       uint64_t *port_masks, uint16_t *valid, enum nervana_routing_patterns mc,
                       enum nervana_virtual_channels vc);
int lcr_net_get_neighbourhood(ipmi_address_t *ipmi_address, int *nodes);
int lcr_get_board_portgroups(void **pgs, int *num_pgs);
unsigned long long lcr_get_default_port_enable(uint8_t board_sku);
int lcr_csmc_send_recv(ipmi_address_t *ipmi_address,
                       void *send_buf, uint16_t send_len,
                       void *recv_buf, uint16_t *recv_len);
int lcr_csmc_send_msg(ipmi_address_t *ipmi_address, char *data_buf, uint16_t send_len);
} // namespace xpum
