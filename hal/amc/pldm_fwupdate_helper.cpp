/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "common.h"
#include "pldm_fwupdate.h"
#include "amc_update_interrupt.h"
#include "pldm.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

static_assert(PLDM_FWU_COMP_VER_STR_SIZE_MAX >= 256, "version string buffer must exceed max uint8_t length");
// DSP0267 Table 27: MaximumTransferSize shall be >= the firmware update baseline transfer size.
static_assert(PLDM_MCTP_MAX_TRANSFER_SIZE >= PLDM_FWU_BASELINE_TRANSFER_SIZE,
			  "MaximumTransferSize must be >= the DSP0267 baseline transfer size");
// The RequestFirmwareData response (message type, PLDM header, completion code, data) must be sendable.
static_assert(1 + PLDM_HEADER_SIZE + PLDM_CMD_RESP_PAYLOAD_SIZE + PLDM_MCTP_MAX_TRANSFER_SIZE <=
				  MCTP_MAX_ASSEMBLED_MSG_SIZE,
			  "RequestFirmwareData response exceeds the MCTP message limit");

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
		DBG("{} TransferComplete \n", str);
	} else if (cmd == VERIFY_COMPLETE) {
		DBG("{} VerifyComplete \n", str);
	} else if (cmd == APPLY_COMPLETE) {
		DBG("{} ApplyComplete \n", str);
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
	mReqUpdate.numComp = fwUpdCompCount();
	mReqUpdate.maxOutstandXferReqs = FWU_MAXIMUM_OUTSTANDING_TRANSFER_REQ;
	mReqUpdate.pkgDataLen = FWU_PACKAGE_DATALENGTH;
	mReqUpdate.compImgSetVerStrType = pkg->compImagesInfo.compImages[mCurComp].verStrType;
	mReqUpdate.compImgSetVerStrLen = pkg->compImagesInfo.compImages[mCurComp].verStrLen;
	memcpy(mReqUpdate.compImgSetVerStr, pkg->compImagesInfo.compImages[mCurComp].verStr,
		   mReqUpdate.compImgSetVerStrLen);

	const size_t len =
		sizeof(struct fwuRequestUpdate) - sizeof(mReqUpdate.compImgSetVerStr) + mReqUpdate.compImgSetVerStrLen;
	const uint8_t *req = reinterpret_cast<const uint8_t *>(&mReqUpdate);
	mFwuTxPayload.assign(req, req + len);

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

	// Start / Middle / End / StartAndEnd, set by fwUpdInitialize() for this component's place in the table
	mPassCompTable.xferFlag = mPassCompTransferFlag;
	mPassCompTable.compClass = pkg->compImagesInfo.compImages[mCurComp].compClassification;
	mPassCompTable.id = pkg->compImagesInfo.compImages[mCurComp].id;

	// In case of multiple components, get this value from GetfirmwareParameters
	// As per spec DSP0267_1.3.0 setting the value to "0x00" for single FD
	mPassCompTable.compClassIdx = COMPONENTCLASSIFICATIONINDEX;
	mPassCompTable.comparisonStamp = pkg->compImagesInfo.compImages[mCurComp].compComparisionStamp;
	mPassCompTable.verStrType = pkg->compImagesInfo.compImages[mCurComp].verStrType;
	mPassCompTable.verStrLen = pkg->compImagesInfo.compImages[mCurComp].verStrLen;
	memcpy(mPassCompTable.verStr, pkg->compImagesInfo.compImages[mCurComp].verStr, mPassCompTable.verStrLen);

	const size_t len = sizeof(struct fwuPassCompTable) - sizeof(mPassCompTable.verStr) + mPassCompTable.verStrLen;
	const uint8_t *req = reinterpret_cast<const uint8_t *>(&mPassCompTable);
	mFwuTxPayload.assign(req, req + len);

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
	// ComponentOptions field of the package header. Force is only asserted when the user
	// explicitly requested it via --force AND the input firmware package advertises the capability. The
	// upfront check in fwUpdInitialize() guarantees the second condition when
	// mForceUpdate is true, so a simple ternary is safe here.
	mUpdComp.updateoptionflags = mForceUpdate ? FWU_FORCEUPDATE : 0;
	mUpdComp.verStrType = pkg->compImagesInfo.compImages[mCurComp].verStrType;
	mUpdComp.verStrLen = pkg->compImagesInfo.compImages[mCurComp].verStrLen;
	memcpy(mUpdComp.verStr, pkg->compImagesInfo.compImages[mCurComp].verStr, mUpdComp.verStrLen);

	const size_t len = sizeof(struct fwUpdComp) - sizeof(mUpdComp.verStr) + mUpdComp.verStrLen;
	const uint8_t *req = reinterpret_cast<const uint8_t *>(&mUpdComp);
	mFwuTxPayload.assign(req, req + len);

	return PLDM_SUCCESS;
}

