/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __PLDM_H
#define __PLDM_H

#include "mctp.h"
#include "pldm_constants.h"
#include "pldm_file_transfer.h"
#include "pldm_fru.h"
#include "pldm_fwpackage.h"
#include "pldm_fwupdate.h"
#include "pldm_platform.h"
#include "pldm_pdr_manager.h"
#include "pldm_amc_gpu_reset.h"
#include <i2c_interface.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// Transfer Operation Flags
enum pldmTransferOpFlag
{
	PLDM_GET_FIRSTPART = 0x01,
	PLDM_GET_NEXTPART = 0x00
};

enum pldmFileTransferOpFlag
{
	PLDM_XFER_FIRST_PART = 0x00,
	PLDM_XFER_NEXT_PART = 0x01,
	PLDM_XFER_ABORT = 0x02,
	PLDM_XFER_COMPLETE = 0x03,
	PLDM_XFER_CURRENT_PART = 0x04
};

// Transfer Flags
enum pldmTransferFlag
{
	PLDM_START = 0x01,
	PLDM_MIDDLE = 0x02,
	PLDM_END = 0x04,
	PLDM_START_AND_END = 0x05,
	PLDM_ACKNOWLEDGE_COMPLETION = 0x08
};

#pragma pack(push, 1)
struct pldmHdr
{
	// Byte1
	uint8_t instanceID : 5;
	uint8_t reserved : 1;
	uint8_t datagram : 1;
	uint8_t request : 1;

	// Byte2
	uint8_t cmdType : 6;
	uint8_t headerVer : 2;

