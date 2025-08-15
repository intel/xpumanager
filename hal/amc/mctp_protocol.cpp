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

#include "common.h"
#include "mctp.h"

/**
 * @brief Construct mctp command message header for SMBus/I2C transport
 *
 * Builds a complete mctp (Management Component Transport Protocol) message header
 * for communication over SMBus/I2C interface. Constructs both the medium-specific
 * header and transport header with proper addressing and packet control fields.
 *
 * @param i2cMctpHdr Pointer to mctp SMBus I2C header structure to populate
 * @param som Start of Message flag (1 = first packet, 0 = continuation)
 * @param eom End of Message flag (1 = last packet, 0 = more packets follow)
 * @param pktSeq Packet sequence number for multi-packet messages (0-3)
 * @param bytecount Number of bytes in the message payload
 * @param integrityCheck Integrity check flag for message validation
 *
 * @return uint8_t Status of header construction
 * @retval MCTP_SUCCESS Header constructed successfully
 * @retval MCTP_FAILURE Invalid parameters or construction failure
 *
 * @note Sets up SMBus addressing with AMC I2C slave address
 * @note Configures transport header with endpoint IDs and packet control
 * @note Uses destination EID from mDestEid member variable
 * @note Sets message type to MCTP_CONTROL per DSP0239 specification
 */
uint8_t mctp::commandConstruction(mctpSmbusI2cHdr *i2cMctpHdr, uint8_t som, uint8_t eom, uint8_t pktSeq,
								  uint8_t bytecount, uint8_t integrityCheck)
{
	TRACING();
	if (i2cMctpHdr == NULL) {
		return MCTP_FAILURE;
	}

	// mctp medium specific header (SMBus / I2C)
	i2cMctpHdr->destSlaveAddr = AMC_I2C_ADDR;
	i2cMctpHdr->destSlaveAddrB0 = MCTP_DESTSLAVE_ADDR_B0;
	i2cMctpHdr->cmdCode = MCTP_CMD_CODE;
	i2cMctpHdr->byteCount = bytecount;
	i2cMctpHdr->srcSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	i2cMctpHdr->srcSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;

	// mctp Transport header
	i2cMctpHdr->reserved = MCTP_RESERVED;
	i2cMctpHdr->hdrVersion = MCTP_HEADER_VERSION;

	i2cMctpHdr->destEpid = mDestEid; // MCTP_DESTINATION_ENDPOINT_ID;
	i2cMctpHdr->srcEpid = MCTP_SOURCE_ENDPOINT_ID;

	i2cMctpHdr->som = som & 0x1;
	i2cMctpHdr->eom = eom & 0x1;
	i2cMctpHdr->packSeq = pktSeq & 0x3; // Packet sequence for multi-packet messages
	i2cMctpHdr->to = MCTP_TAG_OWNER;
	i2cMctpHdr->msgTag = 0;

	i2cMctpHdr->ic = integrityCheck & 0x1; // Mask to 1 bit
	i2cMctpHdr->msgType = MCTP_CONTROL;	   // Spec DSP0239

	return MCTP_SUCCESS;
}

/**
 * @brief Construct mctp control message header
 *
 * Builds an mctp control message header with the specified instance ID and
 * command code. Sets appropriate flags and message type for control operations.
 *
 * @param mctpCtrl Pointer to mctp control header structure to populate
 * @param instanceID mctp instance ID for message correlation
 * @param cmd mctp control command code
 *
 * @return uint8_t Status of header construction
 * @retval MCTP_SUCCESS Header constructed successfully
 * @retval MCTP_FAILURE Invalid parameters or construction failure
 *
 * @note Validates input parameters before construction
 * @note Sets proper message type and control flags
 */
uint8_t mctp::headerConstruction(mctpControlHdr *mctpCtrl, uint8_t instanceId, uint8_t cmd)
{
	TRACING();
	if (mctpCtrl == NULL) {
		return MCTP_FAILURE;
	}

	mctpCtrl->request = MCTP_REQUEST;
	mctpCtrl->datagram = 0; // No Aysnc flow
	mctpCtrl->reserved = MCTP_RESERVED;
	mctpCtrl->instanceID = instanceId & 0x1F; // Mask to 5 bits

	mctpCtrl->cmdCode = cmd;

	return MCTP_SUCCESS;
}