/**
 * @brief Name of the stage-ending request whose result byte is being reported
 */
static const char *fdResultName(uint8_t cmd)
{
	switch (cmd) {
	case TRANSFER_COMPLETE:
		return "TransferResult";
	case VERIFY_COMPLETE:
		return "VerifyResult";
	default:
		return "ApplyResult";
	}
}

using fwuClock = std::chrono::steady_clock;

/**
 * @brief Where one component update stands while the FD drives it (DSP0267 section 7.6)
 *
 * Shared by readFromFD() and the functions it hands each step to. Holds the stage the FD is in,
 * the time of its last command, and the GetStatus polling done while it verifies and applies.
 */
struct pldm::fwuStageState
{
	// The FD's next expected command
	enum class stage
	{
		Download, // RequestFirmwareData until TransferComplete
		Verify,	  // VerifyComplete
		Apply	  // ApplyComplete
	};

	stage current = stage::Download;
	fwuClock::time_point lastMsgTime{}; // last valid command from the FD

	// GetStatus polling during VERIFY and APPLY (DSP0267 section 12.12)
	bool statusOutstanding = false; // a GetStatus request is waiting for its response
	uint8_t statusInstanceId = 0;
	int statusTries = 0;
	fwuClock::time_point statusSentAt{};
	fwuClock::time_point nextPollAt{};
	bool progressReported = false; // the FD reports a ProgressPercent rather than 0x65
	int lastProgress = -1;		   // last ProgressPercent of the current stage, -1 before the first
	fwuClock::time_point progressChangedAt{};

	/**
	 * @brief Move to the next stage
	 *
	 * Every state transition restarts ProgressPercent at 0 (DSP0267 Table 40), so progress tracking
	 * restarts too, and the new stage is polled straight away.
	 *
	 * @param next Stage the FD has moved to
	 */
	void enterStage(stage next)
	{
		current = next;
		lastProgress = -1;
		progressChangedAt = fwuClock::now();
		nextPollAt = fwuClock::now();
	}

	/**
	 * @brief Name of the current verify or apply stage, as GetStatus calls the FD state
	 * @return const char* "VERIFY" or "APPLY"
	 */
	const char *stageName() const { return current == stage::Verify ? "VERIFY" : "APPLY"; }

	/**
	 * @brief When the current stage times out (DSP0267 Table 2)
	 *
	 * UA_T2 after the last command during the transfer. While the FD verifies and applies, UA_T3 after
	 * the last ProgressPercent change when the FD reports one, UA_T6 after its last command otherwise.
	 *
	 * @return fwuClock::time_point Expiry time
	 */
	fwuClock::time_point expiry() const
	{
		if (current == stage::Download) {
			return lastMsgTime + std::chrono::milliseconds(FWU_UA_T2_MS);
		}
		if (progressReported) {
			return progressChangedAt + std::chrono::milliseconds(FWU_UA_T3_MS);
		}
		return lastMsgTime + std::chrono::milliseconds(FWU_UA_T6_MS);
	}

	/**
	 * @brief When readFromFD() next has something to do without a message from the FD
	 *
	 * The stage expiry, or during VERIFY and APPLY the PT2 limit of the outstanding GetStatus or the
	 * time of the next poll, whichever comes first.
	 *
	 * @return fwuClock::time_point Wake-up time
	 */
	fwuClock::time_point wakeAt() const
	{
		if (current == stage::Download) {
			return expiry();
		}
		const fwuClock::time_point pollEvent =
			statusOutstanding ? statusSentAt + std::chrono::milliseconds(PLDM_PT2_MS) : nextPollAt;
		return std::min(expiry(), pollEvent);
	}

	/**
	 * @brief Whether fwUpdPollStatus() has a GetStatus to send, or a poll to give up, at this time
	 *
	 * Only during VERIFY and APPLY: a new poll once nextPollAt has come, or, for the outstanding one, once
	 * PT2 has passed without its response.
	 *
	 * @param now Current time
	 * @return bool True when fwUpdPollStatus() would act
	 */
	bool pollDue(fwuClock::time_point now) const
	{
		if (current == stage::Download) {
			return false;
		}
		if (statusOutstanding) {
			return now >= statusSentAt + std::chrono::milliseconds(PLDM_PT2_MS);
		}
		return now >= nextPollAt;
	}

