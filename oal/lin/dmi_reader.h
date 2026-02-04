/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <concepts>
#include <bit>
#include <utility>
#include <vector>
#include <string>
#include <optional>
#include <cstdint>
#include <string_view>
#include <span>
#include <stdexcept>
#include <cstring>

/**
 * @brief SMBIOS/DMI table reader for extracting system hardware information.
 *
 * Provides access to DMI/SMBIOS tables from /sys/firmware/dmi/tables/DMI on Linux systems.
 * Supports reading system information, PCIe slot details, and mapping devices to physical slots
 * by traversing the PCIe topology.
 *
 * @note Requires read access to /sys/firmware/dmi/tables/DMI (typically requires root or
 *       appropriate permissions via capabilities/groups).
 * @note Thread-safe for read operations after construction completes.
 *
 */
class DmiReader
{
public:
	/**
	 * @brief Information about a physical system slot (PCIe, PCI, etc.).
	 *
	 * Contains both human-readable decoded values and raw SMBIOS field values
	 * for advanced processing.
	 */
	struct SlotInfo
	{
		std::string designation;			   ///< Slot designation/label (e.g., "SLOT1", "PCIE_X16_1")
		std::string type;					   ///< Human-readable slot type (e.g., "PCI Express Gen 4 x16")
		std::string usage;					   ///< Slot usage status (e.g., "In Use", "Available")
		std::optional<std::string> busAddress; ///< PCI Bus address in format "SSSS:BB:DD.F" if available
		uint16_t handle;					   ///< SMBIOS structure handle
		uint8_t slotTypeRaw;				   ///< Raw SMBIOS slot type byte value
		uint8_t usageRaw;					   ///< Raw SMBIOS usage status byte value
	};

	/**
	 * @brief System identification and configuration information.
	 *
	 * Corresponds to SMBIOS Type 1 (System Information) structure.
	 */
	struct SystemInfo
	{
		std::string manufacturer; ///< System manufacturer name
		std::string productName;  ///< System product name/model
		std::string version;	  ///< System version string
		std::string serialNumber; ///< System serial number
		std::string uuid;		  ///< System UUID in format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
		uint16_t handle;		  ///< SMBIOS structure handle
	};

	/**
	 * @brief Constructs a DmiReader and loads DMI tables from sysfs.
	 *
	 * Attempts to read and parse /sys/firmware/dmi/tables/DMI. If initialization
	 * fails, the reader is marked invalid and subsequent queries return empty results.
	 *
	 * @throw std::runtime_error if DMI tables cannot be accessed or are malformed.
	 * @post isValid() returns true if initialization succeeded, false otherwise.
	 *
	 * @note Diagnostic information is printed to std::cerr on failure before rethrowing.
	 */
	DmiReader();

	/**
	 * @brief Checks if the DMI reader initialized successfully.
	 *
	 * @return true if DMI tables were loaded and parsed successfully, false otherwise.
	 */
	[[nodiscard]] bool isValid() const noexcept;

	/**
	 * @brief Retrieves all system slots from SMBIOS tables.
	 *
	 * Parses all Type 9 (System Slot) structures from the DMI tables.
	 *
	 * @return Vector of SlotInfo structures. Empty vector if reader is invalid or no slots found.
	 * @pre isValid() should return true for meaningful results.
	 */
	[[nodiscard]] std::vector<SlotInfo> getSlots() const;

	/**
	 * @brief Retrieves system information from SMBIOS tables.
	 *
	 * Parses the Type 1 (System Information) structure from the DMI tables.
	 *
	 * @return SystemInfo if found, std::nullopt otherwise.
	 * @pre isValid() should return true for meaningful results.
	 */
	[[nodiscard]] std::optional<SystemInfo> getSystemInfo() const;

	/**
	 * @brief Finds the physical slot containing a PCI device.
	 *
	 * Searches for a physical slot matching the given device BDF (Bus:Device.Function).
	 * If no direct match is found, walks up the PCIe hierarchy through parent bridges
	 * to locate the physical slot at the root of the device's topology path.
	 *
	 * @param[in] device_bdf PCI device address in format "SSSS:BB:DD.F" (e.g., "0000:01:00.0").
	 * @return SlotInfo if a matching physical slot is found, std::nullopt otherwise.
	 * @pre isValid() should return true for meaningful results.
	 * @pre device_bdf must be a valid BDF string format.
	 *
	 * @note Uses /sys/bus/pci/devices/ to traverse PCIe topology.
	 * @note Returns first matching slot when multiple parent bridges match.
	 */
	[[nodiscard]] std::optional<SlotInfo> findSlotForDevice(std::string_view deviceBdf) const;

private:
	std::vector<uint8_t> dmiData; ///< Raw DMI table data loaded from sysfs
	bool mIsValid = false;		  ///< Initialization status flag

	/**
	 * @brief Returns a view over the raw DMI data buffer.
	 *
	 * @return Read-only span of the DMI data bytes.
	 */
	[[nodiscard]] std::span<const uint8_t> dataSpan() const noexcept;

	/**
	 * @brief Reads an integer value from a buffer.
	 *
	 * Handles endianness conversion automatically based on host architecture.
	 *
	 * @tparam T Integral type to read (uint8_t, uint16_t, uint32_t, etc.).
	 * @param[in] buffer Source buffer to read from (non-owning view).
	 * @param[in] offset Byte offset within buffer to read from.
	 * @return The value read and converted to host endianness.
	 * @throw std::out_of_range if offset + sizeof(T) exceeds buffer size.
	 * @pre offset + sizeof(T) must be <= buffer.size()
	 */
	template <std::integral T>
	[[nodiscard]] constexpr T readFromBuffer(std::span<const uint8_t> buffer, size_t offset) const
	{
		if (offset + sizeof(T) > buffer.size()) {
			throw std::out_of_range("Buffer read out of bounds");
		}

		if constexpr (std::endian::native == std::endian::little) {
			T result;
			std::memcpy(&result, buffer.data() + offset, sizeof(T));
			return result;
		} else {
			T result = 0;
			for (size_t i = 0; i < sizeof(T); ++i) {
				result |= static_cast<T>(buffer[offset + i]) << (i * 8);
			}
			return result;
		}
	}

