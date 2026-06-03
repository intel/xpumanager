/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "dmi_reader.h"

#include <exception>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <iterator>
#include <stdexcept>
#include <cstring>
#include <format>
#include <span>
#include <array>
#include <filesystem>
#include <ranges>
#include <algorithm>
#include <vector>
#include <utility>
#include <string_view>
#include <string>
#include <tuple>

DmiReader::DmiReader()
{
	try {
		loadDMIData();
		mIsValid = true;
	} catch (const std::exception &e) {
		mIsValid = false;
		throw;
	}
}

bool DmiReader::isValid() const noexcept { return mIsValid; }

std::vector<DmiReader::SlotInfo> DmiReader::getSlots() const
{
	if (!mIsValid) {
		return {};
	}

	std::vector<SlotInfo> slots;

	for (const auto &[offset, header] : getStructureIterator()) {
		if (header->type == 9) { // System Slot
			if (auto slotInfo = parseSlot(offset)) {
				slots.push_back(std::move(*slotInfo));
			}
		}
	}

	return slots;
}

std::optional<DmiReader::SystemInfo> DmiReader::getSystemInfo() const
{
	if (!mIsValid) {
		return std::nullopt;
	}

	for (const auto &[offset, header] : getStructureIterator()) {
		if (header->type == 1) { // System Information
			return parseSystemInfo(offset);
		}
	}

	return std::nullopt;
}

std::optional<DmiReader::SlotInfo> DmiReader::findSlotForDevice(std::string_view deviceBdf) const
{
	if (!mIsValid) {
		return std::nullopt;
	}

	const auto slots = getSlots();

	const auto findSlotByBDF = [&slots](std::string_view targetBdf) -> std::optional<SlotInfo> {
		const auto match = std::ranges::find_if(
			slots, [&targetBdf](const auto &slot) { return slot.busAddress && *slot.busAddress == targetBdf; });

		return (match != slots.end()) ? std::make_optional(*match) : std::nullopt;
	};

	// Direct match first
	if (auto directMatch = findSlotByBDF(deviceBdf)) {
		return directMatch;
	}

	// Parent bridge matching with
	const auto parentBdfs = getParentBridges(deviceBdf);

	for (const auto &parentBdf : parentBdfs) {
		if (auto parentMatch = findSlotByBDF(parentBdf)) {
			return parentMatch;
		}
	}

	return std::nullopt;
}

std::span<const uint8_t> DmiReader::dataSpan() const noexcept { return dmiData; }

std::vector<std::string> DmiReader::getParentBridges(std::string_view deviceBdf)
{
	std::vector<std::string> parents;

	try {
		const std::filesystem::path devicePath = std::format("/sys/bus/pci/devices/{}", deviceBdf);

		if (!std::filesystem::exists(devicePath)) {
			return parents;
		}

		const auto realPath = std::filesystem::canonical(devicePath);

		for (auto parent = realPath.parent_path(); parent != parent.root_path() && parent != parent.parent_path();
			 parent = parent.parent_path()) {

			const std::string parentName = parent.filename().string();

			if (isValidBDF(parentName)) {
				parents.push_back(parentName);
			}
		}
	} catch (const std::filesystem::filesystem_error &) {
		// Return empty vector
	}

	return parents;
}

constexpr bool DmiReader::isValidBDF(std::string_view bdf) noexcept
{
	if (bdf.size() < 7) {
		return false;
	}

	const auto colonPos = bdf.find(':');
	const auto dotPos = bdf.find('.');

	return colonPos != std::string_view::npos && dotPos != std::string_view::npos && dotPos > colonPos + 1 &&
		   colonPos > 0 && dotPos < bdf.size() - 1;
}

void DmiReader::loadDMIData()
{
	std::ifstream dmiFile("/sys/firmware/dmi/tables/DMI", std::ios::binary);
	if (!dmiFile) {
		throw std::runtime_error("Cannot access DMI tables. Ensure you have proper permissions and sysfs support.");
	}

	dmiData = std::vector<uint8_t>(std::istreambuf_iterator<char>(dmiFile), std::istreambuf_iterator<char>());

	if (dmiData.empty()) {
		throw std::runtime_error("DMI table is empty");
	}

	if (dmiData.size() < sizeof(SMBIOSHeader)) {
		throw std::runtime_error("DMI table too small to contain valid data");
	}
}