	/**
	 * @brief Whether the FD may send this command now (DSP0267 section 7.6)
	 * @param cmd Command code of the FD request
	 * @return bool True when the command is the next step of the sequence
	 */
	bool expects(uint8_t cmd) const
	{
		return (cmd == REQUEST_FIRMWARE_DATA && current == stage::Download) ||
			   (cmd == TRANSFER_COMPLETE && current == stage::Download) ||
			   (cmd == VERIFY_COMPLETE && current == stage::Verify) ||
			   (cmd == APPLY_COMPLETE && current == stage::Apply);
	}

	/**
	 * @brief Take in the ProgressPercent of a GetStatus response for the current stage
	 *
	 * 0x65 means the FD does not report progress, so UA_T6 applies; 0 to 100 starts or keeps UA_T3,
	 * which restarts whenever the value changes.
	 *
	 * @param progress ProgressPercent from GetStatus
	 * @param now Time the response arrived
	 */
	void noteProgress(uint8_t progress, fwuClock::time_point now)
	{
		if (progress == FWU_PROGRESS_NOT_SUPPORTED) {
			progressReported = false;
		} else if (progress <= 100) {
			progressReported = true;
			if (progress != lastProgress) {
				DBG("FWU : {} progress {}%\n", stageName(), progress);
				lastProgress = progress;
				progressChangedAt = now;
			}
		}
	}
};

/**
 * @brief Serve the firmware device through one component update
 *
 * Answers the FD's requests for one component (DSP0267 section 7.6): RequestFirmwareData until the
 * FD sends TransferComplete, then VerifyComplete, then ApplyComplete. Every request is answered,
 * including the ones reporting a failed stage; a command arriving out of that order is answered
 * with COMMAND_NOT_EXPECTED.
 *
 * The wait for the FD's next command is bounded by the DSP0267 Table 2 timers. During the transfer
 * that is UA_T2, which starts only once EstimatedTimeBeforeSendingRequestFirmwareData from the
 * UpdateComponent response has elapsed (at most FWU_MAX_EST_TIME_BEFORE_REQ_FW_DATA_SEC, a local
 * bound). The timers count from the FD's last expected command: a command out of the sequence, or
 * a retry of one already answered, is answered but does not keep them alive. While the FD verifies
 * and applies, GetStatus is polled (section 12.12), each poll only after a read has found no message
 * from the FD waiting: when the FD reports a ProgressPercent, the stage times out once it has not
 * changed for UA_T3; when it reports none (0x65), once no command has come from the FD for UA_T6.
 *
 * @return fwuTransferResult Outcome of the component update
 * @retval fwuTransferResult::Applied ApplyComplete reported success and was acknowledged
 * @retval fwuTransferResult::StageTimeout UA_T2, UA_T3 or UA_T6 expired; the caller shall send
 *         CancelUpdateComponent
 * @retval fwuTransferResult::ComponentFailed TransferComplete, VerifyComplete or ApplyComplete reported a
 *         failure; the caller shall send CancelUpdateComponent (DSP0267 sections 12.7 to 12.9)
 * @retval fwuTransferResult::RestartRequired ApplyResult 0x0B: the FD asks for CancelUpdate and a new transfer
 * @retval fwuTransferResult::Failed Communication failure or interruption
 */
fwuTransferResult pldm::readFromFD()
{
	TRACING();
	fwuStageState st;
	// No Table 2 timer runs before EstimatedTimeBeforeSendingRequestFirmwareData has elapsed (Table 31),
	// capped so a faulty FD cannot hold the update for hours.
	const uint16_t estSec = std::min<uint16_t>(mFwuEstTimeBeforeReqFwDataSec, FWU_MAX_EST_TIME_BEFORE_REQ_FW_DATA_SEC);
	if (estSec < mFwuEstTimeBeforeReqFwDataSec) {
		DBG("FWU : Card {:02}: EstimatedTimeBeforeSendingRequestFirmwareData {} s capped at {} s\n", mCardNum,
			mFwuEstTimeBeforeReqFwDataSec, estSec);
	}
	st.lastMsgTime = fwuClock::now() + std::chrono::seconds(estSec);

	while (true) {
		const fwuClock::time_point now = fwuClock::now();
		if (now >= st.expiry()) {
			fwUpdReportStageTimeout(st);
			return fwuTransferResult::StageTimeout;
		}
		// GetStatus is optional for the UA (DSP0267 section 7.6 step 3), so a poll waits until a read has found no
		// message from the FD pending: one the FD has already queued, such as VerifyComplete, is taken first
		// instead of having the poll's write cross it.
		const bool pollDue = st.pollDue(now);
		const auto untilWake = std::chrono::duration_cast<std::chrono::milliseconds>(st.wakeAt() - now);
		const auto wait = pollDue ? std::chrono::milliseconds(0) : std::max(untilWake, std::chrono::milliseconds(0));
		std::vector<uint8_t> msg;
		mctpMsgInfo info = {};
		// Every request received so far has been answered, so an interrupt can be acted on here:
		// DSP0267 asks the UA to respond to outstanding RequestFirmwareData commands before cancelling.
		const uint8_t rxStatus = mctpReceiveMessage(msg, info, wait, pollDue ? 0 : MCTP_RESPONSE_DELAY_MS,
													[] { return AmcUpdateInterruptGuard::cancelRequested(); });
		if (rxStatus == MCTP_CANCELLED) {
			return fwuTransferResult::Failed;
		}
		if (rxStatus == MCTP_TIMEOUT) {
			// Nothing pending: send the poll that is due. Otherwise a timer or a poll is due, which the top of
			// the loop takes care of.
			if (pollDue && !fwUpdPollStatus(st, fwuClock::now())) {
				return fwuTransferResult::Failed;
			}
			continue;
		}
		if (rxStatus != MCTP_SUCCESS) {
			ERR("FWU : Card {:02}: failed to receive from the firmware device\n", mCardNum);
			return fwuTransferResult::Failed;
		}

		// TO clear: a response, which can only be to the outstanding GetStatus. TO set: a request of the FD's.
		const std::optional<fwuTransferResult> outcome =
			info.tagOwner ? fwUpdHandleFdRequest(st, msg, info) : fwUpdHandleStatusResponse(st, msg, info);
		if (outcome) {
			return *outcome;
		}
	}
}

