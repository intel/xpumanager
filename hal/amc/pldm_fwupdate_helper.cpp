/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "common.h"
#include "pldm_fwupdate.h"
#include "pldm.h"
#include <string>

/**
 * @brief Print firmware update command information
 *
 * Helper function to display human-readable names for firmware update
 * completion codes and transfer states.
 *
 * @param str Prefix string to display before the command information
 * @param cmd Firmware update command/completion code
 *
 * @note Static helper function for debugging and logging
 * @note Translates numeric codes to readable text descriptions
 */
static void printFwuCmdInfo(const char str[], uint8_t cmd)
{
	if (cmd == TRANSFER_COMPLETE) {
		DBG("%s TransferComplete \n", str);
	} else if (cmd == VERIFY_COMPLETE) {
		DBG("%s VerifyComplete \n", str);
	} else if (cmd == APPLY_COMPLETE) {
		DBG("%s ApplyComplete \n", str);
	}
}

/**
 * @brief Prepare REQUEST_UPDATE command payload
 *
 * Constructs the payload for the pldm REQUEST_UPDATE command by populating
 * the request structure with package information including transfer parameters,
 * component count, and version strings.
 *
 * @return uint8_t Status of request preparation
 * @retval PLDM_SUCCESS Request payload prepared successfully
 * @retval PLDM_ERROR Payload preparation failed
 *
 * @note Uses package information from parsed firmware file
 * @note Sets maximum transfer size and outstanding request limits
 * @note Populates component version information for current component
 */
uint8_t pldm::fwReqUpdate()
{
	TRACING();

	mReqUpdate.maxTransferSize = PLDM_MCTP_MAX_TRANSFER_SIZE;
	mReqUpdate.numComp = pkg->compImagesInfo.compImageCount;
	mReqUpdate.maxOutstandXferReqs = FWU_MAXIMUM_OUTSTANDING_TRANSFER_REQ;
	mReqUpdate.pkgDataLen = FWU_PACKAGE_DATALENGTH;
	mReqUpdate.compImgSetVerStrType = pkg->compImagesInfo.compImages[mCurComp].verStrType;
	mReqUpdate.compImgSetVerStrLen = pkg->compImagesInfo.compImages[mCurComp].verStrLen;
	memcpy(mReqUpdate.compImgSetVerStr, pkg->compImagesInfo.compImages[mCurComp].verStr,
		   mReqUpdate.compImgSetVerStrLen);

	mFwuCmdLen = FWU_COMMAND_BASE_SIZE + sizeof(struct fwuRequestUpdate) - sizeof(mReqUpdate.compImgSetVerStr) +
				 mReqUpdate.compImgSetVerStrLen;
	memcpy(&mI2cPldmWrite->respPayload[BYTE_0], &mReqUpdate, mFwuCmdLen);
	mI2cPldmWrite->respPayload[mFwuCmdLen] = crc8Smbus(mI2cPldmWrite->respPayload, mFwuCmdLen);

	return PLDM_SUCCESS;
}

/**
 * @brief Prepare PASS_COMPONENT_TABLE command payload
 *
 * Constructs the payload for the pldm PASS_COMPONENT_TABLE command by
 * populating component table information including classification,
 * identifier, version string, and comparison stamp.
 *
 * @return uint8_t Status of component table preparation
 * @retval PLDM_SUCCESS Component table payload prepared successfully
 * @retval PLDM_ERROR Payload preparation failed
 *
 * @note Uses component information from parsed firmware package
 * @note Sets transfer flags and component classification data
 * @note Includes component version string and comparison data
 */
