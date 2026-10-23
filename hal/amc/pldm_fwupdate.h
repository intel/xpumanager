/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __PLDM_FIRMWAREUPDATE_H
#define __PLDM_FIRMWAREUPDATE_H

#include <string>

#define PLDM_FWU_COMP_VER_STR_SIZE_MAX 256
#define PLDM_FWU_BASELINE_TRANSFER_SIZE 32 // DSP0267 section 7.8
#define PLDM_MCTP_MAX_TRANSFER_SIZE 4096
#define FWU_MAXIMUM_OUTSTANDING_TRANSFER_REQ 1
#define FWU_PACKAGE_DATALENGTH 0
#define FWU_FORCEUPDATE 1

#define COMPONENTCLASSIFICATIONINDEX 0x00

// DSP0267 Table 2 timing, in milliseconds
#define FWU_UA_T1_MS 1000	// Retry interval to send next cancel command: 500 ms to 5 s
#define FWU_UA_T2_MS 60000	// Request firmware data idle timeout: 60 s to 90 s
#define FWU_UA_T3_MS 180000 // State change timeout: ProgressPercent unchanged for at least 180 s
#define FWU_UA_T6_MS 600000 // Complete commands timeout when the FD reports no ProgressPercent: 600 s
// GetStatus ProgressPercent value of an FD that does not report progress (DSP0267 Table 40)
#define FWU_PROGRESS_NOT_SUPPORTED 0x65
// Interval between GetStatus polls while the FD verifies and applies; local pacing, not a protocol value
#define FWU_GET_STATUS_POLL_MS 5000
// Longest EstimatedTimeBeforeSendingRequestFirmwareData honoured before UA_T2 starts. A local bound, not
// from DSP0267: the FD reports up to 65535 s and a faulty one must not hold the update for hours.
#define FWU_MAX_EST_TIME_BEFORE_REQ_FW_DATA_SEC 600
// Attempts at transferring one component when UA_T2 expires: the first plus two resends after
// CancelUpdateComponent. DSP0267 section 12.13 allows resending, it does not limit the count.
#define FWU_COMPONENT_TRANSFER_ATTEMPTS 3

// DSP0267 Table 32 - ComponentClassification values for AMC firmware
#define COMP_CLASS_FIRMWARE 0x000A

// Outcome of the firmware device driven part of a component update (pldm::readFromFD())
enum class fwuTransferResult
{
	Applied,		 // ApplyComplete with a successful ApplyResult, acknowledged
	StageTimeout,	 // UA_T2 (transfer), or UA_T3 / UA_T6 (verify, apply) expired
	ComponentFailed, // TransferComplete / VerifyComplete / ApplyComplete reported a failure, acknowledged
	RestartRequired, // ApplyResult 0x0B: the FD asks for CancelUpdate and a new transfer of the image(s)
	Failed			 // any other failure, or the update was interrupted
};

enum _pldm_firmwareupdate_command_code
{
	QUERY_DEVICE_IDENTIFIERS = 0x01,
	GET_FIRMWARE_PARAMETERS = 0x02,
	QUERY_DOWNSTREAM_DEVICES = 0x03,
	QUERY_DOWNSTREAM_IDENTIFIERS = 0x04,
	GET_DOWNSTREAM_FIRMWARE_PARAMETERS = 0x05,
	REQUEST_UPDATE = 0x10,
	GET_PACKAGE_DATA = 0x11,
	GET_DEVICE_META_DATA = 0x12,
	PASS_COMPONENT_TABLE = 0x13,
	UPDATE_COMPONENT = 0x14,
	REQUEST_FIRMWARE_DATA = 0x15,
	TRANSFER_COMPLETE = 0x16,
	VERIFY_COMPLETE = 0x17,
	APPLY_COMPLETE = 0x18,
	GET_META_DATA = 0x19,
	ACTIVATE_FIRMWARE = 0x1A,
	GET_STATUS = 0x1B,
	CANCEL_UPDATE_COMPONENT = 0x1C,
	CANCEL_UPDATE = 0x1D,
	ACTIVATE_PENDING_COMPONENT_IMAGE_SET = 0x1E,
	ACTIVATE_PENDING_COMPONENT_IMAGE = 0x1F,
	REQUEST_DOWNSTREAM_DEVICE_UPDATE = 0x20,
	GET_COMPONENT_OPAQUE_DATA = 0x21,
	UPDATE_SECURITY_REVISION = 0x22
};

enum _pldm_fwupdate_error_codes
{
	NOT_IN_UPDATE_MODE = 0x80,
	ALREADY_IN_UPDATE_MODE = 0x81,
	DATA_OUT_OF_RANGE = 0x82,
	INVALID_TRANSFER_LENGTH = 0x83,
	INVALID_STATE_FOR_COMMAND = 0x84,
	INCOMPLETE_UPDATE = 0x85,
	BUSY_IN_BACKGROUND = 0x86,
	CANCEL_PENDING = 0x87,
	COMMAND_NOT_EXPECTED = 0x88,
	RETRY_REQUEST_FW_DATA = 0x89,
	UNABLE_TO_INITIATE_UPDATE = 0x8A,
	ACTIVATION_NOT_REQUIRED = 0x8B,
	SELF_CONTAINED_ACTIVATION_NOT_PERMITTED = 0x8C,
	NO_DEVICE_METADATA = 0x8D,
	RETRY_REQUEST_UPDATE = 0x8E,
	NO_PACKAGE_DATA = 0x8F,
	INVALID_DATA_TRANSFER_HANDLE = 0x90,
	INVALID_TRANSFER_OPERATION_FLAG = 0x91,
	ACTIVATE_PENDING_IMAGE_NOT_PERMITTED = 0x92,
	PACKAGE_DATA_ERROR = 0x93
};

enum GETSTATUS_STATES
{
	GETSTATUS_IDLE = 0x00,
	GETSTATUS_LEARN_COMPONENTS = 0x01,
	GETSTATUS_READY_XFER = 0x02,
	GETSTATUS_DOWNLOAD = 0x03,
	GETSTATUS_VERIFY = 0x04,
	GETSTATUS_APPLY = 0x05,
	GETSTATUS_ACTIVE = 0x06
};

enum PASSCOMPONENT_TABLE_TRANSFERFLAG
{
	TRANSFERFLAG_START = 0x01,
	TRANSFERFLAG_MIDDLE = 0x02,
	TRANSFERFLAG_END = 0x04,
	TRANSFERFLAG_STARTANDEND = 0x5
};

struct fwupd_cmds_t
{
	std::string name;
	_pldm_firmwareupdate_command_code cmd;
};

// Last, so pldm.h can use the declarations above when this header is included first
#include "pldm.h"

#endif // __PLDM_FIRMWAREUPDATE_H