/**
 * @brief Log which DSP0267 Table 2 timer ended the current stage
 *
 * @param st State of the component update
 */
void pldm::fwUpdReportStageTimeout(const fwuStageState &st)
{
	if (st.current == fwuStageState::stage::Download) {
		ERR("FWU : Card {:02}: no command from the firmware device for {} ms during the transfer (UA_T2)\n", mCardNum,
			FWU_UA_T2_MS);
	} else if (st.progressReported) {
		ERR("FWU : Card {:02}: ProgressPercent stuck at {}% for {} ms in {} (UA_T3)\n", mCardNum, st.lastProgress,
			FWU_UA_T3_MS, st.stageName());
	} else {
		ERR("FWU : Card {:02}: no command from the firmware device for {} ms in {} (UA_T6)\n", mCardNum, FWU_UA_T6_MS,
			st.stageName());
	}
}

/**
 * @brief Send GetStatus for the poll in st
 *
 * Every try of one poll carries the same Instance ID (DSP0240), the one in st.statusInstanceId.
 *
 * @param st State of the component update; statusSentAt is set
 *
 * @return bool True when the request was written
 */
bool pldm::fwUpdSendGetStatus(fwuStageState &st)
{
	struct pldmHdr hdr = {};
	pldmHdrConstruction(&hdr, st.statusInstanceId, PLDM_FIRMWARE_UPDATE, GET_STATUS, PLDM_ASYNC_REQUEST_NOTIFY,
						PLDM_REQUEST);
	st.statusSentAt = fwuClock::now();
	if (mctpSendMessage(mctpRequestInfo(), pldmBuildMessage(hdr, nullptr, 0)) != MCTP_SUCCESS) {
		ERR("FWU : Card {:02}: failed to send GetStatus\n", mCardNum);
		return false;
	}
	return true;
}

/**
 * @brief Poll the FD with GetStatus while it verifies and applies (DSP0267 section 12.12)
 *
 * Starts a poll every FWU_GET_STATUS_POLL_MS. A poll without a response within PT2 is retried with
 * the same Instance ID, PN1 times (DSP0240); one still unanswered after that is given up and the
 * next one is scheduled. readFromFD() calls this only once a read has found no message from the FD
 * pending, so a request the FD has queued is never crossed by the poll.
 *
 * @param st State of the component update
 * @param now Current time
 *
 * @return bool False when a request could not be written
 */
bool pldm::fwUpdPollStatus(fwuStageState &st, fwuClock::time_point now)
{
	if (st.statusOutstanding && now >= st.statusSentAt + std::chrono::milliseconds(PLDM_PT2_MS)) {
		if (st.statusTries <= PLDM_PN1_RETRIES) {
			st.statusTries++;
			if (!fwUpdSendGetStatus(st)) {
				return false;
			}
		} else {
			DBG("FWU : No response to GetStatus after {} tries\n", st.statusTries);
			st.statusOutstanding = false;
			st.nextPollAt = now + std::chrono::milliseconds(FWU_GET_STATUS_POLL_MS);
		}
	}
	if (!st.statusOutstanding && now >= st.nextPollAt) {
		st.statusInstanceId = fwUpdNextInstanceId();
		st.statusOutstanding = true;
		st.statusTries = 1;
		if (!fwUpdSendGetStatus(st)) {
			return false;
		}
	}
	return true;
}

