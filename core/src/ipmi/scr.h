/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file scr.h
 */

#pragma once
#include "tool.h"

namespace xpum {

#define SCR_NUM_ICG_ICLS 8
#define SCR_NUM_NORTH_ICG_ICLS SCR_NUM_ICG_ICLS
#define SCR_NUM_SOUTH_ICG_ICLS SCR_NUM_ICG_ICLS
#define SCR_ICG_NORTH_NUMBER 0
#define SCR_ICG_SOUTH_NUMBER 1
#define SCR_TOTAL_NUM_ICLS (SCR_NUM_NORTH_ICG_ICLS + SCR_NUM_SOUTH_ICG_ICLS)
#define SCR_TOTAL_NUM_PGS 2
#define SCR_NHT_TABLE_ENTRIES 1024
#define SCR_ICC_ROUTE_SIZE (3 * sizeof(uint32_t) + 2 * sizeof(uint8_t))
#define SCR_ICLS_PER_PEER 4
#define SCR_NORTH_DISCOVERY_NODE_ID_OFFSET 64
#define SCR_SOUTH_DISCOVERY_NODE_ID_OFFSET 128

extern struct nrv_portgroup scr_pgs[SCR_TOTAL_NUM_PGS];

int scr_net_set_routes(ipmi_address_t *ipmi_address, int self, unsigned portgroup, int num_nodes,
                       uint64_t *port_masks, uint16_t *valid, enum nervana_routing_patterns mc,
                       enum nervana_virtual_channels vc);
int scr_net_get_neighbourhood(ipmi_address_t *ipmi_address, int *nodes);
int scr_get_board_portgroups(void **pgs, int *num_pgs);
unsigned long long scr_get_default_port_enable(uint8_t board_sku);
int scr_csmc_send_recv(ipmi_address_t *ipmi_address,
                       void *send_buf, uint16_t send_len,
                       void *recv_buf, uint16_t *recv_len);
int scr_csmc_send_msg(ipmi_address_t *ipmi_address, char *data_buf, uint16_t send_len);
void scr_clear_nht_table(void);
int net_get_nht_entries(uint32_t num_nodes, uint32_t idx, uint64_t port_masks, uint16_t valid,
                        enum nervana_routing_patterns mc, unsigned portgroup,
                        enum nervana_virtual_channels vc);
} // namespace xpum
