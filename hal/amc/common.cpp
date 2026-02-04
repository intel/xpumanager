/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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

	DBG("Firmware update progress: %u%%\n", percent);

	// Store percent in firmwareInfo structure
	if (ctx != nullptr) {
		firmwareInfo *fwInfo = (firmwareInfo *)ctx;
		if (fwInfo->dev != nullptr) {
			SETPROGRESS(fwInfo->deviceIndex, fwInfo->curThread, fwInfo->totalThreads, percent);
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
	if (data == nullptr || len == 0) {
		return 0;
	}
	// Required to use C array and manual LUT for compatibility with MSVC's /analyze
	// warning C28020: The expression '_Param_(1)<256' is not true at this call.: Lines: 68, 69, 70, 71, 72, 71, 72, 71,
	// 74, 69, 70, 71, 74 While it is a false positive when generating the LUT as a std::array at compile-time, using a
	// C array here avoids the warning in this function and has the same result.
	static constexpr uint8_t crc8Lut[256] = {
		0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77,
		0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0, 0xE7, 0xEE, 0xE9,
		0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD, 0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B,
		0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
		0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88,
		0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A, 0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16,
		0x03, 0x04, 0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74,
		0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
		0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E,
		0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44, 0x19, 0x1E, 0x17, 0x10,
		0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34, 0x4E, 0x49, 0x40, 0x47, 0x52, 0x55,
		0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63, 0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
		0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91,
		0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83, 0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF,
		0xFA, 0xFD, 0xF4, 0xF3};
	uint8_t crc = 0x00;
	for (size_t i = 0; i < len; i++) {
		crc = crc8Lut[crc ^ data[i]];
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
