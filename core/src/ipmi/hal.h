/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file hal.h
 */

#pragma once
#include "bsmc_interface.h"
#include "bsmc_ipmi_oem_cmd.h"

namespace xpum {
/* Following mulitcast patterns are supported for SCR */
enum nervana_routing_patterns {
    NERVANA_MULTICAST_TO_IMMEDIATE_NEIGHBORS,
    NERVANA_BROADCAST_TO_TOPOLOGY,
    NERVANA_BROADCAST_TO_CHASSIS,
    NERVANA_UNICAST_ROUTE,
    NERVANA_MCAST_NUM_PATTERNS
};

enum nervana_virtual_channels {
    NERVANA_VC_0,
    NERVANA_VC_1,
    NERVANA_VC_NUM,
};

typedef struct {
    int (*csmc_send_recv)(ipmi_address_t *ipmi_address,
                          void *send_buf, uint16_t send_len,
                          void *recv_buf, uint16_t *recv_len);
    int (*csmc_send_msg)(ipmi_address_t *ipmi_address, char *data_buf, uint16_t send_len);
    int (*net_set_routes)(ipmi_address_t *ipmi_address, int self, unsigned portgroup, int num_nodes,
                          uint64_t *port_masks, uint16_t *valid, enum nervana_routing_patterns mc,
                          enum nervana_virtual_channels vc);
    int (*net_get_neighbourhood)(ipmi_address_t *ipmi_address, int *nodes);
    int (*get_board_portgroups)(void **pgs, int *num_pgs);
    unsigned long long (*get_default_port_enable)(uint8_t board_sku);
} nnp_hal;
} // namespace xpum
