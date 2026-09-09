/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "pch_prod_state.h"

#include <array>
#include <chrono>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

#include <metee.h>

namespace {

// BUP_COMMON MKHI client GUID.
constexpr GUID MKHI_GUID = {
	.l = 0xe2c2afa2U,
	.w1 = 0x3817U,
	.w2 = 0x4d19U,
	.b = {0x9dU, 0x95U, 0x06U, 0xb1U, 0x6bU, 0x58U, 0x8aU, 0x5dU},
};

constexpr int TEE_RETRY_DELAY_MS = 50;
constexpr uint32_t TEE_READ_TIMEOUT_MS = 10U;
constexpr int TEE_RETRY_COUNT = 3;

} // namespace

[[nodiscard]] std::string getPchProdState(const std::string &meiPath)
{
	if (meiPath.empty()) {
		return {};
	}

	static std::mutex meteeMutex;
	std::lock_guard<std::mutex> lock(meteeMutex);

	TEEHANDLE cl{};
	TEESTATUS status = TEE_INTERNAL_ERROR;

	for (int i = 0; i < TEE_RETRY_COUNT; ++i) {
		status = TeeInit(&cl, &MKHI_GUID, meiPath.c_str());
		if (status != TEE_DEVICE_NOT_READY && status != TEE_BUSY) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(TEE_RETRY_DELAY_MS));
	}
	if (status != TEE_SUCCESS) {
		return {};
	}

	for (int i = 0; i < TEE_RETRY_COUNT; ++i) {
		status = TeeConnect(&cl);
		if (status == TEE_SUCCESS) {
			break;
		}
	}
	if (status != TEE_SUCCESS) {
		TeeDisconnect(&cl);
		return {};
	}

	// maxMsgLen isn't set until TeeConnect, so check it here not after TeeInit.
	constexpr size_t reqSize = sizeof(uint32_t);
	if (cl.maxMsgLen < reqSize) {
		TeeDisconnect(&cl);
		return {};
	}

	std::vector<uint8_t> buf(cl.maxMsgLen, 0U);

	const uint32_t reqHeader =
		static_cast<uint32_t>(mkhi_pch::GROUP_ID) | (static_cast<uint32_t>(mkhi_pch::CMD) << mkhi_pch::CMD_SHIFT);
	const auto reqBytes = std::bit_cast<std::array<uint8_t, reqSize>>(reqHeader);
	std::ranges::copy(reqBytes, buf.begin());

	size_t writeLen = 0;
	status = TeeWrite(&cl, buf.data(), reqSize, &writeLen, 0);
	if (status != TEE_SUCCESS || writeLen != reqSize) {
		TeeDisconnect(&cl);
		return {};
	}

	size_t readLen = 0;
	status = TeeRead(&cl, buf.data(), buf.size(), &readLen, TEE_READ_TIMEOUT_MS);
	TeeDisconnect(&cl);

	if (status != TEE_SUCCESS) {
		return {};
	}

	const auto stateOpt = parsePchResponse(std::span<const uint8_t>{buf}.first(readLen));
	if (!stateOpt) {
		return {};
	}
	return std::string{pchProdStateName(*stateOpt)};
}