uint8_t pldm::fwupdPassCompTable()
{
	TRACING();

	// Transferflag is needed for multi component image transfer
	// Since we just have only one component to transfer, we set it to end
	mPassCompTable.xferFlag = TRANSFERFLAG_END;
	mPassCompTable.compClass = pkg->compImagesInfo.compImages[mCurComp].compClassification;
	mPassCompTable.id = pkg->compImagesInfo.compImages[mCurComp].id;

	// In case of multiple components, get this value from GetfirmwareParameters
	// As per spec DSP0267_1.3.0 setting the value to "0x00" for single FD
	mPassCompTable.compClassIdx = COMPONENTCLASSIFICATIONINDEX;
	mPassCompTable.comparisonStamp = pkg->compImagesInfo.compImages[mCurComp].compComparisionStamp;
	mPassCompTable.verStrType = pkg->compImagesInfo.compImages[mCurComp].verStrType;
	mPassCompTable.verStrLen = pkg->compImagesInfo.compImages[mCurComp].verStrLen;
	memcpy(mPassCompTable.verStr, pkg->compImagesInfo.compImages[mCurComp].verStr, mPassCompTable.verStrLen);

	mFwuCmdLen = FWU_COMMAND_BASE_SIZE + sizeof(struct fwuPassCompTable) - sizeof(mPassCompTable.verStr) +
				 mPassCompTable.verStrLen;

	memcpy(&mI2cPldmWrite->respPayload[BYTE_0], &mPassCompTable, mFwuCmdLen);
	mI2cPldmWrite->respPayload[mFwuCmdLen] = crc8Smbus(mI2cPldmWrite->respPayload, mFwuCmdLen);

	return PLDM_SUCCESS;
}

/**
 * @brief Prepare UPDATE_COMPONENT command payload
 *
 * Constructs the payload for the pldm UPDATE_COMPONENT command by populating
 * component update information including classification, identifier, image size,
 * comparison stamp, and update options.
 *
 * @return uint8_t Status of update component preparation
 * @retval PLDM_SUCCESS Update component payload prepared successfully
 * @retval PLDM_ERROR Payload preparation failed
 *
 * @note Uses component information from parsed firmware package
 * @note Sets force update flag based on package component options
 * @note Includes component version string and comparison data
 * @note Follows DSP0267_1.3.0 specification for single FD components
 */
uint8_t pldm::fwUpdComp()
{
	TRACING();

	mUpdComp.compClass = pkg->compImagesInfo.compImages[mCurComp].compClassification;
	mUpdComp.id = pkg->compImagesInfo.compImages[mCurComp].id;

	// In case of multiple components, get this value from GetfirmwareParameters
	// As per spec DSP0267_1.3.0 setting the value to "0x00" for single FD
	mUpdComp.compClassIdx = COMPONENTCLASSIFICATIONINDEX;
	mUpdComp.comparisonStamp = pkg->compImagesInfo.compImages[mCurComp].compComparisionStamp;
	mUpdComp.componentimagesize = pkg->compImagesInfo.compImages[mCurComp].compSize;

	// UA will set this bit for any component which has the force update bit set in the
	// ComponentOptions field of the package header.
	mUpdComp.updateoptionflags = FWU_FORCEUPDATE;
	mUpdComp.verStrType = pkg->compImagesInfo.compImages[mCurComp].verStrType;
	mUpdComp.verStrLen = pkg->compImagesInfo.compImages[mCurComp].verStrLen;
	memcpy(mUpdComp.verStr, pkg->compImagesInfo.compImages[mCurComp].verStr, mUpdComp.verStrLen);

	mFwuCmdLen = FWU_COMMAND_BASE_SIZE + sizeof(struct fwUpdComp) - sizeof(mUpdComp.verStr) + mUpdComp.verStrLen;

	memcpy(&mI2cPldmWrite->respPayload[BYTE_0], &mUpdComp, mFwuCmdLen);
	mI2cPldmWrite->respPayload[mFwuCmdLen] = crc8Smbus(mI2cPldmWrite->respPayload, mFwuCmdLen);

	return PLDM_SUCCESS;
}

/**
 * @brief Read firmware data requests from firmware device
 *
 * Handles the complete firmware data transfer process by reading requests from
 * the AMC firmware device and responding with appropriate firmware data chunks.
 * Manages the entire firmware transfer protocol including progress monitoring.
 *
 * @return uint8_t Status of firmware data transfer
 * @retval PLDM_SUCCESS All firmware data transferred successfully
 * @retval PLDM_ERROR Communication failure or transfer error
 *
 * @note Handles REQUEST_FIRMWARE_DATA, TRANSFER_COMPLETE, VERIFY_COMPLETE, and APPLY_COMPLETE
 * @note Validates transfer parameters and responds with appropriate completion codes
 * @note Updates progress percentage during transfer operations
 * @note Implements error handling for out-of-range and invalid transfer requests
 * @note Manages file positioning and data reading from firmware package
 */
