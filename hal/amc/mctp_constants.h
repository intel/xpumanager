/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __MCTP_CONSTANTS_H
#define __MCTP_CONSTANTS_H

#define MCTP_SUCCESS 0x00
#define MCTP_FAILURE 0x01
#define MCTP_CANCELLED 0x02 // local status: a receive was abandoned at the caller's request
#define MCTP_TIMEOUT 0x03	// local status: no message began to arrive within the time given
#define MCTP_HEADER_VERSION 0x01
#define MCTP_HEADER_SIZE 0x09
#define MCTP_HEADER_PEC_SIZE 0x01
#define MCTP_DESTSLAVE_ADDR_B0 0x00
#define MCTP_SRC_SLAVE_ADDR 0x30 // UA will configure this address
#define MCTP_SRC_SLAVE_ADDR_B0 0x01
#define MCTP_CMD_CODE 0x0F
#define MCTP_RESERVED 0x00
#define MCTP_INTEGRITY_CHECK 0x00
#define MCTP_TAG_OWNER 0x01
#define MCTP_DESTINATION_ENDPOINT_ID 0x00 // Initial value, will be updated after configuring SetEID in mctp control
#define MCTP_SOURCE_ENDPOINT_ID 0x07	  // UA will configure this
#define MCTP_DEST_SET_EID 0x09
#define MCTP_REQUEST 0x01
#define MCTP_RESPONSE 0x00

#define MCTP_MAX_PAYLOAD_SIZE 20
#define MCTP_MAX_RESPONSE_SIZE 32
#define m_mctp_instance_id_MAX 32
// mctp CONTROL CMD SIZE in bytes
#define MCTP_SET_EID_CMD_SIZE 13
#define MCTP_GET_EID_CMD_SIZE 11
#define MCTP_GET_EID_UUID_CMD_SIZE 11
#define MCTP_GET_VERSION_SIZE 12
#define MCTP_GET_MSG_TYPE_SIZE 11
#define MCTP_SOM 1
#define MCTP_EOM 1
#define MCTP_PAK_SEQ 0
#define MCTL_PLDM_RESPONSE_PAYLOAD_SIZE 128

// DSP0236 transport header: header version, destination EID, source EID, SOM/EOM/Pkt Seq#/TO/Msg Tag
#define MCTP_TRANSPORT_HDR_SIZE 4
// DSP0236 baseline transmission unit: the packet payload size every MCTP endpoint must support.
// Nothing in this stack negotiates a larger one, so every packet is sized against it.
#define MCTP_BASELINE_TRANSMISSION_UNIT 64
// DSP0236 null and broadcast EIDs
#define MCTP_NULL_EID 0x00
#define MCTP_BROADCAST_EID 0xFF
// DSP0237 frame: destination slave address, command code, byte count, ..., PEC.
// The byte count covers the source slave address through the last payload byte.
#define MCTP_SMBUS_SRC_ADDR_SIZE 1
#define MCTP_SMBUS_FRAME_OVERHEAD 4 // destination slave address + command code + byte count + PEC
#define MCTP_SMBUS_MAX_BYTE_COUNT (MCTP_SMBUS_SRC_ADDR_SIZE + MCTP_TRANSPORT_HDR_SIZE + MCTP_BASELINE_TRANSMISSION_UNIT)
// Number of bytes requested per I2C read; one AMC frame plus the padding the AMC returns around it
#define MCTP_SMBUS_READ_SIZE 128
// Upper bound on a reassembled message; a local resource limit, not a protocol value
#define MCTP_MAX_ASSEMBLED_MSG_SIZE 8192
// Poll interval for the next packet of a message already being received; local pacing, not a protocol value
#define MCTP_ASSEMBLY_POLL_MS 10
// Longest gap between two packets of one message before its reassembly is abandoned; a local bound
#define MCTP_ASSEMBLY_GAP_TIMEOUT_MS 1000
// Gap between the packets of one message. The AMC's I2C target holds a single received
// frame and drops it if the next write starts before its I2C thread has drained it.
#define MCTP_INTER_PACKET_DELAY_MS 2

enum mctpControlCode
{
	MCTP_SET_ENDPOINT_ID = 0x01,
	MCTP_GET_ENDPOINT_ID = 0x02,
	MCTP_GET_ENDPOINT_UUID = 0x03,
	MCTP_GET_VERSION = 0x04,
	MCTP_GET_MESSAGE_TYPE = 0x05,
	MCTP_GET_VENDOR_DEFINED_MESSAGE_SUPPORT = 0x06
};

enum mctpMessageType
{
	MCTP_CONTROL = 0X00,
	PLDM_OVER_MCTP = 0X01,
	NCSI_OVER_MCTP = 0X02,
	ETHERNET_OVER_MCTP = 0X03,
	NVMEXPRESS_OVER_MCTP = 0X04,
	VENDORDEFINED_PCI = 0X7E,
	VENDORDEFINED_IANA = 0X7F
};

enum mctpCompletionCodes
{
	MCTP_COMPLETION_SUCCESS = 0x00,
	MCTP_COMPLETION_ERROR = 0x01,
	MCTP_INVALID_DATA = 0x02,
	MCTP_INVALID_LENGTH = 0x03,
	MCTP_ERROR_NOT_READY = 0x04,
	MCTP_UNSUPPORTED_CMD = 0x05
};

#endif // __MCTP_CONSTANTS_H