/**
 * @brief Take in a response received while the FD drives the update
 *
 * The only request outstanding is the GetStatus poll, so anything that is not its response is
 * ignored. A response must match its PLDM Type, Command Code and Instance ID (DSP0240) and its
 * message tag (DSP0236).
 *
 * @param st State of the component update
 * @param msg MCTP message body
 * @param info MCTP transport header fields
 *
 * @return std::optional<fwuTransferResult> Failed when the FD reports it has left update mode,
 *         std::nullopt to keep serving it
 */
std::optional<fwuTransferResult> pldm::fwUpdHandleStatusResponse(fwuStageState &st, const std::vector<uint8_t> &msg,
																 const mctpMsgInfo &info)
{
	const fwuClock::time_point now = fwuClock::now();
	const bool isStatus =
		st.statusOutstanding && msg[0] == PLDM_OVER_MCTP && info.msgTag == mctpRequestInfo().msgTag &&
		fwUpdStoreRxMessage(msg, info) == PLDM_SUCCESS && mI2cPldmRead->pldmHdr.request == PLDM_RESPONSE &&
		mI2cPldmRead->pldmHdr.cmdType == PLDM_FIRMWARE_UPDATE && mI2cPldmRead->pldmHdr.cmdCode == GET_STATUS &&
		mI2cPldmRead->pldmHdr.instanceID == st.statusInstanceId;
	if (!isStatus) {
		DBG("FWU : Ignoring MCTP message that is not the response to the outstanding GetStatus\n");
		return std::nullopt;
	}
	st.statusOutstanding = false;
	st.nextPollAt = now + std::chrono::milliseconds(FWU_GET_STATUS_POLL_MS);

	// Table 40: CompletionCode, CurrentState, PreviousState, AuxState, AuxStateStatus, ProgressPercent,
	// ReasonCode, UpdateOptionFlagsEnabled
	if (mFwuRxPayload.size() < 7 || mFwuRxPayload[BYTE_0] != PLDM_SUCCESS) {
		DBG("FWU : GetStatus response unusable ({} bytes, completion code 0x{:02x})\n", mFwuRxPayload.size(),
			mFwuRxPayload.empty() ? 0 : mFwuRxPayload[BYTE_0]);
		return std::nullopt;
	}
	const uint8_t fdState = mFwuRxPayload[BYTE_1];
	mFwuCurrentState = fdState;
	if (fdState == GETSTATUS_IDLE) {
		ERR("FWU : Card {:02}: firmware device left update mode during {} (ReasonCode {})\n", mCardNum, st.stageName(),
			mFwuRxPayload[BYTE_6]);
		return fwuTransferResult::Failed;
	}
	// A response that crossed a stage change describes the previous stage; only the current one counts
	if (fdState == (st.current == fwuStageState::stage::Verify ? GETSTATUS_VERIFY : GETSTATUS_APPLY)) {
		st.noteProgress(mFwuRxPayload[BYTE_5], now);
	}
	return std::nullopt;
}

/**
 * @brief Answer a request from the FD (DSP0267 section 7.6)
 *
 * A command out of the sequence is answered with COMMAND_NOT_EXPECTED, unless it is a retry of the
 * last stage result answered, which gets the same response again (DSP0240).
 *
 * @param st State of the component update; lastMsgTime is updated for an expected command only
 * @param msg MCTP message body
 * @param info MCTP transport header fields
 *
 * @return std::optional<fwuTransferResult> The outcome of the component update when this request
 *         ends it, std::nullopt to keep serving the FD
 */
std::optional<fwuTransferResult> pldm::fwUpdHandleFdRequest(fwuStageState &st, const std::vector<uint8_t> &msg,
															const mctpMsgInfo &info)
{
	// Everything the FD sends here is a request of its own: TO set (DSP0236), Rq set (DSP0240).
	if (msg[0] != PLDM_OVER_MCTP || fwUpdStoreRxMessage(msg, info) != PLDM_SUCCESS ||
		mI2cPldmRead->pldmHdr.request != PLDM_REQUEST) {
		DBG("FWU : Ignoring MCTP message that is not a PLDM request from the firmware device\n");
		return std::nullopt;
	}

	if (mI2cPldmRead->pldmHdr.cmdType != PLDM_FIRMWARE_UPDATE) {
		ERR("FWU : Invalid Command Type {}, expected {}\n", mI2cPldmRead->pldmHdr.cmdType, PLDM_FIRMWARE_UPDATE);
		return fwuTransferResult::Failed;
	}

	const uint8_t cmd = mI2cPldmRead->pldmHdr.cmdCode;

	if (!st.expects(cmd)) {
		// A retry of a stage result already answered, possibly from the previous component, is answered
		// again rather than treated as out of order.
		if (fwUpdReplayIfRetry()) {
			return std::nullopt;
		}
		// DSP0267 COMMAND_NOT_EXPECTED: "received when it is not expected based on the sequence
		// defined to update a firmware component"
		DBG("FWU : Command 0x{:02x} out of sequence, respond AMC with COMMAND_NOT_EXPECTED\n", cmd);
		if (respondtoamc(COMMAND_NOT_EXPECTED) != PLDM_SUCCESS) {
			ERR("FWU : Respond to AMC failed...\n");
			return fwuTransferResult::Failed;
		}
		return std::nullopt;
	}

	// DSP0267 Table 2: UA_T2 runs while no command is received "when it's expected", so only the next step
	// of the sequence restarts the stage timer.
	st.lastMsgTime = fwuClock::now();

	if (cmd == REQUEST_FIRMWARE_DATA) {
		return fwUpdServeFirmwareData();
	}
	return fwUpdHandleStageResult(st, cmd);
}

