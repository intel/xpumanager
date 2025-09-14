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
#include <array>
#include <fwupd.h>
#include <debug.h>

/**
 * @brief Common progress callback function for firmware update operations
 *
 * This callback function can be used across all firmware update modules (GSC, AMC, etc.)
 * to report progress status. It calculates the completion percentage and updates
 * the firmware information structure with the current progress for monitoring.
 *
 * @param done Number of bytes or units completed in the update process
 * @param total Total number of bytes or units in the update process
 * @param ctx Context pointer to firmwareInfo structure for progress tracking
 */
void commonProgressCallback(uint32_t done, uint32_t total, void *ctx)
{
	if (total == 0) {
		return; // Avoid division by zero
	}

	uint32_t percent = (done * 100) / total;

	DBG("Firmware update progress: %d%%\n", percent);

	// Store percent in firmwareInfo structure
	if (ctx != nullptr) {
		firmwareInfo *fwInfo = (firmwareInfo *)ctx;
		if (fwInfo->dev != nullptr) {
			fwInfo->dev->setProgress(fwInfo->curThread, fwInfo->totalThreads, percent);
		}
	}
}

/**
 * @brief Calculate CRC8 checksum using SMBus polynomial
 *
 * Computes an 8-bit Cyclic Redundancy Check (CRC) using the SMBus polynomial (0x07).
 * This is commonly used for data integrity verification in SMBus/I2C communications.
 *
 * @param data Pointer to the data buffer to calculate CRC for
 * @param len Length of the data buffer in bytes
 *
 * @return uint8_t Calculated CRC8 value
 *
 * @note Uses polynomial 0x07 which is standard for SMBus CRC calculations
 */
uint8_t crc8Smbus(const uint8_t *data, size_t len)
{
	static constexpr auto crc8_lut = []() constexpr -> std::array<uint8_t, 256> {
		std::array<uint8_t, 256> lut{};
		for (int i = 0; i < 256; i++) {
			auto crc = static_cast<uint8_t>(i);
			for (int j = 0; j < 8; j++) {
				crc = (crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1);
			}
			lut[i] = crc;
		}
		return lut;
	}();
	uint8_t crc = 0x00;
	for (size_t i = 0; i < len; i++) {
		crc = crc8_lut[crc ^ data[i]];
	}
	return crc;
}

/**
 * @brief Print hexadecimal dump of a buffer
 *
 * Outputs the contents of a buffer in hexadecimal format for debugging purposes.
 * Each byte is printed as a two-digit hexadecimal value prefixed with "0x".
 *
 * @param buff Array of bytes to dump
 * @param len Length of the buffer in bytes
 *
 * @note Uses DBG for output, so visibility depends on debug settings
 */
void hexdump(uint8_t buff[], unsigned int len)
{
	unsigned int i = 0;
	for (i = 0; i < len; i++) {
		DBG("0x%02x ", buff[i]);
	}
	DBG("\n");
}

/**
 * @brief Convert four bytes to a 32-bit unsigned integer
 *
 * Combines four individual bytes into a single 32-bit unsigned integer using
 * big-endian byte ordering (most significant byte first).
 *
 * @param byte1 Most significant byte (bits 31-24)
 * @param byte2 Second byte (bits 23-16)
 * @param byte3 Third byte (bits 15-8)
 * @param byte4 Least significant byte (bits 7-0)
 *
 * @return unsigned int 32-bit integer formed from the four input bytes
 *
 * @note Uses big-endian byte ordering (network byte order)
 * @note Commonly used for converting multi-byte fields from firmware packages
 * @note Result format: (byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4
 */
unsigned int bytesToInt(uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4)
{
	unsigned int result = 0;
	result |= byte1 << 24;
	result |= byte2 << 16;
	result |= byte3 << 8;
	result |= byte4;
	return result;
}