std::vector<std::pair<size_t, const DmiReader::SMBIOSHeader *>> DmiReader::getStructureIterator() const
{
	std::vector<std::pair<size_t, const SMBIOSHeader *>> structures;

	size_t offset = 0;
	while (offset < dmiData.size()) {
		if (!validateStructure(offset, sizeof(SMBIOSHeader))) {
			break;
		}

		const auto *header = reinterpret_cast<const SMBIOSHeader *>(&dmiData[offset]);
		structures.emplace_back(offset, header);

		offset = findNextStructure(offset);
		if (offset == 0) {
			break;
		}
	}

	return structures;
}

bool DmiReader::validateStructure(size_t offset, size_t minSize) const noexcept
{
	if (offset + sizeof(SMBIOSHeader) > dmiData.size()) {
		return false;
	}

	const auto *header = reinterpret_cast<const SMBIOSHeader *>(&dmiData[offset]);

	return header->length >= sizeof(SMBIOSHeader) && offset + header->length <= dmiData.size() &&
		   header->length >= minSize;
}

std::string DmiReader::getStringFromTable(size_t structureOffset, uint8_t structureLength, uint8_t stringIndex) const
{
	if (stringIndex == 0) {
		return {};
	}

	const size_t stringAreaStart = structureOffset + structureLength;
	if (stringAreaStart >= dmiData.size()) {
		return {};
	}

	const auto stringArea = dataSpan().subspan(stringAreaStart);

	size_t pos = 0;
	for (uint8_t currentString = 1; currentString < stringIndex; ++currentString) {
		// Find next null terminator
		const auto remainingArea = stringArea.subspan(pos);
		const auto nullIt = std::ranges::find(remainingArea, uint8_t{0});

		if (nullIt == remainingArea.end()) {
			return {};
		}

		pos += std::distance(remainingArea.begin(), nullIt) + 1;
		if (pos >= stringArea.size()) {
			return {};
		}
	}

	const auto targetArea = stringArea.subspan(pos);
	const auto stringEnd = std::ranges::find(targetArea, uint8_t{0});
	const size_t stringLength = std::distance(targetArea.begin(), stringEnd);

	constexpr size_t maxStringLength = 256;
	const size_t safeLength = std::min(stringLength, maxStringLength);

	const std::string_view sv(reinterpret_cast<const char *>(targetArea.data()), safeLength);
	return std::string(sv);
}

size_t DmiReader::findNextStructure(size_t currentOffset) const noexcept
{
	if (!validateStructure(currentOffset, sizeof(SMBIOSHeader))) {
		return 0;
	}

	const auto *header = reinterpret_cast<const SMBIOSHeader *>(&dmiData[currentOffset]);
	size_t const offset = currentOffset + header->length;

	// Find end of string section (double null terminator)
	const auto remainingData = dataSpan().subspan(offset);

	for (size_t i = 0; i + 1 < remainingData.size(); ++i) {
		if (remainingData[i] == 0 && remainingData[i + 1] == 0) {
			return offset + i + 2; // Start of next structure
		}
	}

	return 0;
}