/**
 * @brief Answer RequestFirmwareData with the component image portion requested (DSP0267 Table 34)
 *
 * Firmware Update Baseline Transfer Size <= Length <= MaximumTransferSize, else
 * INVALID_TRANSFER_LENGTH. The offset shall be within the image and Offset + Length shall not exceed
 * ComponentImageSize + Firmware Update Baseline Transfer Size, else DATA_OUT_OF_RANGE.
 *
 * @return std::optional<fwuTransferResult> Failed when the response could not be sent,
 *         std::nullopt otherwise
 */
std::optional<fwuTransferResult> pldm::fwUpdServeFirmwareData()
{
	const uint32_t compSize = pkg->compImagesInfo.compImages[mCurComp].compSize;

	// DSP0267 Table 34 request: uint32 Offset, uint32 Length
	if (mFwuRxPayload.size() < 2 * sizeof(uint32_t)) {
		ERR("FWU : RequestFirmwareData request too short ({} bytes)\n", mFwuRxPayload.size());
		if (respondtoamc(PLDM_ERROR_INVALID_LENGTH) != PLDM_SUCCESS) {
			ERR("FWU : Respond to AMC failed!!!\n");
			return fwuTransferResult::Failed;
		}
		return std::nullopt;
	}
	const uint32_t offset =
		bytesToInt(mFwuRxPayload[BYTE_3], mFwuRxPayload[BYTE_2], mFwuRxPayload[BYTE_1], mFwuRxPayload[BYTE_0]);
	const uint32_t length =
		bytesToInt(mFwuRxPayload[BYTE_7], mFwuRxPayload[BYTE_6], mFwuRxPayload[BYTE_5], mFwuRxPayload[BYTE_4]);

	mProgPercent = (int)(((uint64_t)offset * 100) / compSize);
	DBG("Card - {:02} :: offset = {}, length = {}, mProgPercent = {}\n", mCardNum, offset, length, mProgPercent.load());

	uint8_t cc = PLDM_SUCCESS;
	if (length < PLDM_FWU_BASELINE_TRANSFER_SIZE || length > PLDM_MCTP_MAX_TRANSFER_SIZE) {
		cc = INVALID_TRANSFER_LENGTH;
	} else if (offset >= compSize || (uint64_t)offset + length > (uint64_t)compSize + PLDM_FWU_BASELINE_TRANSFER_SIZE) {
		cc = DATA_OUT_OF_RANGE;
	}
	if (cc != PLDM_SUCCESS) {
		DBG("FWU : RequestFirmwareData offset {} length {} rejected with 0x{:02x}\n", offset, length, cc);
		if (respondtoamc(cc) != PLDM_SUCCESS) {
			ERR("FWU : Transfer fail, exiting!!!\n");
			return fwuTransferResult::Failed;
		}
		return std::nullopt;
	}
	if (writeToAMCCompXfer(offset, length, PLDM_RESPONSE_COMPLETION) != PLDM_SUCCESS) {
		ERR("FWU : Transfer fail, exiting!!!\n");
		return fwuTransferResult::Failed;
	}
	return std::nullopt;
}

/**
 * @brief Answer TransferComplete, VerifyComplete or ApplyComplete and act on its result
 *
 * The request is acknowledged whatever its result: COMMAND_NOT_EXPECTED is only for commands out of
 * sequence (DSP0267 Tables 35 to 37), and a failure is answered with CancelUpdateComponent by the
 * caller afterwards. A successful result moves to the next stage (section 7.6).
 *
 * @param st State of the component update
 * @param cmd TRANSFER_COMPLETE, VERIFY_COMPLETE or APPLY_COMPLETE
 *
 * @return std::optional<fwuTransferResult> Applied, ComponentFailed, RestartRequired or Failed when
 *         the request ends the component update, std::nullopt when the FD moves to the next stage
 */
