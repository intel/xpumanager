/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __PLDM_FILE_TRANSFER_H
#define __PLDM_FILE_TRANSFER_H

#include "pldm_types.h"

#include <cstdint>

#define PLDM_FT_CMD_BASE_SIZE 12
#define PLDM_FT_DF_OPEN_SIZE 17
#define PLDM_FT_DF_CLOSE_SIZE 17
#define PLDM_FT_DF_HEARTBEAT_SIZE 19
#define PLDM_FT_DF_PROPERTIES_SIZE 17
#define PLDM_FT_DF_GET_FILE_ATTRIBUTE_SIZE 20
#define PLDM_FT_DF_SET_FILE_ATTRIBUTE_SIZE 21
#define PLDM_FT_DF_READ_SIZE 23
#define PLDM_FT_DF_FIFO_SEND_SIZE 23

#define MAX_NUM_RETRIES 3

enum pldm_ft_command_code
{
	PLDM_FT_CMDCODE_DF_OPEN = 0x01,
	PLDM_FT_CMDCODE_DF_CLOSE = 0x02,
	PLDM_FT_CMDCODE_DF_HEARTBEAT = 0x03,
	PLDM_FT_CMDCODE_DF_PROPERTIES = 0x10,
	PLDM_FT_CMDCODE_DF_GET_FILE_ATTRIBUTE = 0x11,
	PLDM_FT_CMDCODE_DF_SET_FILE_ATTRIBUTE = 0x12,
	PLDM_FT_CMDCODE_DF_READ = 0x20,
	PLDM_FT_CMDCODE_DF_FIFO_SEND = 0x21,
};

enum pldm_ft_completion_codes
{
	PLDM_FT_COMPLCODE_INVALID_FILE_DESCRIPTOR = 0x80,
	PLDM_FT_COMPLCODE_INVALID_DF_ATTRIBUTE = 0x81,
	PLDM_FT_COMPLCODE_ZEROLENGTH_NOT_ALLOWED = 0x82,
	PLDM_FT_COMPLCODE_EXCLUSIVE_OWNERSHIP_NOT_ESTABLISHED = 0x83,
	PLDM_FT_COMPLCODE_EXCLUSIVE_OWNERSHIP_NOT_ALLOWED = 0x84,
	PLDM_FT_COMPLCODE_EXCLUSIVE_OWNERSHIP_NOT_AVAILABLE = 0x85,
	PLDM_FT_COMPLCODE_INVALID_FILE_IDENTIFIER = 0x86,
	PLDM_FT_COMPLCODE_DFOPEN_DIR_NOT_ALLOWED = 0x87,
	PLDM_FT_COMPLCODE_MAX_NUM_FDS_EXCEEDED = 0x88,
	PLDM_FT_COMPLCODE_FILE_OPEN = 0x89,
	PLDM_FT_COMPLCODE_UNABLE_TO_OPEN_FILE = 0x8A,
};

enum pldm_ft_status
{
	PLDM_FT_SUCCESS = 0x00,
	PLDM_FT_ERROR = 0x01
};

enum pldm_file_table_type
{
	PLDM_FILE_TABLE_OEM = 0x00
};

#pragma pack(push, 1)

struct pldm_file_df_open_req
{
	uint16_t fileIdentifier;
	bitfield16_t fileAttribute;
};

struct pldm_file_df_open_resp
{
	uint8_t completionCode;
	uint16_t fileDescriptor;
};

struct pldm_file_df_close_req
{
	uint16_t fileDescriptor;
	bitfield16_t dfCloseOptions;
};

struct pldm_file_df_close_resp
{
	uint8_t completionCode;
};

struct pldm_file_df_heartbeat_req
{
	uint16_t fileDescriptor;
	uint32_t requesterMaxInterval;
};

struct pldm_file_df_heartbeat_resp
{
	uint8_t completionCode;
	uint32_t responderMaxInterval;
};

struct pldm_file_df_read_req
{
	uint16_t fileDescriptor;
	uint32_t offset;
	uint32_t length;
};

struct pldm_file_df_read_resp
{
	uint8_t completionCode;
	uint32_t dataLength;
	uint8_t data[512];
};

struct pldm_base_multipart_receive_req
{
	uint8_t pldmType;
	uint8_t transferOperation;
	uint32_t transferContext;
	uint32_t dataTransferHandle;
	uint32_t requestedSectionOffset;
	uint32_t requestedSectionLengthBytes;
};

struct pldm_base_multipart_receive_resp
{
	uint8_t completionCode;
	uint8_t transferFlag;
	uint32_t nextDataTransferHandle;
	uint32_t dataLengthBytes;
	uint8_t data[1];
};

struct pldm_get_file_table_req
{
	uint32_t dataTransferHandle;
	uint8_t transferOperationFlag;
	uint8_t tableType;
};

struct pldm_get_file_table_resp
{
	uint8_t completionCode;
	uint32_t nextDataTransferHandle;
	uint8_t transferFlag;
	uint8_t tableData[1];
};

// ReadFile
struct pldm_read_file_req
{
	uint32_t fileHandle;
	uint32_t offset;
	uint32_t length;
};

struct pldm_read_file_resp
{
	uint8_t completionCode;
	uint32_t length;
	uint8_t fileData[1];
};

// WriteFile
struct pldm_write_file_req
{
	uint32_t fileHandle;
	uint32_t offset;
	uint32_t length;
	uint8_t fileData[1];
};

struct pldm_write_file_resp
{
	uint8_t completionCode;
	uint32_t length;
};

// ReadFileByType
struct pldm_read_file_by_type_req
{
	uint16_t fileType;
	uint32_t fileHandle;
	uint32_t offset;
	uint32_t length;
};

struct pldm_read_file_by_type_resp
{
	uint8_t completionCode;
	uint32_t length;
	uint8_t fileData[1];
};

// WriteFileByType
struct pldm_write_file_by_type_req
{
	uint16_t fileType;
	uint32_t fileHandle;
	uint32_t offset;
	uint32_t length;
	uint8_t fileData[1];
};

struct pldm_write_file_by_type_resp
{
	uint8_t completionCode;
	uint32_t length;
};

// FileAcked
struct pldm_file_acked_req
{
	uint16_t fileType;
	uint32_t fileHandle;
	uint8_t fileStatus;
};

struct pldm_file_acked_resp
{
	uint8_t completionCode;
};

// NegotiateTransferParameters
struct pldm_negotiate_transfer_parameters_req
{
	uint16_t partSize;
	uint8_t protocolSupport[8];
};

struct pldm_negotiate_transfer_parameters_resp
{
	uint8_t completionCode;
	uint16_t partSize;
	uint8_t protocolSupport[8];
};

// TransferComplete
struct pldm_transfer_complete_req
{
	uint8_t transferResult;
};

struct pldm_transfer_complete_resp
{
	uint8_t completionCode;
};

#pragma pack(pop)

#endif
