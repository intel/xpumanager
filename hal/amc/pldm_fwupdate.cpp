/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "pldm_fwupdate.h"
#include "amc_update_interrupt.h"
#include "pldm.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

/**
 * @brief Execute pldm firmware update command
 *
 * Sends a pldm firmware update request to the device and waits for its response. Manages the pldm
 * instance ID and hands the response to fwUpdateResp(). The request and the response may each span
 * several MCTP packets.
 *
 * @param cmd Firmware update command code to execute
 *
 * @return uint8_t Status of the command execution
 * @retval PLDM_SUCCESS Command executed successfully
 * @retval PLDM_ERROR_* Various error conditions
 *
 * @note Automatically manages pldm instance ID wrapping at maximum value
 * @note A request without a response within DSP0240 PT2 is retried PN1 times with the same
 *       Instance ID
 */
uint8_t pldm::fwUpdCmd(uint8_t cmd)
{
	TRACING();

	// Cleared until a response supplies a completion code, so a timeout is never mistaken for one
	mLastFwuCompletionCode = PLDM_ERROR;

	// Fill the payload based on command
	if (pldmFwUpdFillPayload(cmd) != PLDM_SUCCESS) {
		ERR("FWU : Failed to fill payload for command 0x{:02x}\n", cmd);
		return PLDM_ERROR;
	}

	// The retries below keep this Instance ID
	const uint8_t requestId = fwUpdNextInstanceId();

	struct pldmHdr hdr = {};
	pldmHdrConstruction(&hdr, requestId, PLDM_FIRMWARE_UPDATE, cmd, PLDM_ASYNC_REQUEST_NOTIFY, PLDM_REQUEST);
	const mctpMsgInfo reqInfo = mctpRequestInfo();
	const std::vector<uint8_t> request = pldmBuildMessage(hdr, mFwuTxPayload.data(), mFwuTxPayload.size());

	// For ActivateFirmware command don't do I2C read, post AMC fw update this cmd will
	// reboot the AMC, I2C channel will be lost and we won't get any response
	if (cmd == ACTIVATE_FIRMWARE) {
		if (mctpSendMessage(reqInfo, request) != MCTP_SUCCESS) {
			ERR("FWU : Failed to send command 0x{:02x}\n", cmd);
			return PLDM_ERROR;
		}
		return PLDM_SUCCESS;
	}

	// DSP0240 PN1: a request that gets no response within PT2 is sent again, unchanged and with the
	// same Instance ID so the responder can tell it is a retry.
	bool responded = false;
	for (int attempt = 0; attempt <= PLDM_PN1_RETRIES && !responded; attempt++) {
		if (attempt > 0) {
			DBG("FWU : Card {:02}: no response to cmd 0x{:02x} within {} ms, retrying ({}/{})\n", mCardNum, cmd,
				PLDM_PT2_MS, attempt, PLDM_PN1_RETRIES);
		}
		if (mctpSendMessage(reqInfo, request) != MCTP_SUCCESS) {
			ERR("FWU : Failed to send command 0x{:02x}\n", cmd);
			return PLDM_ERROR;
		}

		const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(PLDM_PT2_MS);
		while (true) {
			const auto remaining =
				std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
			std::vector<uint8_t> msg;
			mctpMsgInfo info = {};
			if (remaining.count() <= 0 || mctpReceiveMessage(msg, info, remaining) != MCTP_SUCCESS) {
				break;
			}

			if (info.tagOwner && msg[0] == PLDM_OVER_MCTP && fwUpdStoreRxMessage(msg, info) == PLDM_SUCCESS) {
				// The FD retrying a stage result whose response it missed, such as ApplyComplete while
				// this is already the next component's UpdateComponent, is answered again (DSP0240).
				if (fwUpdReplayIfRetry()) {
					continue;
				}
				// A request from the FD that crosses a cancel still gets its response (DSP0267 CANCEL_PENDING).
				if (cmd == CANCEL_UPDATE || cmd == CANCEL_UPDATE_COMPONENT) {
					fwUpdAnswerRequestWhileCancelling();
					continue;
				}
			}
			// DSP0236: the response clears TO and carries the request's message tag. Anything else,
			// such as a request the FD is still retrying from an earlier session, is not this response.
			if (info.tagOwner || info.msgTag != reqInfo.msgTag || msg[0] != PLDM_OVER_MCTP) {
				DBG("FWU : Ignoring MCTP message (type 0x{:02x}, TO {}, tag {}) while waiting for cmd 0x{:02x} "
					"response\n",
					msg[0], info.tagOwner, info.msgTag, cmd);
				continue;
			}

			if (fwUpdStoreRxMessage(msg, info) != PLDM_SUCCESS) {
				continue;
			}
			// DSP0240: act on a response only when its PLDM Type, Command Code and Instance ID match
			// the outstanding request.
			const struct pldmHdr &rsp = mI2cPldmRead->pldmHdr;
			if (rsp.request != PLDM_RESPONSE || rsp.cmdType != PLDM_FIRMWARE_UPDATE || rsp.cmdCode != cmd ||
				rsp.instanceID != requestId) {
				DBG("FWU : Ignoring PLDM message (type 0x{:02x}, cmd 0x{:02x}, instance {}) that does not answer "
					"cmd 0x{:02x} instance {}\n",
					rsp.cmdType, rsp.cmdCode, rsp.instanceID, cmd, requestId);
				continue;
			}
			responded = true;
			break;
		}
	}

	if (!responded) {
		DBG("FWU : Card {:02}: no response to cmd 0x{:02x} after {} tries\n", mCardNum, cmd, PLDM_PN1_RETRIES + 1);
		return PLDM_ERROR;
	}

	if (fwUpdateResp(cmd, requestId) != PLDM_SUCCESS) {
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Take the Instance ID for a new firmware update request
 *
 * Every new request takes a new Instance ID, whatever became of the previous one, and only retries
 * of a request reuse its ID (DSP0240).
 *
 * @return uint8_t Instance ID for the request, 1 to PLDM_INSTANCE_ID_MAX - 1
 */
uint8_t pldm::fwUpdNextInstanceId()
{
	if (instanceID == PLDM_INSTANCE_ID_MAX) {
		instanceID = 1;
	}
	return instanceID++;
}

/**
 * @brief Fill payload data for pldm firmware update commands
 *
 * Builds mFwuTxPayload, the request data that follows the PLDM header, for the specified
 * firmware update command.
 *
 * @param cmd Firmware update command code that determines payload structure
 *
 * @return uint8_t Status of payload construction
 * @retval PLDM_SUCCESS Payload filled successfully
 * @retval PLDM_ERROR Unsupported command or payload construction failure
 *
 * @note Some commands like QUERY_DEVICE_IDENTIFIERS require no payload data
 */
uint8_t pldm::pldmFwUpdFillPayload(uint8_t cmd)
{
	TRACING();
	uint8_t ret = PLDM_SUCCESS;

	switch (cmd) {
	case QUERY_DEVICE_IDENTIFIERS:
	case GET_FIRMWARE_PARAMETERS:
	case CANCEL_UPDATE_COMPONENT:
	case CANCEL_UPDATE:
	case GET_STATUS:
		// No request data
		mFwuTxPayload.clear();
		break;

	case ACTIVATE_FIRMWARE:
		mFwuTxPayload = {0x01}; // TRUE - this will activate AMC to update firmware
		break;

	case REQUEST_UPDATE:
		if (fwReqUpdate() != PLDM_SUCCESS) {
			ERR("FWU : Failed to fill payload for REQUEST_UPDATE\n");
			return PLDM_ERROR;
		}
		break;

	case PASS_COMPONENT_TABLE:
		if (fwupdPassCompTable() != PLDM_SUCCESS) {
			ERR("FWU : Failed to fill payload for PASS_COMPONENT_TABLE\n");
			return PLDM_ERROR;
		}
		break;

	case UPDATE_COMPONENT:
		if (fwUpdComp() != PLDM_SUCCESS) {
			ERR("FWU : Failed to fill payload for UPDATE_COMPONENT\n");
			return PLDM_ERROR;
		}
		break;

	default:
		ERR("FWU : Invalid Firmware Update Command\n");
		ret = PLDM_ERROR;
		break;
	}

	return ret;
}

/**
 * @brief Take in a received firmware update message
 *
 * Keeps the PLDM payload in mFwuRxPayload and the MCTP header fields in mFwuRxInfo, and copies the
 * PLDM header plus the start of the payload into mI2cPldmRead, where the response handlers read
 * fixed fields from. Payloads longer than mI2cPldmRead->respPayload (GetFirmwareParameters) are
 * only complete in mFwuRxPayload.
 *
 * @param msg MCTP message body, starting with the message type byte
 * @param info MCTP transport header fields of the message
 *
 * @return uint8_t PLDM_SUCCESS, or PLDM_ERROR if msg is too short to hold a PLDM header
 */
uint8_t pldm::fwUpdStoreRxMessage(const std::vector<uint8_t> &msg, const mctpMsgInfo &info)
{
	TRACING();

	if (msg.size() < 1 + sizeof(struct pldmHdr)) {
		ERR("FWU : PLDM message too short ({} bytes)\n", msg.size());
		return PLDM_ERROR;
	}

	mFwuRxPayload.assign(msg.begin() + 1 + sizeof(struct pldmHdr), msg.end());
	mFwuRxInfo = info;

	memset(mI2cPldmRead, 0, sizeof(*mI2cPldmRead));
	// Message type too, so checks on mI2cPldmRead such as isResponseTo() see the message as received
	mI2cPldmRead->mctpSmbusHdr.ic = static_cast<uint8_t>((msg[0] >> 7) & 0x1);
	mI2cPldmRead->mctpSmbusHdr.msgType = static_cast<uint8_t>(msg[0] & 0x7F);
	memcpy(&mI2cPldmRead->pldmHdr, msg.data() + 1, sizeof(struct pldmHdr));
	memcpy(mI2cPldmRead->respPayload, mFwuRxPayload.data(),
		   std::min(mFwuRxPayload.size(), sizeof(mI2cPldmRead->respPayload)));

	return PLDM_SUCCESS;
}

/**
 * @brief Answer a firmware device request received while a cancel is outstanding
 *
 * DSP0267 CANCEL_PENDING is "sent by the UA when it receives a RequestFirmwareData command after
 * sending a CancelUpdate or CancelUpdateComponent command". Any other command from the FD is no
 * longer part of the expected sequence at that point and gets COMMAND_NOT_EXPECTED.
 *
 * @note The request has been stored by fwUpdStoreRxMessage()
 */
void pldm::fwUpdAnswerRequestWhileCancelling()
{
	TRACING();

	if (mI2cPldmRead->pldmHdr.request != PLDM_REQUEST || mI2cPldmRead->pldmHdr.cmdType != PLDM_FIRMWARE_UPDATE) {
		DBG("FWU : Ignoring MCTP message that is not a firmware update request while cancelling\n");
		return;
	}

	const uint8_t cc = static_cast<uint8_t>(
		(mI2cPldmRead->pldmHdr.cmdCode == REQUEST_FIRMWARE_DATA) ? CANCEL_PENDING : COMMAND_NOT_EXPECTED);
	DBG("FWU : Answering FD command 0x{:02x} with 0x{:02x} while the cancel is outstanding\n",
		mI2cPldmRead->pldmHdr.cmdCode, cc);
	if (fwUpdSendResponse(cc, {}) != PLDM_SUCCESS) {
		ERR("FWU : Respond to AMC failed while cancelling\n");
	}
}

/**
 * @brief Send CancelUpdate or CancelUpdateComponent, retrying while the FD cannot act on it yet
 *
 * DSP0267 sections 12.13 and 12.14: on BUSY_IN_BACKGROUND the UA shall retry after UA_T1. A cancel
 * that gets no response has already been retried by fwUpdCmd() (DSP0240 PN1). Any other outcome is
 * final and left in mLastFwuCompletionCode for the caller to interpret.
 *
 * @param cmd CANCEL_UPDATE or CANCEL_UPDATE_COMPONENT
 *
 * @return uint8_t PLDM_SUCCESS when the FD accepted the cancel, PLDM_ERROR otherwise
 */
uint8_t pldm::fwUpdSendCancel(uint8_t cmd)
{
	TRACING();
	// Local bound on the retries; the specification does not limit them
	constexpr int kMaxCancelAttempts = 10;

	for (int attempt = 1; attempt <= kMaxCancelAttempts; attempt++) {
		DBG(">>> Send cancel command 0x{:02x} (attempt {}/{})\n", cmd, attempt, kMaxCancelAttempts);
		if (fwUpdCmd(cmd) == PLDM_SUCCESS) {
			DBG("<<< Cancel command 0x{:02x} Success\n", cmd);
			return PLDM_SUCCESS;
		}
		if (mLastFwuCompletionCode != BUSY_IN_BACKGROUND) {
			return PLDM_ERROR;
		}
		MSLEEP(FWU_UA_T1_MS);
	}

	return PLDM_ERROR;
}

/**
 * @brief Take the firmware device out of update mode with CancelUpdate (DSP0267 section 12.14)
 *
 * CancelUpdate is accepted in every update mode state and returns the FD to IDLE (DSP0267 Table 15).
 * A NOT_IN_UPDATE_MODE completion code means the FD has already left update mode.
 *
 * @return uint8_t PLDM_SUCCESS once the FD is out of update mode, PLDM_ERROR otherwise
 */
uint8_t pldm::fwUpdCancel()
{
	TRACING();

	if (fwUpdSendCancel(CANCEL_UPDATE) == PLDM_SUCCESS) {
		return PLDM_SUCCESS;
	}
	if (mLastFwuCompletionCode == NOT_IN_UPDATE_MODE) {
		DBG("<<< CANCEL UPDATE: firmware device is already out of update mode\n");
		return PLDM_SUCCESS;
	}

	ERR("FWU : CancelUpdate failed on card {}; the AMC may stay in update mode until it times out\n", mCardNum);
	return PLDM_ERROR;
}

/**
 * @brief Stop the current component transfer so the component can be sent again
 *
 * DSP0267 section 12.13: CancelUpdateComponent moves the FD from DOWNLOAD, VERIFY or APPLY back to
 * READY XFER, where it accepts another UpdateComponent. INVALID_STATE_FOR_COMMAND means the FD is in
 * update mode but no component update is in progress, which leaves it ready for UpdateComponent too.
 *
 * @return uint8_t PLDM_SUCCESS when the FD can take UpdateComponent, PLDM_ERROR otherwise
 *         (NOT_IN_UPDATE_MODE, for one, means it has left update mode altogether)
 */
uint8_t pldm::fwUpdCancelComponent()
{
	TRACING();

	if (fwUpdSendCancel(CANCEL_UPDATE_COMPONENT) == PLDM_SUCCESS ||
		mLastFwuCompletionCode == INVALID_STATE_FOR_COMMAND) {
		return PLDM_SUCCESS;
	}

	ERR("FWU : CancelUpdateComponent failed on card {:02} (completion code 0x{:02x})\n", mCardNum,
		mLastFwuCompletionCode);
	return PLDM_ERROR;
}

/**
 * @brief Whether a component of the parsed package should be left untouched
 *
 * A composite package can carry components meant for interfaces other than this one. When a
 * component filter is armed, every component whose ComponentIdentifier does not match it is
 * skipped instead of being pushed to the firmware device.
 *
 * @param comp Index into the parsed package's component image table
 * @return bool True when the component must not be transferred
 */
bool pldm::fwUpdSkipComp(uint8_t comp) const
{
	if (mCompIdFilter == 0) {
		return false;
	}

	return pkg->compImagesInfo.compImages[comp].id != mCompIdFilter;
}

/**
 * @brief Number of components this update will actually transfer
 *
 * Reported to the firmware device in RequestUpdate, so it has to account for the component filter
 * rather than the package's total component count.
 *
 * @return uint16_t Count of components that pass the filter
 */
uint16_t pldm::fwUpdCompCount() const
{
	if (mCompIdFilter == 0) {
		return pkg->compImagesInfo.compImageCount;
	}

	uint16_t count = 0;
	for (int i = 0; i < pkg->compImagesInfo.compImageCount; i++) {
		if (pkg->compImagesInfo.compImages[i].id == mCompIdFilter) {
			count++;
		}
	}

	return count;
}

/**
 * @brief Wait for the FD to reach READY XFER after it reported a successful apply
 *
 * The FD moves from APPLY to READY XFER only on receiving a successful response to ApplyComplete
 * (DSP0267 section 7.6 step 4). If that response is lost the FD retries ApplyComplete within PT3 of
 * the first try (DSP0240 Table 6), and until then it rejects UpdateComponent and ActivateFirmware.
 * GetStatus is accepted in APPLY, and while fwUpdCmd() waits for its response it answers such a
 * retry again, so polling it until READY XFER makes sure the FD has the response before the next
 * state changing command.
 *
 * @return uint8_t PLDM_SUCCESS once GetStatus reports READY XFER, PLDM_ERROR otherwise
 */
uint8_t pldm::fwUpdWaitForReadyXfer()
{
	TRACING();
	// Local pause between GetStatus polls
	constexpr unsigned int kPollIntervalMs = 500;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(PLDM_PT3_MAX_MS);

	while (true) {
		if (fwUpdCmd(GET_STATUS) != PLDM_SUCCESS) {
			ERR("FWU : GetStatus Failed!!!\n");
			return PLDM_ERROR;
		}
		if (mFwuCurrentState == GETSTATUS_READY_XFER) {
			return PLDM_SUCCESS;
		}
		if (std::chrono::steady_clock::now() > deadline) {
			ERR("Card {:02}: firmware device still in state {} {} ms after ApplyComplete, expected READY XFER\n",
				mCardNum, mFwuCurrentState, PLDM_PT3_MAX_MS);
			return PLDM_ERROR;
		}
		MSLEEP(kPollIntervalMs);
	}
}

/**
 * @brief Update one component of an update session (DSP0267 section 7.6)
 *
 * Sends UpdateComponent and serves the FD's requests until it has applied the component. When a
 * stage stalls past its Table 2 timer (UA_T2 for the transfer, UA_T3 or UA_T6 for verify and
 * apply), or the FD reports a failed transfer, verification or apply, the UA sends
 * CancelUpdateComponent (DSP0267 sections 12.7 to 12.9 and 12.12, Table 2) and may resend the same
 * component image (section 12.13), up to FWU_COMPONENT_TRANSFER_ATTEMPTS times in all.
 *
 * @param comp Index of the component in the package
 *
 * @return uint8_t PLDM_SUCCESS once the component is applied and the FD is back in READY XFER,
 *         PLDM_ERROR otherwise
 */
uint8_t pldm::fwUpdComponent(uint8_t comp)
{
	TRACING();
	mCurComp = comp;

	for (int attempt = 1;; attempt++) {
		mProgPercent = 0;
		DBG(">>> Send UpdateComponent for component {}\n", comp + 1);
		if (fwUpdCmd(UPDATE_COMPONENT) != PLDM_SUCCESS) {
			ERR("FWU : UpdateComponent Failed!!! \n");
			return PLDM_ERROR;
		}
		DBG("<<< UpdateComponent Success...\n");

		DBG("\n========= Firmware Update Read from FirmwareDevice(FD) for Component :: {} =========\n", comp + 1);
		const fwuTransferResult xfer = readFromFD();
		if (xfer == fwuTransferResult::Applied) {
			return fwUpdWaitForReadyXfer();
		}
		if (xfer == fwuTransferResult::RestartRequired) {
			ERR("AMC on card {:02} asks for the update to be cancelled and run again\n", mCardNum);
			return PLDM_ERROR;
		}
		if (xfer == fwuTransferResult::Failed || AmcUpdateInterruptGuard::cancelRequested()) {
			return PLDM_ERROR;
		}

		// StageTimeout or ComponentFailed: the UA shall send CancelUpdateComponent, which returns the FD to
		// READY XFER, ready for another UpdateComponent.
		if (fwUpdCancelComponent() != PLDM_SUCCESS) {
			return PLDM_ERROR;
		}
		if (attempt >= FWU_COMPONENT_TRANSFER_ATTEMPTS) {
			ERR("Card {:02}: component {} failed {} times, giving up\n", mCardNum, comp + 1, attempt);
			return PLDM_ERROR;
		}
		ERR("Card {:02}: resending component {} after CancelUpdateComponent (attempt {}/{})\n", mCardNum, comp + 1,
			attempt + 1, FWU_COMPONENT_TRANSFER_ATTEMPTS);
	}
}

/**
 * @brief Initialize pldm firmware update process
 *
 * Parses the firmware package file and runs one update session covering all of its components
 * (DSP0267 section 7.4): QueryDeviceIdentifiers and GetFirmwareParameters, one RequestUpdate, a
 * PassComponentTable per component, then UpdateComponent and the transfer, verify and apply of each
 * component in package order (fwUpdComponent()), and finally ActivateFirmware.
 *
 * Components filtered out by mCompIdFilter are left out of the session: they are not counted in
 * RequestUpdate, not passed in the component table and never transferred.
 *
 * @param pkgFilePath Path to the firmware package file to process
 *
 * @return uint8_t Status of the firmware update initialization
 * @retval PLDM_SUCCESS All firmware update operations completed successfully
 * @retval PLDM_ERROR File access error, parsing failure, or command execution error
 *
 * @note This function performs the complete firmware update workflow
 * @note Automatically sends CANCEL_UPDATE commands on failure for cleanup
 * @note Manages file operations and closes file handle before returning
 */
uint8_t pldm::fwUpdInitialize(const char *pkgFilePath)
{
	TRACING();
	uint8_t ret = PLDM_ERROR;

	// Inventory, before the FD is asked to enter update mode
	fwupd_cmds_t inventoryCmds[] = {{"QueryDeviceIdentifiers", QUERY_DEVICE_IDENTIFIERS},
									{"GetFirmwareParameters", GET_FIRMWARE_PARAMETERS}};

	// Initializing pldm Firmware Update variables to Zero
	mReqUpdate = {};
	mPassCompTable = {};
	mUpdComp = {};
	mLastFwuCompletionCode = PLDM_SUCCESS;
	mFwuLastAnswered = {};

	// Track whether an update session has been established on the device
	// (RequestUpdate succeeded). When true, we must send CancelUpdate on any
	// early return so the device does not stay stuck in update mode.
	bool updateStarted = false;
	uint8_t finalStatus = PLDM_ERROR;

	// Closes the package file, frees the parsed package, and sends CancelUpdate
	// if an update session was started but the flow did not complete successfully.
	auto cleanupAndReturn = [&](uint8_t status) -> uint8_t {
		finalStatus = status;
		if (status != PLDM_SUCCESS && AmcUpdateInterruptGuard::cancelRequested()) {
			ERR("Firmware update of card {:02} interrupted{}\n", mCardNum,
				updateStarted ? ", cancelling the update on the AMC" : "");
		}
		if (updateStarted && status != PLDM_SUCCESS && mLastFwuCompletionCode != NOT_IN_UPDATE_MODE) {
			(void)fwUpdCancel();
		}
		pkg.reset();
		if (mCompFp != NULL) {
			fclose(mCompFp);
			mCompFp = NULL;
		}
		return finalStatus;
	};

	DBG("\n=====================AMC Firmware File Parser===============================\n");
	if (FOPEN_S(&mCompFp, pkgFilePath, "rb") != 0 || mCompFp == NULL) {
		ERR("Failed to open package file: {}\n", pkgFilePath);
		return PLDM_ERROR;
	}

	uint8_t result = fwpkgParseInfo(pkgFilePath);
	if (result != PLDM_SUCCESS) {
		ERR("Failed to parse firmware package info from file: {}\n", pkgFilePath);
		return cleanupAndReturn(PLDM_ERROR);
	} else {
		DBG("Firmware package info parsed successfully from file: {}\n", pkgFilePath);
	}
	DBG("Firmware package info parsed successfully from AMC img file for card : {:02}\n", mCardNum);

	// A filtered update has nothing to do when the package does not carry the requested component.
	// Report it rather than sending RequestUpdate with a component count of zero.
	if (mCompIdFilter != 0 && fwUpdCompCount() == 0) {
		ERR("Firmware package '{}' does not contain a component with identifier 0x{:04X}.\n", pkgFilePath,
			mCompIdFilter);
		return cleanupAndReturn(PLDM_ERROR);
	}

	// If the user requested --force, make sure every component image in the package
	// advertises the ForceUpdate capability (bit 0 of ComponentOptions per DSP0267).
	// Fail fast before any PLDM traffic is sent so the caller gets a clear reason.
	if (mForceUpdate) {
		for (int i = 0; i < pkg->compImagesInfo.compImageCount; i++) {
			if (fwUpdSkipComp((uint8_t)i)) {
				continue;
			}
			if (!pkg->compImagesInfo.compImages[i].compOptions.bits.bit0) {
				ERR("Force update requested for card {:02} but firmware image '{}' is not "
					"downgradable: ForceUpdate bit is not set in ComponentOptions for component {}.\n",
					mCardNum, pkgFilePath, i);
				return cleanupAndReturn(PLDM_ERROR);
			}
		}
		DBG("Force update requested and all {} component(s) advertise the ForceUpdate capability.\n", fwUpdCompCount());
	}

	for (int i = 0; i < pkg->compImagesInfo.compImageCount; i++) {
		if (fwUpdSkipComp((uint8_t)i)) {
			DBG("Component Number = {} skipped: identifier 0x{:04X} does not match the requested 0x{:04X}\n", i + 1,
				pkg->compImagesInfo.compImages[i].id, mCompIdFilter);
			continue;
		}
		DBG("Component Number = {}\n", i + 1);
		DBG("Offset = {} & Size = {}\n", pkg->compImagesInfo.compImages[i].compLocOffset,
			pkg->compImagesInfo.compImages[i].compSize);
	}

	// Components of the package this update covers, in package order
	std::vector<uint8_t> comps;
	for (uint8_t n = 0; n < pkg->compImagesInfo.compImageCount; n++) {
		if (!fwUpdSkipComp(n)) {
			comps.push_back(n);
		}
	}
	if (comps.empty()) {
		ERR("Firmware package '{}' contains no component to update.\n", pkgFilePath);
		return cleanupAndReturn(PLDM_ERROR);
	}

	// One update session covers every component (DSP0267 section 7.4): inventory, RequestUpdate once,
	// PassComponentTable for each component, then UpdateComponent and the transfer for each in turn.
	DBG("\n========= Firmware Update Inventory =========\n");
	for (const auto &inv : inventoryCmds) {
		if (AmcUpdateInterruptGuard::cancelRequested()) {
			return cleanupAndReturn(PLDM_ERROR);
		}
		DBG(">>> Send {}\n", inv.name.c_str());
		if (fwUpdCmd((uint8_t)inv.cmd) != PLDM_SUCCESS) {
			ERR("FWU : {} Failed!!! \n", inv.name.c_str());
			return cleanupAndReturn(PLDM_ERROR);
		}
		DBG("<<< {} Success...\n", inv.name.c_str());
	}

	if (AmcUpdateInterruptGuard::cancelRequested()) {
		return cleanupAndReturn(PLDM_ERROR);
	}
	mCurComp = comps.front();
	DBG(">>> Send RequestUpdate\n");
	ret = fwUpdCmd(REQUEST_UPDATE);
	// If RequestUpdate fails because the device is already in update
	// mode (0x81), it is carrying a stale session from a previous
	// attempt. Cancel that session and retry RequestUpdate once - the
	// same recovery amctool performs.
	if (ret != PLDM_SUCCESS && mLastFwuCompletionCode == ALREADY_IN_UPDATE_MODE) {
		DBG("FWU : RequestUpdate reported ALREADY_IN_UPDATE_MODE; cancelling stale session and retrying\n");
		(void)fwUpdCancel();
		ret = fwUpdCmd(REQUEST_UPDATE);
	}
	if (ret != PLDM_SUCCESS) {
		ERR("FWU : RequestUpdate Failed!!! \n");
		return cleanupAndReturn(ret);
	}
	DBG("<<< RequestUpdate Success...\n");
	// The device is now in update mode and must receive CancelUpdate on any early return from here on.
	updateStarted = true;

	// DSP0267 section 12.4: one PassComponentTable per component in ascending order of index, the
	// TransferFlag marking the start, middle and end of the table.
	for (size_t k = 0; k < comps.size(); k++) {
		if (AmcUpdateInterruptGuard::cancelRequested()) {
			return cleanupAndReturn(PLDM_ERROR);
		}
		mCurComp = comps[k];
		if (comps.size() == 1) {
			mPassCompTransferFlag = TRANSFERFLAG_STARTANDEND;
		} else if (k == 0) {
			mPassCompTransferFlag = TRANSFERFLAG_START;
		} else if (k + 1 == comps.size()) {
			mPassCompTransferFlag = TRANSFERFLAG_END;
		} else {
			mPassCompTransferFlag = TRANSFERFLAG_MIDDLE;
		}
		DBG(">>> Send PassComponentTable for component {} (TransferFlag 0x{:02x})\n", mCurComp + 1,
			mPassCompTransferFlag);
		if (fwUpdCmd(PASS_COMPONENT_TABLE) != PLDM_SUCCESS) {
			ERR("FWU : PassComponentTable Failed!!! \n");
			return cleanupAndReturn(PLDM_ERROR);
		}
		DBG("<<< PassComponentTable Success...\n");
	}

	// DSP0267 section 7.6 step 5: each component image in the order it is listed in the package
	for (uint8_t n : comps) {
		if (AmcUpdateInterruptGuard::cancelRequested()) {
			return cleanupAndReturn(PLDM_ERROR);
		}
		if (fwUpdComponent(n) != PLDM_SUCCESS) {
			return cleanupAndReturn(PLDM_ERROR);
		}
	}

	if (AmcUpdateInterruptGuard::cancelRequested()) {
		return cleanupAndReturn(PLDM_ERROR);
	}
	DBG(">>> Send GetStatus\n");
	if (fwUpdCmd(GET_STATUS) != PLDM_SUCCESS) {
		ERR("FWU : GetStatus Failed!!!\n");
		return cleanupAndReturn(PLDM_ERROR);
	}
	DBG("<<< GetStatus Success...\n");

	DBG("\n========= Activate Firmware for all Components =========\n");
	// Last point at which the update can be cancelled: once ActivateFirmware is sent the FD
	// finishes activation even if CancelUpdate follows (DSP0267 section 7.6).
	if (AmcUpdateInterruptGuard::cancelRequested()) {
		return cleanupAndReturn(PLDM_ERROR);
	}
	DBG(">>> Send ActivateFirmware\n");
	if (fwUpdCmd(ACTIVATE_FIRMWARE) != PLDM_SUCCESS) {
		ERR("FWU : Failed!!! Activate Firmware\n");
		return cleanupAndReturn(PLDM_ERROR);
	}
	DBG("<<< ActivateFirmware Success...\n");
	DBG("Firmware Update and Activation completed successfully for card : {:02}\n", mCardNum);

	// ActivateFirmware succeeded (or the device already self-activated) - the
	// update session is finished so no CancelUpdate is required.
	updateStarted = false;
	return cleanupAndReturn(PLDM_SUCCESS);
}

/**
 * @brief Send GET_FIRMWARE_PARAMETERS command and cache the parsed response.
 *
 * Sends PLDM firmware update command GET_FIRMWARE_PARAMETERS (0x02) to the AMC
 * and stores the raw response payload in mFwParamRawData. On success the
 * parsed ActiveComponentImageSetVersionString is stored in mFwParamActiveVersion.
 *
 * The response usually spans several MCTP packets; mctpReceiveMessage() reassembles
 * it and fwUpdateResp() copies the PLDM payload into mFwParamRawData. After
 * fwUpdCmd() returns, this function calls parseFwParamResponse() to decode it.
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 *
 * @note Follows DSP0267 section 11.2.
 * @note On success mFwParamsInitialized is set to true and mFwParamActiveVersion
 *       holds the active component image set version string.
 */
uint8_t pldm::getFirmwareParameters()
{
	TRACING();

	mFwParamRawData.clear();
	mFwParamsInitialized = false;
	mFwParamActiveVersion.clear();

	DBG("\n========= GetFirmwareParameters CARD {} =========\n", mCardNum);
	DBG(">>> Send GetFirmwareParameters\n");

	if (fwUpdCmd(GET_FIRMWARE_PARAMETERS) != PLDM_SUCCESS) {
		ERR("FWU: GET_FIRMWARE_PARAMETERS command failed on card {}\n", mCardNum);
		return PLDM_ERROR;
	}

	DBG("<<< GetFirmwareParameters received {} bytes of payload\n", mFwParamRawData.size());

	return parseFwParamResponse();
}

/**
 * @brief Parse the GET_FIRMWARE_PARAMETERS response buffer per DSP0267 section 11.2.
 *
 * Decodes the PLDM payload accumulated in mFwParamRawData. Walks the
 * ComponentParameterTable (DSP0267 Table 19) and extracts the
 * ActiveComponentVersionString from the first entry whose ComponentClassification
 * is COMP_CLASS_FIRMWARE (0x000A).
 *
 * @return uint8_t PLDM_SUCCESS on success, PLDM_ERROR on failure.
 *
 * @note On success mFwParamsInitialized is set to true and mFwParamActiveVersion
 *       contains the ActiveComponentVersionString of the matched component.
 */
uint8_t pldm::parseFwParamResponse()
{
	TRACING();

	const uint8_t *payload = mFwParamRawData.data();
	const size_t len = mFwParamRawData.size();

	// DSP0267 section 11.2 Table 18 - GET_FIRMWARE_PARAMETERS response layout:
	constexpr size_t kResponseHeaderLen = 11;

	if (len < kResponseHeaderLen) {
		ERR("FWU: GET_FIRMWARE_PARAMETERS response too short ({} bytes, need at least {})\n", len, kResponseHeaderLen);
		return PLDM_ERROR;
	}

	// Byte 0: CompletionCode
	if (payload[0] != PLDM_SUCCESS) {
		ERR("FWU: GET_FIRMWARE_PARAMETERS completion code 0x{:02x}\n", payload[0]);
		return PLDM_ERROR;
	}

	// Bytes 5-6: ComponentCount (little-endian uint16)
	const uint16_t compCount = static_cast<uint16_t>(payload[5]) | (static_cast<uint16_t>(payload[6]) << 8);
	DBG("FWU: ComponentCount = {}\n", compCount);

	if (compCount == 0) {
		ERR("FWU: GET_FIRMWARE_PARAMETERS: no components reported\n");
		return PLDM_ERROR;
	}

	// Bytes 8 and 10: image-set version string lengths — used only to skip to
	// the start of the ComponentParameterTable.
	const uint8_t imgSetActiveLen = payload[8];
	const uint8_t imgSetPendingLen = payload[10];

	// Offset to the first ComponentParameterTable entry
	size_t tableOffset = kResponseHeaderLen + imgSetActiveLen + imgSetPendingLen;

	// DSP0267 Table 19 - ComponentParameterTable entry fixed header size
	constexpr size_t kCompParamEntryFixedLen = 39;

	// Walk each ComponentParameterTable entry (DSP0267 Table 19)
	for (uint16_t i = 0; i < compCount; ++i) {
		if (tableOffset + kCompParamEntryFixedLen > len) {
			ERR("FWU: component parameter table entry {} truncated\n", i);
			return PLDM_ERROR;
		}

		const uint8_t *entry = payload + tableOffset;

		const uint16_t classification = static_cast<uint16_t>(entry[0]) | (static_cast<uint16_t>(entry[1]) << 8);
		const uint16_t identifier = static_cast<uint16_t>(entry[2]) | (static_cast<uint16_t>(entry[3]) << 8);
		const uint8_t activeCompVerStrLen = entry[10];
		const uint8_t pendingCompVerStrLen = entry[24];
		const size_t entrySize = kCompParamEntryFixedLen + activeCompVerStrLen + pendingCompVerStrLen;

		DBG("FWU: component[{}]: classification=0x{:04x} identifier=0x{:04x} "
			"activeCompVerStrLen={}\n",
			i, classification, identifier, activeCompVerStrLen);

		if (tableOffset + entrySize > len) {
			ERR("FWU: component parameter table entry {} version string truncated\n", i);
			return PLDM_ERROR;
		}

		// Match AMC firmware component by classification
		if (classification == COMP_CLASS_FIRMWARE) {
			if (activeCompVerStrLen > 0) {
				mFwParamActiveVersion =
					std::string(reinterpret_cast<const char *>(entry + kCompParamEntryFixedLen), activeCompVerStrLen);
				mFwParamsInitialized = true;
				DBG("FWU: AMC ActiveComponentVersionString (class=0x{:04x}) = \"{}\"\n", classification,
					mFwParamActiveVersion.c_str());
				return PLDM_SUCCESS;
			}
		}

		tableOffset += entrySize;
	}

	ERR("FWU: no firmware component (0x{:04x}) found in "
		"ComponentParameterTable\n",
		COMP_CLASS_FIRMWARE);
	return PLDM_ERROR;
}