std::optional<fwuTransferResult> pldm::fwUpdHandleStageResult(fwuStageState &st, uint8_t cmd)
{
	// The first request byte is the stage result
	printFwuCmdInfo(">>> Received", cmd);
	if (mFwuRxPayload.empty()) {
		ERR("FWU : Request 0x{:02x} carries no result byte\n", cmd);
		if (respondtoamc(PLDM_ERROR_INVALID_LENGTH) != PLDM_SUCCESS) {
			ERR("FWU : Respond to AMC failed!!!\n");
			return fwuTransferResult::Failed;
		}
		return std::nullopt;
	}
	const uint8_t result = mFwuRxPayload[BYTE_0];

	if (respondtoamc(PLDM_RESPONSE_COMPLETION) != PLDM_SUCCESS) {
		ERR("FWU Respond to AMC failed!!!\n");
		return fwuTransferResult::Failed;
	}
	printFwuCmdInfo("<<< Respond Success to AMC for", cmd);
	fwUpdRememberAnswer(PLDM_RESPONSE_COMPLETION);

	if (cmd == TRANSFER_COMPLETE && result == 0x00) {
		mProgPercent = 100;
		st.enterStage(fwuStageState::stage::Verify);
		return std::nullopt;
	}
	if (cmd == VERIFY_COMPLETE && result == 0x00) {
		st.enterStage(fwuStageState::stage::Apply);
		return std::nullopt;
	}
	// ApplyResult 0x01: applied, with a different activation method (Table 37)
	if (cmd == APPLY_COMPLETE && (result == 0x00 || result == 0x01)) {
		if (result == 0x01 && mFwuRxPayload.size() >= 3) {
			DBG("FWU : Component applied; ComponentActivationMethodsModification 0x{:04x}\n",
				mFwuRxPayload[BYTE_1] | (mFwuRxPayload[BYTE_2] << 8));
		}
		return fwuTransferResult::Applied;
	}

	ERR("AMC on card {:02} reported {} 0x{:02x} for component 0x{:04x}\n", mCardNum, fdResultName(cmd), result,
		pkg->compImagesInfo.compImages[mCurComp].id);
	// ApplyResult 0x0B: "the FD/FDP is requesting the UA to use the CancelUpdate command and restart the
	// transfer" (Table 37)
	if (cmd == APPLY_COMPLETE && result == 0x0B) {
		return fwuTransferResult::RestartRequired;
	}
	return fwuTransferResult::ComponentFailed;
}

/**
 * @brief Transfer component data to AMC device
 *
 * Reads firmware component data from the package file at the specified offset
 * and length, then sends the RequestFirmwareData response carrying it. A response
 * larger than the MCTP transmission unit goes out as a multi-packet message.
 *
 * @param offset Byte offset within the component image to start reading from
 * @param lenToSend Number of bytes to read and transfer
 * @param completioncode pldm completion code to include in the response
 *
 * @return uint8_t Status of data transfer
 * @retval PLDM_SUCCESS Data transferred successfully
 * @retval PLDM_ERROR File operation error or transfer failure
 *
 * @note offset must be inside the component image and lenToSend at most PLDM_MCTP_MAX_TRANSFER_SIZE;
 *       readFromFD() answers anything else with an error completion code instead
 * @note Zero-pads requests that extend past the end of the component image
 */
uint8_t pldm::writeToAMCCompXfer(uint32_t offset, uint32_t lenToSend, uint8_t completioncode)
{
	TRACING();
	const uint32_t compStartLoc = pkg->compImagesInfo.compImages[mCurComp].compLocOffset;
	const uint32_t compTotalSize = pkg->compImagesInfo.compImages[mCurComp].compSize;

	if (offset >= compTotalSize || lenToSend > PLDM_MCTP_MAX_TRANSFER_SIZE) {
		ERR("FWU : Request for {} bytes at offset {} is outside the transfer limits\n", lenToSend, offset);
		return PLDM_ERROR;
	}

	// This code path is needed for padding, if the request extends past the end of the image
	// the bytes beyond it are sent as 0x00
	std::vector<uint8_t> data(lenToSend, 0x00);
	const uint32_t imageBytes = std::min(lenToSend, compTotalSize - offset);

	if (fseek(mCompFp, compStartLoc + offset, SEEK_SET) != 0) {
		ERR("FWU: Error seeking in file\n");
		return PLDM_ERROR;
	}
	if (fread(data.data(), 1, imageBytes, mCompFp) != imageBytes) {
		ERR("FWU: Short read of {} bytes at component offset {}\n", imageBytes, offset);
		return PLDM_ERROR;
	}

	DBG("pldm TX  Component transfer:: Sending offset = {}, lenToSend = {}\n", offset, lenToSend);
	if (fwUpdSendResponse(completioncode, data) != PLDM_SUCCESS) {
		return PLDM_ERROR;
	}

	if (compTotalSize == (offset + lenToSend)) {
		DBG("Transfer complete!! RequestFirmwareData Success...\n");
	}

	MSLEEP(FWU_TRANSFER_DELAY_MS);
	return PLDM_SUCCESS;
}

