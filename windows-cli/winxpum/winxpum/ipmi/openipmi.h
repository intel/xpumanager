/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file openipmi.h
 */

#ifndef IPMI_OPENIPMI_H
#define IPMI_OPENIPMI_H

#define IPMI_MAX_ADDR_SIZE		0x20
#define IPMI_BMC_CHANNEL		0xf
#define IPMI_NUM_CHANNELS		0x10

#define IPMI_SYSTEM_INTERFACE_ADDR_TYPE	0x0c
#define IPMI_IPMB_ADDR_TYPE		0x01
#define IPMI_IPMB_BROADCAST_ADDR_TYPE	0x41

#define IPMI_RESPONSE_RECV_TYPE		1
#define IPMI_ASYNC_EVENT_RECV_TYPE	2
#define IPMI_CMD_RECV_TYPE		3

struct ipmi_addr {
	int addr_type;
	short channel;
	char data[IPMI_MAX_ADDR_SIZE];
};

struct ipmi_msg {
	unsigned char netfn;
	unsigned char cmd;
	unsigned short data_len;
	unsigned char *data;
};

struct ipmi_req {
	unsigned char *addr;
	unsigned int addr_len;
	long msgid;
	struct ipmi_msg msg;
};

struct ipmi_recv {
	int recv_type;
	unsigned char *addr;
	unsigned int addr_len;
	long msgid;
	struct ipmi_msg msg;
};

struct ipmi_cmdspec {
	unsigned char netfn;
	unsigned char cmd;
};

struct ipmi_system_interface_addr {
	int addr_type;
	short channel;
	unsigned char lun;
};

struct ipmi_ipmb_addr {
	int addr_type;
	short channel;
	unsigned char slave_addr;
	unsigned char lun;
};

#define IPMI_IOC_MAGIC			'i'
#define IPMICTL_RECEIVE_MSG_TRUNC	_IOWR(IPMI_IOC_MAGIC, 11, struct ipmi_recv)
#define IPMICTL_RECEIVE_MSG		_IOWR(IPMI_IOC_MAGIC, 12, struct ipmi_recv)
#define IPMICTL_SEND_COMMAND		_IOR(IPMI_IOC_MAGIC, 13, struct ipmi_req)
#define IPMICTL_REGISTER_FOR_CMD	_IOR(IPMI_IOC_MAGIC, 14, struct ipmi_cmdspec)
#define IPMICTL_UNREGISTER_FOR_CMD	_IOR(IPMI_IOC_MAGIC, 15, struct ipmi_cmdspec)
#define IPMICTL_SET_GETS_EVENTS_CMD	_IOR(IPMI_IOC_MAGIC, 16, int)
#define IPMICTL_SET_MY_ADDRESS_CMD	_IOR(IPMI_IOC_MAGIC, 17, unsigned int)
#define IPMICTL_GET_MY_ADDRESS_CMD	_IOR(IPMI_IOC_MAGIC, 18, unsigned int)
#define IPMICTL_SET_MY_LUN_CMD		_IOR(IPMI_IOC_MAGIC, 19, unsigned int)
#define IPMICTL_GET_MY_LUN_CMD		_IOR(IPMI_IOC_MAGIC, 20, unsigned int)

/* The default slave address */
#define IPMI_BMC_SLAVE_ADDR 0x20

#endif /*IPMI_OPENIPMI_H*/