uint8_t pldm::readFromFD()
{
	TRACING();
	uint32_t offset = 0;
	uint32_t lenToSend = 0;
	uint32_t totalSize = 0;
	uint8_t *rptr = (uint8_t *)mI2cPldmRead;

	// for a specific component, do read and write until it reaches end of component
	while (true) {

		if (i2cobj->readAmc(rptr + 1, PLDM_MAX_RESPONSE_SIZE) != true) {
			ERR("pldm from FD : I2C Read failure\n");
			return PLDM_ERROR;
		}

		if (mI2cPldmRead->mctpSmbusHdr.cmdCode != MCTP_CMD_CODE) {
			// This we need if we read a response that is not a mctp response
			// Need to replace this sleep with "wait for signal"
			MSLEEP(MCTP_RESPONSE_DELAY_MS);
			continue;
		}

		totalSize = mI2cPldmRead->mctpSmbusHdr.byteCount + 3;

		if (mI2cPldmRead->pldmHdr.cmdType != PLDM_FIRMWARE_UPDATE) {
			ERR("FWU : Invalid Command Type %d, expected %d\n", mI2cPldmRead->pldmHdr.cmdType, PLDM_FIRMWARE_UPDATE);
			return PLDM_ERROR;
		}

		if (mI2cPldmRead->pldmHdr.cmdCode == REQUEST_FIRMWARE_DATA) {
			offset = bytesToInt(mI2cPldmRead->respPayload[BYTE_3], mI2cPldmRead->respPayload[BYTE_2],
								mI2cPldmRead->respPayload[BYTE_1], mI2cPldmRead->respPayload[BYTE_0]);
			lenToSend = bytesToInt(mI2cPldmRead->respPayload[BYTE_7], mI2cPldmRead->respPayload[BYTE_6],
								   mI2cPldmRead->respPayload[BYTE_5], mI2cPldmRead->respPayload[BYTE_4]);

			mProgPercent = (int)(offset * 100) / pkg->compImagesInfo.compImages[mCurComp].compSize;

			DBG("Card - %02d :: offset = %u, lenToSend = %u, mProgPercent = %d\n", mCardNum, offset, lenToSend,
				mProgPercent);
			if (offset >= pkg->compImagesInfo.compImages[mCurComp].compSize) {
				DBG("FWU : Offset exceeds component size, Respond back to AMC!!!\n");
				if (respondtoamc(DATA_OUT_OF_RANGE) != PLDM_SUCCESS) {
					ERR("FWU : Transfer fail, exiting!!!\n");
					return PLDM_ERROR;
				}
			} else if (lenToSend > PLDM_MCTP_MAX_TRANSFER_SIZE) {
				DBG("FWU : Invalid Transfer Length, Respond back to AMC!!!\n");
				if (respondtoamc(INVALID_TRANSFER_LENGTH) != PLDM_SUCCESS) {
					ERR("FWU : Transfer fail, exiting!!!\n");
					return PLDM_ERROR;
				}
			} else {
				if (writeToAMCCompXfer(offset, lenToSend, PLDM_RESPONSE_COMPLETION) != PLDM_SUCCESS) {
					ERR("FWU : Transfer fail, exiting!!!\n");
					return PLDM_ERROR;
				}
			}
		} else if ((mI2cPldmRead->pldmHdr.cmdCode == TRANSFER_COMPLETE) ||
				   (mI2cPldmRead->pldmHdr.cmdCode == VERIFY_COMPLETE) ||
				   (mI2cPldmRead->pldmHdr.cmdCode == APPLY_COMPLETE)) {
			printFwuCmdInfo(">>> Received", mI2cPldmRead->pldmHdr.cmdCode);
			mProgPercent = 100;

			DBG("pldm RX  :: ");
			hexdump((uint8_t *)mI2cPldmRead, totalSize);

			// Check for Completion Code
			if (mI2cPldmRead->respPayload[BYTE_0] != PLDM_SUCCESS) {
				ERR("FWU Err :0x%02x, respond COMMAND_NOT_EXPECTED to AMC and exit XPUM!!!\n",
					mI2cPldmRead->respPayload[BYTE_0]);
				if (respondtoamc(COMMAND_NOT_EXPECTED) != PLDM_SUCCESS) {
					ERR("FWU : Respond to AMC failed!!!\n");
				}
				return PLDM_ERROR;
			} else {
				if (respondtoamc(PLDM_RESPONSE_COMPLETION) != PLDM_SUCCESS) {
					ERR("FWU Respond to AMC failed!!!\n");
					return PLDM_ERROR;
				}
				printFwuCmdInfo("<<< Respond Success to AMC for", mI2cPldmRead->pldmHdr.cmdCode);

				if (mI2cPldmRead->pldmHdr.cmdCode == APPLY_COMPLETE) {
					// Exit the loop and return success
					return PLDM_SUCCESS;
				}
			}
		} else {
			DBG("FWU : Invalid Command Code %d.. respond AMC with COMMAND_NOT_EXPECTED\n",
				mI2cPldmRead->pldmHdr.cmdCode);
			if (respondtoamc(COMMAND_NOT_EXPECTED) != PLDM_SUCCESS) {
				ERR("FWU : Respond to AMC failed...\n");
				return PLDM_ERROR;
			}
		}
	}
}