/**
 * @brief Send the response to the firmware device request held in mI2cPldmRead
 *
 * Echoes the request's PLDM instance ID and command code (DSP0240) and answers on the request's
 * MCTP message tag with TO cleared (DSP0236).
 *
 * @param completionCode pldm completion code
 * @param data Response data following the completion code, empty for most commands
 *
 * @return uint8_t PLDM_SUCCESS when the response was sent, PLDM_ERROR otherwise
 */
uint8_t pldm::fwUpdSendResponse(uint8_t completionCode, const std::vector<uint8_t> &data)
{
	TRACING();
	const uint8_t cmd = mI2cPldmRead->pldmHdr.cmdCode;

	// Completion code, then the data. Sized once and filled in place, see pldmBuildMessage().
	std::vector<uint8_t> payload(PLDM_CMD_RESP_PAYLOAD_SIZE + data.size());
	payload[0] = completionCode;
	std::copy(data.begin(), data.end(), payload.begin() + PLDM_CMD_RESP_PAYLOAD_SIZE);

	struct pldmHdr hdr = {};
	pldmHdrConstruction(&hdr, mI2cPldmRead->pldmHdr.instanceID, PLDM_FIRMWARE_UPDATE, cmd, PLDM_ASYNC_REQUEST_NOTIFY,
						PLDM_RESPONSE);
	if (mctpSendMessage(mctpResponseInfo(mFwuRxInfo), pldmBuildMessage(hdr, payload.data(), payload.size())) !=
		MCTP_SUCCESS) {
		ERR("FWU : Failed to send response to cmd 0x{:02x}\n", cmd);
		return PLDM_ERROR;
	}

	return PLDM_SUCCESS;
}

/**
 * @brief Remember the response just given to the FD request held in mI2cPldmRead
 *
 * Kept for TransferComplete, VerifyComplete and ApplyComplete, whose response moves the FD to its
 * next state (DSP0267 section 7.6) and so must reach it even when the first one is lost.
 *
 * @param completionCode Completion code that was sent
 */
void pldm::fwUpdRememberAnswer(uint8_t completionCode)
{
	mFwuLastAnswered = {true, mI2cPldmRead->pldmHdr.cmdCode, mI2cPldmRead->pldmHdr.instanceID, completionCode,
						std::chrono::steady_clock::now()};
}

/**
 * @brief Answer the FD request held in mI2cPldmRead again if it retries the last one answered
 *
 * A requester that misses a response retries the request with the same Instance ID, within PT3 of
 * the first try (DSP0240 Table 6). The FD can do that after the UA has moved on, for example after
 * ApplyComplete while the UA is already sending UpdateComponent for the next component or
 * ActivateFirmware, so every receive path checks for it.
 *
 * @return bool True when the request was such a retry and has been answered again
 */
bool pldm::fwUpdReplayIfRetry()
{
	const struct pldmHdr &req = mI2cPldmRead->pldmHdr;
	if (!mFwuLastAnswered.valid || req.request != PLDM_REQUEST || req.cmdType != PLDM_FIRMWARE_UPDATE ||
		req.cmdCode != mFwuLastAnswered.cmd || req.instanceID != mFwuLastAnswered.instanceId ||
		std::chrono::steady_clock::now() - mFwuLastAnswered.time > std::chrono::milliseconds(PLDM_PT3_MAX_MS)) {
		return false;
	}

	DBG("FWU : Retry of FD command 0x{:02x} (instance {}), answering it again with 0x{:02x}\n", req.cmdCode,
		static_cast<unsigned>(req.instanceID), mFwuLastAnswered.completionCode);
	if (respondtoamc(mFwuLastAnswered.completionCode) != PLDM_SUCCESS) {
		ERR("FWU : Respond to AMC failed while answering a retried request\n");
	}
	return true;
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
 */
uint8_t pldm::respondtoamc(uint8_t completioncode)
{
	TRACING();

	if (fwUpdSendResponse(completioncode, {}) != PLDM_SUCCESS) {
		return PLDM_ERROR;
	}

	MSLEEP(I2C_EVENT_WAIT_PERIOD_MS);
	return PLDM_SUCCESS;
}