std::optional<DmiReader::SlotInfo> DmiReader::parseSlot(size_t offset) const
{
	if (!validateStructure(offset, 12)) {
		return std::nullopt;
	}

	const auto buffer = dataSpan().subspan(offset);
	const auto *header = reinterpret_cast<const SMBIOSHeader *>(buffer.data());

	SlotInfo slot;
	slot.handle = header->handle;

	if (header->length >= 6) {
		const auto [designationIdx, slotType] = [&]() { return std::make_pair(buffer[4], buffer[5]); }();

		slot.designation = getStringFromTable(offset, header->length, designationIdx);
		slot.slotTypeRaw = slotType;
		slot.type = decodeSlotType(slotType);
	}

	if (header->length >= 8) {
		slot.usageRaw = buffer[7];
		slot.usage = decodeSlotUsage(buffer[7]);
	}

	// Extended fields with type-safe reading
	if (header->length >= 17) {
		// Helper to try reading bus address at specific offsets
		const auto tryReadBusAddress = [this,
										buffer](size_t segOff, size_t busOff,
												size_t dfOff) -> std::optional<std::tuple<uint16_t, uint8_t, uint8_t>> {
			const auto seg = readFromBuffer<uint16_t>(buffer, segOff);
			const auto busNum = readFromBuffer<uint8_t>(buffer, busOff);
			const auto df = readFromBuffer<uint8_t>(buffer, dfOff);

			// Valid if bus is present (not 0xFF marker)
			if (busNum != 0xFF) {
				return std::make_tuple(seg, busNum, df);
			}
			return std::nullopt;
		};

		// Try SMBIOS 2.6+ standard offsets: segment@13, bus@15, dev/func@16
		auto result = tryReadBusAddress(13, 15, 16);

		// Fallback to alternative offsets if standard fails: segment@12, bus@14, dev/func@15
		if (!result) {
			result = tryReadBusAddress(12, 14, 15);
		}

		if (result) {
			const auto &[segment, bus, devfunc] = *result;
			slot.busAddress = std::format("{:04x}:{:02x}:{:02x}.{:01x}", segment, bus, devfunc >> 3, devfunc & 0x7);
		}
	}

	return slot;
}

std::optional<DmiReader::SystemInfo> DmiReader::parseSystemInfo(size_t offset) const
{
	if (!validateStructure(offset, 8)) {
		return std::nullopt;
	}

	const auto buffer = dataSpan().subspan(offset);
	const auto *header = reinterpret_cast<const SMBIOSHeader *>(buffer.data());

	SystemInfo info;
	info.handle = header->handle;

	if (header->length >= 8) {
		const auto [mfgIdx, prodIdx, verIdx, serialIdx] = [&]() {
			return std::make_tuple(buffer[4], buffer[5], buffer[6], buffer[7]);
		}();

		info.manufacturer = getStringFromTable(offset, header->length, mfgIdx);
		info.productName = getStringFromTable(offset, header->length, prodIdx);
		info.version = getStringFromTable(offset, header->length, verIdx);
		info.serialNumber = getStringFromTable(offset, header->length, serialIdx);
	}

	if (header->length >= 25) {
		const auto uuidSpan = buffer.subspan(8, 16);

		const auto [part1, part2, part3, part4, part5] = [&]() {
			return std::make_tuple(uuidSpan.subspan(0, 4), uuidSpan.subspan(4, 2), uuidSpan.subspan(6, 2),
								   uuidSpan.subspan(8, 2), uuidSpan.subspan(10, 6));
		}();

		info.uuid = std::format(
			"{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
			part1[0], part1[1], part1[2], part1[3], part2[0], part2[1], part3[0], part3[1], part4[0], part4[1],
			part5[0], part5[1], part5[2], part5[3], part5[4], part5[5]);
	}

	return info;
}