/**
 * @brief Transfer component data to AMC device
 *
 * Reads firmware component data from the package file at the specified offset
 * and length, then constructs and sends a pldm response containing the requested
 * firmware data to the AMC device.
 *
 * @param offset Byte offset within the component image to start reading from
 * @param lenToSend Number of bytes to read and transfer
 * @param completioncode pldm completion code to include in the response
 *
 * @return uint8_t Status of data transfer
 * @retval PLDM_SUCCESS Data transferred successfully
 * @retval PLDM_ERROR File operation error or transfer failure
 *
 * @note Handles file seeking and reading from firmware package
 * @note Supports padding for non-32-byte-aligned images
 * @note Constructs proper pldm response headers and checksums
 * @note Manages component location offset and size validation
 */
uint8_t pldm::writeToAMCCompXfer(uint32_t offset, uint32_t lenToSend, uint8_t completioncode)
{
	TRACING();
	uint8_t totalLen = 0;
	uint8_t amcfilebuffer[PLDM_FWUPDATE_MAX_PAYLOAD_SIZE] = {0};
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;
	uint32_t compStartLoc = pkg->compImagesInfo.compImages[mCurComp].compLocOffset;
	uint32_t compTotalSize = pkg->compImagesInfo.compImages[mCurComp].compSize;
	uint8_t id = mI2cPldmRead->pldmHdr.instanceID;
	uint8_t cmd = mI2cPldmRead->pldmHdr.cmdCode;

	totalLen =
		sizeof(struct mctpSmbusI2cHdr) + sizeof(struct pldmHdr) + PLDM_CMD_RESP_PAYLOAD_SIZE + (uint8_t)lenToSend;

	// UA will calculate this PAK_SEQ for multi packet messages, currently we are not using multi packet messages
	commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (totalLen - 3),
						MCTP_INTEGRITY_CHECK);
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;
	pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, id, PLDM_FIRMWARE_UPDATE, cmd, PLDM_ASYNC_REQUEST_NOTIFY,
						PLDM_RESPONSE);

	if (fseek(mCompFp, compStartLoc + offset, SEEK_SET) != 0) {
		ERR("FWU: Error seeking in file\n");
		fclose(mCompFp);
		return PLDM_ERROR;
	}

	if ((offset + lenToSend) <= compTotalSize) {
		size_t bytesRead = fread(amcfilebuffer, 1, lenToSend, mCompFp);
		if (bytesRead != lenToSend) {
			ERR("FWU: Length Error\n");
		}

		// Copy the "WriteToFd" payload
		memcpy(&mI2cPldmWrite->respPayload[BYTE_1], amcfilebuffer, lenToSend);
	} else {
		// These code path is needed for padding, if the AMC image is not 32Byte aligned
		//  we need to fill with 0x00 and send the remaining bytes
		memset(mI2cPldmWrite->respPayload, 0x0, 128);
		uint32_t remBytes = pkg->compImagesInfo.compImages[mCurComp].compSize - offset;

		size_t bytesRead = fread(amcfilebuffer, 1, remBytes, mCompFp);
		if (bytesRead != remBytes) {
			ERR("FWU : Length Error\n");
		}

		// Copy the "WriteToFd" payload
		memcpy(&mI2cPldmWrite->respPayload[BYTE_1], amcfilebuffer, remBytes);
	}

	// Update Completion Code and CRC
	mI2cPldmWrite->respPayload[BYTE_0] = completioncode;
	mI2cPldmWrite->respPayload[totalLen] = crc8Smbus(mI2cPldmWrite->respPayload, totalLen - 1);

	if (i2cobj->writeAmc(wptr + 1, totalLen) != true) {
		ERR("FWU : I2C Write failure\n");
		return PLDM_ERROR;
	}

	if (pkg->compImagesInfo.compImages[mCurComp].compSize == (offset + lenToSend)) {
		DBG("Transfer complete!! RequestFirmwareData Success...\n");
	}

	MSLEEP(FWU_TRANSFER_DELAY_MS);
	return PLDM_SUCCESS;
}

