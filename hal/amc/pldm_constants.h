/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#ifndef __PLDM_CONSTANTS_H
#define __PLDM_CONSTANTS_H

#pragma once

// pldm Header Info
#define PLDM_RESPONSE 0
#define PLDM_REQUEST 1
#define PLDM_HEADER_VERSION 0 // Spec DSP0240
#define PLDM_HEADER_SIZE 3
#define PLDM_INSTANCE_ID_MAX 32
#define PLDM_TYPES_MAX 64
#define PLDM_RESERVED 0
#define PLDM_DISCOVERY_TID 0x03
#define PLDM_RESPONSE_COMPLETION 0x00

#define PLDM_DISCOVERY_MAX_PAYLOAD_SIZE 20
#define MAX_SMBUS_I2C_DATA_SIZE 512

// pldm Response Size
#define PLDM_VER_DATA_LENGTH 2
#define PLDM_GETPLDMTYPE_SIZE 9
#define PLDM_GetPLDMCOMMANDS_SIZE 3
#define PLDM_ASYNC_REQUEST_NOTIFY 0

// pldm Command Codes Length
#define PLDM_GETTID_SIZE 12
#define PLDM_GETTYPES_SIZE 12
#define PLDM_SETTID_SIZE 13
#define PLDM_GETVERSION_SIZE 18
#define PLDM_GETCOMMANDS_SIZE 17
#define PLDM_GETFRUTABLEMETADATA_SIZE 12
#define PLDM_GETFRUTABLE_SIZE 17
#define PLDM_MAX_RESPONSE_SIZE 128
#define PLDM_CMD_RESP_PAYLOAD_SIZE 1

#define PLDM_SOM_BIT_ON 0x01
#define PLDM_EOM_BIT_ON 0x01
#define PLDM_SOM_BIT_OFF 0x00
#define PLDM_EOM_BIT_OFF 0x00

enum pldmErrCodes
{
	PLDM_SUCCESS = 0x00,
	PLDM_ERROR = 0x01,
	PLDM_ERROR_INVALID_DATA = 0x02,
	PLDM_ERROR_INVALID_LENGTH = 0x03,
	PLDM_ERROR_NOT_READY = 0x04,
	PLDM_ERROR_UNSUPPORTED_PLDM_CMD = 0x05,
	PLDM_ERROR_INVALID_PLDM_TYPE = 0x20,
	PLDM_INVALID_TRANSFER_OPERATION_FLAG = 0x21
};

enum cmdType
{
	PLDM_MESSAGE_DISCOVERY = 0X00,
	PLDM_SMBIOS = 0X1,
	PLDM_PLATFORM_MONITORING = 0X2,
	PLDM_BIOS_CTRL = 0X3,
	PLDM_FRU = 0X4,
	PLDM_FIRMWARE_UPDATE = 0X5,
	PLDM_REDIFSH_DEVICE_ENABLEMENT = 0X6,
	PLDM_FILETRANSFER = 0X7,
	PLDM_OEM_SPECIFIC = 0x3F
};

enum pldmDiscCmdCode
{
	PLDM_SETTID = 0X01,
	PLDM_GETTID = 0X02,
	PLDM_GETVERSION = 0X03,
	PLDM_GETTYPES = 0X04,
	PLDM_GETCOMMANDS = 0X05,
	PLDM_SELECTVERSION = 0X06,
	PLDM_NEGOTIATETRANSFERPARAMETERS = 0X07,
	PLDM_MULTIPARTSEND = 0X08,
	PLDM_MULTIPARTRECEIVE = 0X09
};

#endif // __PLDM_CONSTANTS_H