std::string DmiReader::decodeSlotType(uint8_t type)
{
	static constexpr std::array slotTypes{std::pair{uint8_t{0x01}, std::string_view{"Other"}},
										  std::pair{uint8_t{0x02}, std::string_view{"Unknown"}},
										  std::pair{uint8_t{0x03}, std::string_view{"ISA"}},
										  std::pair{uint8_t{0x04}, std::string_view{"MCA"}},
										  std::pair{uint8_t{0x05}, std::string_view{"EISA"}},
										  std::pair{uint8_t{0x06}, std::string_view{"PCI"}},
										  std::pair{uint8_t{0x07}, std::string_view{"PC Card (PCMCIA)"}},
										  std::pair{uint8_t{0x08}, std::string_view{"VL-VESA"}},
										  std::pair{uint8_t{0x09}, std::string_view{"Proprietary"}},
										  std::pair{uint8_t{0x0A}, std::string_view{"Processor Card Slot"}},
										  std::pair{uint8_t{0x0B}, std::string_view{"Proprietary Memory Card Slot"}},
										  std::pair{uint8_t{0x0C}, std::string_view{"I/O Riser Card Slot"}},
										  std::pair{uint8_t{0x0D}, std::string_view{"NuBus"}},
										  std::pair{uint8_t{0x0E}, std::string_view{"PCI - 66MHz Capable"}},
										  std::pair{uint8_t{0x0F}, std::string_view{"AGP"}},
										  std::pair{uint8_t{0x10}, std::string_view{"AGP 2X"}},
										  std::pair{uint8_t{0x11}, std::string_view{"AGP 4X"}},
										  std::pair{uint8_t{0x12}, std::string_view{"PCI-X"}},
										  std::pair{uint8_t{0x13}, std::string_view{"AGP 8X"}},
										  std::pair{uint8_t{0xA5}, std::string_view{"PCI Express"}},
										  std::pair{uint8_t{0xA6}, std::string_view{"PCI Express x1"}},
										  std::pair{uint8_t{0xA7}, std::string_view{"PCI Express x2"}},
										  std::pair{uint8_t{0xA8}, std::string_view{"PCI Express x4"}},
										  std::pair{uint8_t{0xA9}, std::string_view{"PCI Express x8"}},
										  std::pair{uint8_t{0xAA}, std::string_view{"PCI Express x16"}},
										  std::pair{uint8_t{0xAB}, std::string_view{"PCI Express Gen 2"}},
										  std::pair{uint8_t{0xAC}, std::string_view{"PCI Express Gen 2 x1"}},
										  std::pair{uint8_t{0xAD}, std::string_view{"PCI Express Gen 2 x2"}},
										  std::pair{uint8_t{0xAE}, std::string_view{"PCI Express Gen 2 x4"}},
										  std::pair{uint8_t{0xAF}, std::string_view{"PCI Express Gen 2 x8"}},
										  std::pair{uint8_t{0xB0}, std::string_view{"PCI Express Gen 2 x16"}},
										  std::pair{uint8_t{0xB1}, std::string_view{"PCI Express Gen 3"}},
										  std::pair{uint8_t{0xB2}, std::string_view{"PCI Express Gen 3 x1"}},
										  std::pair{uint8_t{0xB3}, std::string_view{"PCI Express Gen 3 x2"}},
										  std::pair{uint8_t{0xB4}, std::string_view{"PCI Express Gen 3 x4"}},
										  std::pair{uint8_t{0xB5}, std::string_view{"PCI Express Gen 3 x8"}},
										  std::pair{uint8_t{0xB6}, std::string_view{"PCI Express Gen 3 x16"}},
										  std::pair{uint8_t{0xB8}, std::string_view{"PCI Express Gen 4 x1"}},
										  std::pair{uint8_t{0xB9}, std::string_view{"PCI Express Gen 4 x2"}},
										  std::pair{uint8_t{0xBA}, std::string_view{"PCI Express Gen 4 x4"}},
										  std::pair{uint8_t{0xBB}, std::string_view{"PCI Express Gen 4 x8"}},
										  std::pair{uint8_t{0xBC}, std::string_view{"PCI Express Gen 4 x16"}},
										  std::pair{uint8_t{0xBD}, std::string_view{"PCI Express Gen 5 x1"}},
										  std::pair{uint8_t{0xBE}, std::string_view{"PCI Express Gen 5 x2"}},
										  std::pair{uint8_t{0xBF}, std::string_view{"PCI Express Gen 5 x4"}},
										  std::pair{uint8_t{0xC0}, std::string_view{"PCI Express Gen 5 x8"}},
										  std::pair{uint8_t{0xC1}, std::string_view{"PCI Express Gen 5 x16"}}};

	const auto *const match =
		std::ranges::find_if(slotTypes, [type](const auto &entry) { return entry.first == type; });

	return (match != slotTypes.end()) ? std::string(match->second) : std::format("Unknown (0x{:x})", type);
}

std::string DmiReader::decodeSlotUsage(uint8_t usage)
{
	static constexpr std::array usageTypes{
		std::pair{uint8_t{0x01}, std::string_view{"Other"}}, std::pair{uint8_t{0x02}, std::string_view{"Unknown"}},
		std::pair{uint8_t{0x03}, std::string_view{"Available"}}, std::pair{uint8_t{0x04}, std::string_view{"In Use"}},
		std::pair{uint8_t{0x05}, std::string_view{"Unavailable"}}};

	const auto *const match =
		std::ranges::find_if(usageTypes, [usage](const auto &entry) { return entry.first == usage; });

	return (match != usageTypes.end()) ? std::string(match->second) : std::format("Reserved (0x{:x})", usage);
}
