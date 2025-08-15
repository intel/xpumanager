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

	if (instanceId > mPldmInstanceIDMax) {
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