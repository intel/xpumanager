/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm.h"

/**
 * @brief Construct pldm message header
 *
 * Builds a pldm (Platform Level Data Model) message header with the specified
 * parameters. Validates input parameters and sets appropriate header fields.
 *
 * @param pldmHdr Pointer to pldm header structure to populate
 * @param instanceID pldm instance ID (must be <= max instance ID)
 * @param cmdType pldm command type (must be < PLDM_TYPES_MAX)
 * @param cmd pldm command code
 * @param async Asynchronous notification flag
 * @param reqresp Request/response indicator (PLDM_REQUEST or PLDM_RESPONSE)
 *
 * @return uint8_t Status of header construction
 * @retval PLDM_SUCCESS Header constructed successfully
 * @retval PLDM_ERROR_INVALID_DATA Invalid input parameters
 * @retval PLDM_ERROR_INVALID_PLDM_TYPE Invalid pldm command type
 */
uint8_t pldm::pldmHdrConstruction(struct pldmHdr *pldmHdr, uint8_t instanceId, uint8_t cmdType, uint8_t cmd,
								  uint8_t async, uint8_t reqresp)
{
	TRACING();
	if (pldmHdr == NULL) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (instanceId > PLDM_INSTANCE_ID_MAX) {
		return PLDM_ERROR_INVALID_DATA;
	}

	if (cmdType > (PLDM_TYPES_MAX - 1)) {
		return PLDM_ERROR_INVALID_PLDM_TYPE;
	}

	// Byte 01
	pldmHdr->request = reqresp & 0x1; // Mask to 1 bit
	pldmHdr->datagram = ((async == PLDM_ASYNC_REQUEST_NOTIFY) ? 0 : 1);
	pldmHdr->reserved = PLDM_RESERVED;
	pldmHdr->instanceID = instanceId & 0x1F; // Mask to 5 bits

	// Byte 02
	pldmHdr->headerVer = PLDM_HEADER_VERSION;
	pldmHdr->cmdType = cmdType & 0x3F; // Mask to 6 bits

	// Byte 03
	pldmHdr->cmdCode = cmd;

	return PLDM_SUCCESS;
}

/**
 * @brief Check that the received PLDM header answers the request that was just sent
 *
 * The AMC answers whatever request it saw last, so a reply left over from an earlier request
 * (from this process or another one) can be read in place of the expected one. Its
 * completion code alone cannot tell the two apart.
 *
 * @param[in] cmdType PLDM type of the request
 * @param[in] cmd     PLDM command code of the request
 * @param[in] id      PLDM instance ID of the request
 *
 * @retval true  The header in mI2cPldmRead is a response to that request
 * @retval false The header belongs to some other message
 */
bool pldm::isResponseTo(uint8_t cmdType, uint8_t cmd, uint8_t id) const
{
	// An MCTP control reply has the same instance ID byte, so the message type is checked too.
	const unsigned msgType = mI2cPldmRead->mctpSmbusHdr.msgType;
	const struct pldmHdr &hdr = mI2cPldmRead->pldmHdr;
	if (msgType == PLDM_OVER_MCTP && hdr.request == PLDM_RESPONSE && hdr.cmdType == cmdType && hdr.cmdCode == cmd &&
		hdr.instanceID == id) {
		return true;
	}
	ERR("PLDM : Response does not match request type 0x{:02x} cmd 0x{:02x} instance {} (got message type 0x{:02x} "
		"type 0x{:02x} cmd 0x{:02x} instance {} request {})\n",
		cmdType, cmd, id, msgType, static_cast<unsigned>(hdr.cmdType), hdr.cmdCode,
		static_cast<unsigned>(hdr.instanceID), static_cast<unsigned>(hdr.request));
	return false;
}
