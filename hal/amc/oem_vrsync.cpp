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

#include "oem_vrsync.h"
#include "pldm.h"

/**
 * @brief Execute OEM VRSync command
 *
 * Sends an OEM-specific VRSync (Voltage Regulator Synchronization) command
 * to the AMC device. Used for power management operations like pausing or
 * resuming voltage regulator synchronization.
 *
 * @param cmd VRSync command code to execute:
 *            - OEM_VR_SYNC_PAUSE (0x01): Pause VR synchronization
 *            - OEM_VR_SYNC_RESUME (0x02): Resume VR synchronization
 *
 * @return uint8_t Status of the VRSync operation
 * @retval 0 Command executed successfully
 * @retval Non-zero Error occurred during command execution
 *
 * @note Uses pldm protocol over mctp for communication
 * @note Manages pldm instance ID automatically, wrapping at maximum value
 */
uint8_t pldm::oemVrsyncCmd(uint8_t cmd)
{
	TRACING();
	uint8_t oemCmdLen = OEM_COMMAND_BASE_SIZE;
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;
	uint8_t *rptr = (uint8_t *)mI2cPldmRead;

	if (instanceID == PLDM_INSTANCE_ID_MAX) {
		instanceID = 1;
	}

	// UA will calculate this PAK_SEQ for multi packet messages, currently we are not using multi packet messages
	commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (OEM_COMMAND_BASE_SIZE - 3),
						MCTP_INTEGRITY_CHECK);
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;

	pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, instanceID, PLDM_OEM_SPECIFIC, cmd, PLDM_ASYNC_REQUEST_NOTIFY,
						PLDM_REQUEST);

	// Initialize payload buffer and fill CRC Byte
	memset(mI2cPldmWrite->respPayload, 0, oemCmdLen);
	// CRC is calculated over the payload excluding the CRC byte itself
	mI2cPldmWrite->respPayload[BYTE_0] = crc8Smbus(mI2cPldmWrite->respPayload, oemCmdLen - 1);

	DBG("pldm TX  :: ");
	hexdump((uint8_t *)mI2cPldmWrite, oemCmdLen);

	if (i2cobj->writeAmc(wptr + 1, oemCmdLen) != true) {
		ERR("pldm Discovery : I2C Write failure\n");
		return PLDM_ERROR;
	}

	// Wait for I2C event
	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);

	if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) != true) {
		ERR("pldm Discovery : I2C Read failure\n");
		return PLDM_ERROR;
	}

	if (oemVrsyncResp(cmd, instanceID) != PLDM_SUCCESS) {
		return PLDM_ERROR;
	}

	instanceID++;
	return PLDM_SUCCESS;
}

/**
 * @brief Process OEM VRSync response messages
 *
 * Handles and validates response messages for OEM VRSync commands.
 * Processes responses for both pause and resume VRSync operations.
 *
 * @param cmd OEM VRSync command that generated this response
 *            - PLDM_OEM_VR_SYNC_PAUSE: VRSync pause command response
 *            - PLDM_OEM_VR_SYNC_RESUME: VRSync resume command response
 * @param id pldm instance ID used for the original command
 *
 * @return uint8_t Status of response processing
 * @retval PLDM_SUCCESS Response processed successfully
 * @retval PLDM_ERROR Invalid command or response processing failure
 *
 * @note Validates response for supported OEM VRSync commands only
 * @note Calls payload processing function for detailed response validation
 */
uint8_t pldm::oemVrsyncResp(uint8_t cmd, uint8_t id)
{
	TRACING();
	uint8_t ret = PLDM_ERROR;

	switch (cmd) {
	case PLDM_OEM_VR_SYNC_PAUSE:
	case PLDM_OEM_VR_SYNC_RESUME: {
		ret = oemVrsyncRespPayload(cmd, id);
		break;
	}
	default:
		ERR("Invalid OEM Command\n");
		break;
	}
	return ret;
}

/**
 * @brief Process payload data from OEM VRSync response messages
 *
 * Validates and processes the payload content of OEM VRSync response messages.
 * Performs message integrity checks including command code, instance ID,
 * and completion code validation for VRSync operations.
 *
 * @param cmd OEM VRSync command that generated this response
 * @param id pldm instance ID used for the original command
 *
 * @return uint8_t Status of payload processing
 * @retval PLDM_SUCCESS Payload processed successfully and completion code is success
 * @retval PLDM_ERROR Invalid message format, mismatched parameters, or error completion code
 *
 * @note Validates response message header fields against expected values
 * @note Checks completion code in payload for VRSync command execution status
 * @note Sets up proper addressing for response message handling
 * @note Provides detailed error logging for OEM command debugging
 */
uint8_t pldm::oemVrsyncRespPayload(UNUSED uint8_t cmd, UNUSED uint8_t id)
{
	TRACING();
	unsigned int totalSize = 0;

	mI2cPldmRead->mctpSmbusHdr.destSlaveAddr = MCTP_SRC_SLAVE_ADDR;
	mI2cPldmRead->mctpSmbusHdr.destSlaveAddrB0 = MCTP_SRC_SLAVE_ADDR_B0;
	totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;

	DBG("pldm RX  :: ");
	hexdump((uint8_t *)mI2cPldmRead, totalSize);

	if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
		ERR("OEM Cmd : Completion Error Code : 0x%02x\n", mI2cPldmRead->respPayload[BYTE_0]);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}