	/**
	 * @brief Walks PCIe topology to find all parent bridge BDFs for a device.
	 *
	 * Traverses /sys/bus/pci/devices/ hierarchy upward from the given device
	 * to collect all parent bridge addresses.
	 *
	 * @param[in] device_bdf PCI device address to start from.
	 * @return Vector of parent bridge BDF strings in bottom-up order (immediate parent first).
	 *         Empty vector if device not found or on filesystem errors.
	 *
	 * @note Does not throw on filesystem errors, returns empty vector instead.
	 */
	[[nodiscard]] static std::vector<std::string> getParentBridges(std::string_view deviceBdf);

	/**
	 * @brief Validates if a string is a properly formatted PCI BDF address.
	 *
	 * Checks for presence and ordering of ':' and '.' separators.
	 *
	 * @param[in] bdf String to validate.
	 * @return true if string matches BDF format pattern, false otherwise.
	 *
	 * @note Does not validate numeric ranges, only format structure.
	 */
	[[nodiscard]] static constexpr bool isValidBDF(std::string_view bdf) noexcept;

	/**
	 * @brief SMBIOS structure header present at start of each DMI table entry.
	 */
	struct SMBIOSHeader
	{
		uint8_t type;	 ///< SMBIOS structure type identifier
		uint8_t length;	 ///< Length of formatted section (excluding strings)
		uint16_t handle; ///< Unique handle for this structure
	} __attribute__((packed));

	/**
	 * @brief Loads raw DMI table data from /sys/firmware/dmi/tables/DMI.
	 *
	 * @throw std::runtime_error if DMI file cannot be opened, is empty, or too small.
	 * @post dmi_data contains complete DMI table on success.
	 */
	void loadDMIData();

	/**
	 * @brief Creates an iterator over all SMBIOS structures in the DMI table.
	 *
	 * @return Vector of (offset, header_pointer) pairs for each valid structure.
	 *         Header pointers are non-owning observers into dmi_data.
	 * @pre dmi_data must be populated with valid DMI table data.
	 *
	 * @note Returned pointers remain valid only while dmi_data is unchanged.
	 */
	[[nodiscard]] std::vector<std::pair<size_t, const SMBIOSHeader *>> getStructureIterator() const;

	/**
	 * @brief Validates that a structure at given offset is well-formed.
	 *
	 * @param[in] offset Byte offset of structure start in dmi_data.
	 * @param[in] min_size Minimum expected structure size in bytes.
	 * @return true if structure header exists and structure fits within dmi_data bounds, false otherwise.
	 */
	[[nodiscard]] bool validateStructure(size_t offset, size_t minSize) const noexcept;

	/**
	 * @brief Extracts a string from the string area of an SMBIOS structure.
	 *
	 * SMBIOS structures store strings in a null-terminated section following the
	 * formatted data area. Strings are indexed starting from 1.
	 *
	 * @param[in] structure_offset Offset of structure start in dmi_data.
	 * @param[in] structure_length Length of formatted section.
	 * @param[in] string_index 1-based index of string to retrieve (0 = no string).
	 * @return The requested string, or empty string if not found or index is 0.
	 *
	 * @note Limits returned strings to 256 bytes maximum for safety.
	 */
	[[nodiscard]] std::string getStringFromTable(size_t structureOffset, uint8_t structureLength,
												 uint8_t stringIndex) const;

	/**
	 * @brief Locates the next SMBIOS structure in the DMI table.
	 *
	 * Skips past current structure's formatted area and null-terminated string section.
	 *
	 * @param[in] current_offset Offset of current structure.
	 * @return Offset of next structure, or 0 if no more structures exist.
	 */
	[[nodiscard]] size_t findNextStructure(size_t currentOffset) const noexcept;

	/**
	 * @brief Parses an SMBIOS Type 9 (System Slot) structure.
	 *
	 * @param[in] offset Byte offset of structure in dmi_data.
	 * @return Parsed SlotInfo if valid, std::nullopt if structure is malformed.
	 * @pre Structure at offset must be validated before calling.
	 *
	 * @note Attempts multiple offset strategies for bus address to handle SMBIOS version variations.
	 */
	[[nodiscard]] std::optional<SlotInfo> parseSlot(size_t offset) const;

	/**
	 * @brief Parses an SMBIOS Type 1 (System Information) structure.
	 *
	 * @param[in] offset Byte offset of structure in dmi_data.
	 * @return Parsed SystemInfo if valid, std::nullopt if structure is malformed.
	 * @pre Structure at offset must be validated before calling.
	 */
	[[nodiscard]] std::optional<SystemInfo> parseSystemInfo(size_t offset) const;

	/**
	 * @brief Decodes SMBIOS slot type byte to human-readable string.
	 *
	 * @param[in] type Raw SMBIOS slot type value.
	 * @return Human-readable slot type description.
	 */
	[[nodiscard]] static std::string decodeSlotType(uint8_t type);

	/**
	 * @brief Decodes SMBIOS slot usage byte to human-readable string.
	 *
	 * @param[in] usage Raw SMBIOS slot usage value.
	 * @return Human-readable usage status description.
	 */
	[[nodiscard]] static std::string decodeSlotUsage(uint8_t usage);
};