/**
 * @brief Send response to AMC device with completion code
 *
 * Constructs and sends a pldm response message to the AMC device with the
 * specified completion code. Used to acknowledge firmware update commands
 * and indicate success or failure status.
 *
 * @param completioncode pldm completion code to send in the response
 *                      - PLDM_SUCCESS: Operation completed successfully
 *                      - COMMAND_NOT_EXPECTED: Invalid or unexpected command
 *                      - DATA_OUT_OF_RANGE: Invalid offset or data request
 *                      - INVALID_TRANSFER_LENGTH: Transfer size exceeds limits
 *
 * @return uint8_t Status of response transmission
 * @retval PLDM_SUCCESS Response sent successfully
 * @retval PLDM_ERROR I2C communication failure
 *
 * @note Uses same instance ID and command code from received request
 * @note Constructs proper pldm response headers and protocol fields
 * @note Includes CRC checksum for message integrity
 */
uint8_t pldm::respondtoamc(uint8_t completioncode)
{
	TRACING();
	uint8_t totalLen = FWU_COMMAND_BASE_SIZE + 1;
	uint8_t *wptr = (uint8_t *)mI2cPldmWrite;
	uint8_t id = mI2cPldmRead->pldmHdr.instanceID;
	uint8_t cmd = mI2cPldmRead->pldmHdr.cmdCode;

	// UA will calculate this PAK_SEQ for multi packet messages, currently we are not using multi packet messages
	commandConstruction(&mI2cPldmWrite->mctpSmbusHdr, MCTP_SOM, MCTP_EOM, MCTP_PAK_SEQ, (totalLen - 3),
						MCTP_INTEGRITY_CHECK);
	mI2cPldmWrite->mctpSmbusHdr.msgType = PLDM_OVER_MCTP;
	pldmHdrConstruction(&mI2cPldmWrite->pldmHdr, id, PLDM_FIRMWARE_UPDATE, cmd, PLDM_ASYNC_REQUEST_NOTIFY,
						PLDM_RESPONSE);

	// Update Completion Code
	mI2cPldmWrite->respPayload[BYTE_0] = completioncode;
	mI2cPldmWrite->respPayload[BYTE_1] = crc8Smbus(mI2cPldmWrite->respPayload, totalLen - 1);

	DBG("pldm TX  :: ");
	hexdump((uint8_t *)mI2cPldmWrite, (unsigned int)totalLen);

	if (i2cobj->writeAmc(wptr + 1, totalLen) != true) {
		ERR("FWU : I2C Write failure\n");
		return PLDM_ERROR;
	}

	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);
	return PLDM_SUCCESS;
}