	// Byte3
	uint8_t cmdCode;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pldmVersionInfo
{
	uint8_t type;
	uint8_t major;
	uint8_t minor;
	uint8_t update;
	uint8_t alpha;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct pldmResponseData
{
	uint8_t supportedTypes;
	uint8_t totalSupportedTypes;
	uint8_t tid;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fwuRequestUpdate
{
	uint32_t maxTransferSize;
	uint16_t numComp;
	uint8_t maxOutstandXferReqs;
	uint16_t pkgDataLen;
	uint8_t compImgSetVerStrType;
	uint8_t compImgSetVerStrLen;
	uint8_t compImgSetVerStr[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fwuPassCompTable
{
	uint8_t xferFlag;
	uint16_t compClass;
	uint16_t id;
	uint8_t compClassIdx;
	uint32_t comparisonStamp;
	uint8_t verStrType;
	uint8_t verStrLen;
	uint8_t verStr[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct fwUpdComp
{
	uint16_t compClass;
	uint16_t id;
	uint8_t compClassIdx;
	uint32_t comparisonStamp;
	uint32_t componentimagesize;
	uint32_t updateoptionflags;
	uint8_t verStrType;
	uint8_t verStrLen;
	uint8_t verStr[PLDM_FWU_COMP_VER_STR_SIZE_MAX];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct compParseData
{
	uint32_t locOffset;
	uint32_t size;
};
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct i2cdata
{
	struct mctpSmbusI2cHdr mctpSmbusHdr;
	struct pldmHdr pldmHdr;
	uint8_t respPayload[128];
} i2cdataPldmInfo;
#pragma pack(pop)

class pldm : public mctp
{
private:
	PdrManager mPdrManager;
	struct pldmResponseData mPldmRespInfo = {};
	struct pldmVersionInfo versionInfo[8] = {};
	i2cdataPldmInfo *mI2cPldmRead, *mI2cPldmWrite;
	std::mutex *progMutex;

	uint8_t instanceID;
	int mCardNum;

	// pldm Firmware Update datastructures
	struct fwuRequestUpdate mReqUpdate = {};
	struct fwuPassCompTable mPassCompTable = {};
	struct fwUpdComp mUpdComp = {};
	uint8_t mFwuCurrentState = 0;
	bool mForceUpdate; // The current update was requested as forced (downgrade-capable)
	uint8_t mLastFwuCompletionCode;
	uint16_t mCompIdFilter;				// Only transfer the component with this ComponentIdentifier, 0 for all of them
	std::vector<uint8_t> mFwuTxPayload; // PLDM payload (after the PLDM header) of the next UA request
	std::vector<uint8_t> mFwuRxPayload; // PLDM payload (after the PLDM header) of the last message received
	mctpMsgInfo mFwuRxInfo = {};		// MCTP header fields of that message, used to respond to FD requests
	uint8_t mPassCompTransferFlag = TRANSFERFLAG_STARTANDEND; // TransferFlag of the next PassComponentTable
	uint16_t mFwuEstTimeBeforeReqFwDataSec = 0; // EstimatedTimeBeforeSendingRequestFirmwareData, in seconds
	// The last TransferComplete / VerifyComplete / ApplyComplete answered, kept across components so a
	// retry of it is answered again wherever it arrives (see fwUpdReplayIfRetry())
	struct fwuAnsweredRequest
	{
		bool valid;
		uint8_t cmd;
		uint8_t instanceId;
		uint8_t completionCode;
		std::chrono::steady_clock::time_point time;
	} mFwuLastAnswered = {};

	// PLDM FRU datastructures
	struct fruGetTableRequest mFruTableRequest = {};
	struct fruTableResponse mFruTableResponse = {};
	struct fruTableMetadata mFruMetadata = {};
	struct fruTable mFruTable = {};
	uint16_t mFruCurrentDataLength = 0;
	bool mFruTableInitialized;

	// PLDM Platform datastructures
	struct pdrRepositoryInfoResp pfPdrRepoInfo = {};
	struct pdrReqPayload pfPdrReq = {};
	struct pdrRespPayload pfPdrResp = {};
	struct pldmGetSensorReadingReq pfSensorReadingReq = {};
	struct pldmGetSensorReadingResp pfSensorReadingResp = {};
	sensorReadingValue mSensorReading = {};
	std::vector<pldmSensorInfo> mSensorInfoList;

	// GetStateEffecterStates request/response (DSP0248 s22.6)
	struct pldmGetStateEffecterStatesReq mStateEffecterReq = {};
	struct pldmGetStateEffecterStatesResp mStateEffecterResp = {};

	// PLDM File Transfer datastructures
	struct pldm_file_df_open_req mDfOpenReq = {};
	struct pldm_file_df_open_resp mDfOpenResp = {};
	struct pldm_file_df_close_req mDfCloseReq = {};
	struct pldm_file_df_close_resp mDfCloseResp = {};
	struct pldm_file_df_heartbeat_req mDfHeartbeatReq = {};
	struct pldm_file_df_heartbeat_resp mDfHeartbeatResp = {};
	struct pldm_file_df_read_req mDfReadReq = {};
	struct pldm_file_df_read_resp mDfReadResp = {};
	struct pldm_base_multipart_receive_req mMultipartReceiveReq = {};
	struct pldm_base_multipart_receive_resp mMultipartReceiveResp = {};
	struct pldm_negotiate_transfer_parameters_req mNegotiateTransferReq = {};
	struct pldm_negotiate_transfer_parameters_resp mNegotiateTransferResp = {};
	uint16_t mNegotiatedPartSize = 0;
	std::vector<uint8_t> mMultipartReceiveData;
	std::vector<uint8_t> mRxAssembledFrame;
	std::vector<uint8_t> mRxAssembledPayload;

	// pldm Base APIs
	int pldminit();
	void cleanup();
	uint8_t receivePldmMessage();
	uint8_t pldmHdrConstruction(struct pldmHdr *pldmHdr, uint8_t instanceID, uint8_t cmdType, uint8_t cmd,
								uint8_t async, uint8_t reqresp);
	[[nodiscard]] bool isResponseTo(uint8_t cmdType, uint8_t cmd, uint8_t id) const;
	static std::vector<uint8_t> pldmBuildMessage(const struct pldmHdr &hdr, const uint8_t *payload, size_t len);

	//============== pldm Discovery ================
	// pldm Discovery Command
	uint8_t pldmDiscInitialize();
	uint8_t discoveryCmd(uint8_t cmd, uint8_t size, uint8_t pldmVersionType);
	uint8_t pldmDiscoveryFillPayload(uint8_t cmd, uint8_t pldmCmdLen);
	void pldmDiscoveryGetVersionPayload(uint8_t msg_type, uint8_t pldmCmdLen);
	void pldmDiscoveryGetCmdPayload(uint8_t msg_type, uint8_t pldmCmdLen);

	// pldm Discovery Response
	uint8_t discoveryResponse(uint8_t cmd, uint8_t id, uint8_t pldmVersionType);
	uint8_t discoveryResponsePayload(uint8_t cmd, uint8_t id);
	uint8_t getbitposition(uint8_t num);

	//============== pldm Firmware Update ===========
	// pldm FwPackage Parse
	FILE *mCompFp = nullptr;
	std::optional<fwPkg> pkg;
	struct compParseData *mCompParseData = nullptr;
	uint8_t mCurComp = 0;

	uint8_t fwpkgParseInfo(const char *pkgFilePath);
	uint8_t dumpPldmFwpkgInfo();

	// pldm FwUpdate Command
	uint8_t fwUpdInitialize(const char *pkgFilePath);
	bool fwUpdSkipComp(uint8_t comp) const;
	uint16_t fwUpdCompCount() const;
	uint8_t fwUpdCmd(uint8_t cmd);
	uint8_t fwUpdNextInstanceId();
	uint8_t pldmFwUpdFillPayload(uint8_t cmd);
	uint8_t fwUpdStoreRxMessage(const std::vector<uint8_t> &msg, const mctpMsgInfo &info);
	uint8_t fwUpdSendResponse(uint8_t completionCode, const std::vector<uint8_t> &data);
	void fwUpdRememberAnswer(uint8_t completionCode);
	bool fwUpdReplayIfRetry();
	void fwUpdAnswerRequestWhileCancelling();
	uint8_t fwUpdSendCancel(uint8_t cmd);
	uint8_t fwUpdCancel();
	uint8_t fwUpdCancelComponent();
	uint8_t fwUpdComponent(uint8_t comp);
	uint8_t fwUpdWaitForReadyXfer();
	uint8_t fwUpdUpdateComponentResp();

	// pldm FwUpdate Command Helper
	uint8_t fwReqUpdate();
	uint8_t fwupdPassCompTable();
	uint8_t fwUpdComp();

	// pldm FwUpdate Response
	uint8_t fwUpdateResp(uint8_t cmd, uint8_t id);
	uint8_t pldmFwUpdateRespPayload(uint8_t cmd, uint8_t id);

	// pldm GetFirmwareParameters (DSP0267 section 11.2)
	uint8_t getFirmwareParameters();
	uint8_t parseFwParamResponse();

	// Cached results from GetFirmwareParameters
	std::vector<uint8_t> mFwParamRawData; // PLDM payload of the GetFirmwareParameters response
	std::string mFwParamActiveVersion;	  // ActiveComponentImageSetVersionString
	bool mFwParamsInitialized;
	struct fwuStageState;
	fwuTransferResult readFromFD();
	void fwUpdReportStageTimeout(const fwuStageState &st);
	bool fwUpdSendGetStatus(fwuStageState &st);
	bool fwUpdPollStatus(fwuStageState &st, std::chrono::steady_clock::time_point now);
	std::optional<fwuTransferResult> fwUpdHandleStatusResponse(fwuStageState &st, const std::vector<uint8_t> &msg,
															   const mctpMsgInfo &info);
	std::optional<fwuTransferResult> fwUpdHandleFdRequest(fwuStageState &st, const std::vector<uint8_t> &msg,
														  const mctpMsgInfo &info);
	std::optional<fwuTransferResult> fwUpdServeFirmwareData();
	std::optional<fwuTransferResult> fwUpdHandleStageResult(fwuStageState &st, uint8_t cmd);
	uint8_t writeToAMCCompXfer(uint32_t offset, uint32_t lenToSend, uint8_t completioncode);
	uint8_t respondtoamc(uint8_t completioncode);

	// pldm OEM VRSync API's
	uint8_t oemVrsyncResp(uint8_t cmd, uint8_t id);
	uint8_t oemVrsyncRespPayload(uint8_t cmd, uint8_t id);

	//============== PLDM FRU ====================
	// PLDM FRU Command
	uint8_t pldmFruInitialize();
	uint8_t pldmFruCommand(uint8_t cmd, uint8_t size);
	uint8_t pldmFruFillPayload(uint8_t cmd, uint8_t size);
	uint8_t getFruRecordTableMetadata();
	uint8_t getFruRecordTable(uint32_t dataTransferHandle, uint8_t transferOperationFlag);

	// PLDM FRU Response
	uint8_t pldmFruResponse(uint8_t cmd, uint8_t id);
	uint8_t pldmFruResponsePayload(uint8_t cmd, uint8_t id);

	// PLDM FRU Helper
	uint8_t ensureFruInitialized();
	bool parseTimestamp104(const uint8_t *timestampData, uint8_t fieldLength, timestamp104_t *timestamp);
	uint8_t parseFruTable(const uint8_t *fruData, size_t dataLength);
	uint8_t parseFruField(const uint8_t *fieldData, uint8_t fieldType, uint8_t recordType, uint8_t encodingType,
						  uint8_t fieldLength);
	void clearFruTableData();

	// PLDM Platform APIs
	uint8_t pfMonCtrlInitialize();
	uint8_t pfMonCtrlCmd(uint8_t cmd, uint8_t size);
	uint8_t processPlatformResponse(uint8_t cmd, uint8_t id);
	uint8_t pfGetPdrRepositoryInfo();
	uint8_t pfFillPayload(uint8_t cmd, uint8_t size);
	uint8_t pfPdrRepoRespPayload();
	uint8_t pfPdrRespPayload();
	uint8_t pfSensorReadingRespPayload();
	uint8_t pfGetTotalPdrs();
	uint8_t pfGetSensorValuesByUnit(sensorUnits unit);
	uint8_t pfGetSensorValuesById(uint16_t sensorId);
	uint8_t pfGetSensorValue(const pldmNumericSensorValuePdr *sensor);
	uint8_t pfStateEffecterRespPayload();
	uint8_t pfGetStateEffecterStates(uint16_t effecterId);
	bool isFirmwareUpdateEnabled();
	void pfBuildPdrSensorCache() { mPdrManager.buildSensorCache(); }

	// PLDM File Transfer APIs
	uint8_t pldmFileTransferCmd(uint8_t cmd, uint8_t size);
	uint8_t PldmFillFileTxPayload(uint8_t cmd, uint8_t size);
	uint8_t pldmFileTransferResp(uint8_t cmd, uint8_t id);
	uint8_t pldmFileTransferRespPayload(const uint8_t *respPayload, size_t respPayloadLen, const uint8_t cmd,
										const uint8_t id);
	uint8_t handleDfOpenResp(const uint8_t *respPayload, size_t respPayloadLen);
	uint8_t handleDfCloseResp(const uint8_t *respPayload, size_t respPayloadLen);
	uint8_t handleDfHeartbeatResp(const uint8_t *respPayload, size_t respPayloadLen);
	uint8_t handleDfReadResp(const uint8_t *respPayload, size_t respPayloadLen, size_t totalFrameSize);
	uint8_t handleMultipartReceiveResp(const uint8_t *respPayload, size_t respPayloadLen, size_t totalFrameSize);
	uint8_t handleNegotiateTransferParamsResp(const uint8_t *respPayload, size_t respPayloadLen);
	uint8_t pldmNegotiateTransferParamsCommand();
	uint8_t pldmDfOpenCommand(uint16_t fileIdentifier, uint16_t &fileDescriptor);
	uint8_t pldmDfCloseCommand(uint16_t fileDescriptor);
	uint8_t pldmDfReadCompleteSection(uint16_t fileDescriptor);
	uint8_t pldmDfReadSection(uint16_t fileDescriptor, uint32_t fileOffset, uint32_t sectionLength,
							  std::vector<uint8_t> &fileData, uint32_t &sectionBytesRead);
	uint8_t pldmMultipartReceiveCommand(uint16_t fileDescriptor, uint32_t maxFileSize, std::vector<uint8_t> &fileData);
	const PdrRecord *getFilePdrById(uint16_t filePdrId);

public:
	pldm(const std::string &devpath, int cardnum)
		: mctp(devpath), mI2cPldmRead(nullptr), mI2cPldmWrite(nullptr), progMutex(nullptr), instanceID(1),
		  mCardNum(cardnum), mForceUpdate(false), mLastFwuCompletionCode(PLDM_SUCCESS), mCompIdFilter(0),
		  mFruTableInitialized(false), mFwParamsInitialized(false)
	{
		pldminit();
	}
	uint8_t getFruSerialNum(char *serialNumber, size_t &bufferSize);
	uint8_t getFruPartNum(char *partNumber, size_t &bufferSize);
	uint8_t getFruTdp(char *tdp, size_t &bufferSize);
	uint8_t getAmcVersion(char *version, size_t *bufferSize);
	~pldm() { cleanup(); }

	// pldm Base APIs
	int initialize();
	int fwupd(const char *pkgFilePath, uint16_t compIdFilter = 0, bool forceUpdate = false);
	uint8_t getSensorInfoById(uint16_t sensorId);
	uint8_t getSensorInfoByUnit(sensorUnits unit);
	uint8_t getFile(uint16_t filePdrId, std::vector<uint8_t> &fileData);
	std::vector<pldmSensorInfo> &getSensorInfoList() { return mSensorInfoList; }
	uint8_t oemVrsyncCmd(uint8_t cmd);
	[[nodiscard]] int fwupdProgress() const { return mProgPercent.load(); }
	// The AMC bus is locked only for the duration of one amclib operation, so a long-running
	// process does not keep every other process away from the card.
	[[nodiscard]] bool isI2cOpen() const { return i2cobj != nullptr && i2cobj->isInit(); }
	[[nodiscard]] bool tryLockBus() { return i2cobj != nullptr && i2cobj->tryLock(); }
	void unlockBus()
	{
		if (i2cobj != nullptr) {
			i2cobj->unlock();
		}
	}
	// True when the last tryLockBus() failed because the AMC's I2C device is held by another process
	bool isI2cBusy() { return i2cobj != nullptr && i2cobj->isBusy(); }
	// Set while a wait for the bus has timed out and the bus has not been acquired since, so
	// later calls fail fast instead of each waiting again
	bool mBusWaitExpired = false;
	// pldminit() sets this, but the constructor ignores pldminit()'s failure return,
	// so a caller polling fwupdProgress() after a failed init must not read garbage.
	// Written by the flash thread while amclib::amcFirmwareProgress() reads it from another one.
	std::atomic<int> mProgPercent{0};
	uint8_t amcGpuReset();

	// Closes the card's AMC I2C handle. The I2C adapter belongs to the GPU, so it is torn
	// down when the GPU leaves the bus; the handle has to be given up before that happens
	// rather than left pointing at an adapter the kernel has removed. Safe to call twice.
	// Nothing can talk to the AMC afterwards until the card is enumerated again.
	bool closeAmcInterface() { return i2cobj == nullptr ? true : i2cobj->closeAmc(); }
};

#endif // __PLDM_H
