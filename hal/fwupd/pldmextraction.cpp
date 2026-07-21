/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

/**
 * @file pldmextraction.cpp
 * @brief Implementation of the DSP0267 PLDM firmware update package reader
 *
 * Implements the interface declared in pldmextraction.h. Everything that is not part of that
 * interface -- the CRC, the bounds-checked cursor, the UUID table, the header parser -- is local to
 * this translation unit.
 *
 * Parsing uses exceptions internally so that any malformed field aborts the walk at the point of
 * discovery; the public entry points catch them at the boundary and report a Result, so no
 * exception escapes the library.
 *
 * @note Nothing here writes to stdout or stderr -- diagnostics are the caller's business.
 */

#include "pldmextraction.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace pldm_fw {
namespace {

/** @brief One entry from the Component Image Information Area (DSP0267) */
struct ComponentImageInfo
{
	uint16_t classification = 0;	  /**< ComponentClassification */
	uint16_t identifier = 0;		  /**< ComponentIdentifier */
	uint32_t comparisonStamp = 0;	  /**< ComponentComparisonStamp */
	uint16_t options = 0;			  /**< ComponentOptions */
	uint16_t requestedActivation = 0; /**< RequestedComponentActivationMethod */
	uint32_t locationOffset = 0;	  /**< offset of raw image within the package */
	uint32_t size = 0;				  /**< length of raw image in bytes */
	std::string versionString;		  /**< ComponentVersionString */
};

/**
 * @brief Computes the CRC-32 DSP0267 specifies for the package checksums
 *
 * Reflected CRC-32, polynomial 0xEDB88320, init and xorout 0xFFFFFFFF -- the algorithm used for
 * both PackageHeaderChecksum and PackagePayloadChecksum.
 *
 * @param data Start of the buffer to checksum
 * @param len Length of the buffer in bytes
 * @return uint32_t CRC-32 over the buffer
 */
uint32_t crc32(const uint8_t *data, size_t len)
{
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < len; ++i) {
		crc ^= data[i];
		for (int k = 0; k < 8; ++k) {
			crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
		}
	}
	return ~crc;
}

/**
 * @brief Forward-only cursor over the package buffer, with bounds checking
 *
 * Every read is checked against the buffer length, so a malformed or truncated package can never
 * walk the parser off the end of the buffer.
 *
 * @note Reads throw std::runtime_error when they would leave the buffer; parsePackageHeader() lets
 *       that unwind and the public entry points turn it into Result::MalformedPackage.
 */
class PackageReader
{
public:
	/**
	 * @brief Constructs a cursor positioned at the start of a package buffer
	 *
	 * @param data Start of the package buffer
	 * @param len Length of the package buffer in bytes
	 */
	PackageReader(const uint8_t *data, size_t len) : mData(data), mLen(len) {}

	/**
	 * @brief Moves the cursor to an absolute position
	 *
	 * @param pos Byte offset from the start of the package buffer
	 */
	void seek(size_t pos)
	{
		if (pos > mLen) {
			throw std::runtime_error("seek past end of package");
		}
		mPos = pos;
	}

	/**
	 * @brief Reports the current cursor position
	 *
	 * @return size_t Byte offset of the cursor from the start of the package buffer
	 */
	size_t position() const { return mPos; }

	/**
	 * @brief Reads one byte and advances the cursor
	 *
	 * @return uint8_t The byte at the cursor
	 */
	uint8_t readU8() { return read<uint8_t>(); }

	/**
	 * @brief Reads a little-endian 16-bit value and advances the cursor
	 *
	 * @return uint16_t The value at the cursor
	 */
	uint16_t readU16() { return read<uint16_t>(); }

	/**
	 * @brief Reads a little-endian 32-bit value and advances the cursor
	 *
	 * @return uint32_t The value at the cursor
	 */
	uint32_t readU32() { return read<uint32_t>(); }

	/**
	 * @brief Advances the cursor past a run of bytes
	 *
	 * @param len Number of bytes to skip
	 */
	void skip(size_t len)
	{
		require(len);
		mPos += len;
	}

	/**
	 * @brief Reads a fixed-length string and advances the cursor
	 *
	 * @param len Number of bytes to read
	 * @return std::string The bytes at the cursor, taken verbatim
	 */
	std::string readString(size_t len)
	{
		require(len);
		std::string value(reinterpret_cast<const char *>(mData + mPos), len);
		mPos += len;
		return value;
	}

private:
	/**
	 * @brief Reads a little-endian integer of the given width and advances the cursor
	 *
	 * @return T The value assembled from the bytes at the cursor
	 */
	template <typename T> T read()
	{
		require(sizeof(T));
		T value = 0;
		for (size_t i = 0; i < sizeof(T); ++i) { // little-endian assemble
			value |= static_cast<T>(mData[mPos + i]) << (8 * i);
		}
		mPos += sizeof(T);
		return value;
	}

	/**
	 * @brief Checks that a read of the given size stays inside the buffer
	 *
	 * @param len Number of bytes about to be read
	 */
	void require(size_t len) const
	{
		if (mPos + len > mLen) {
			throw std::runtime_error("unexpected end of package");
		}
	}

	const uint8_t *mData; /**< the package buffer, borrowed from the caller */
	size_t mLen;		  /**< length of the package buffer in bytes */
	size_t mPos = 0;	  /**< current cursor position */
};

/**
 * @brief A PackageHeaderIdentifier UUID and the format revision it goes with
 *
 * Each DSP0267 format revision defines its own UUID; a binary is a Type 5 package if and only if
 * its first 16 bytes match one of these.
 */
struct PackageUuid
{
	const char *name;  /**< human-readable format name, e.g. "DSP0267 v1.3.0" */
	uint8_t revision;  /**< the PackageHeaderFormatRevision this UUID goes with */
	uint8_t bytes[16]; /**< the PackageHeaderIdentifier itself */
};

/**
 * @brief Known PackageHeaderIdentifier UUIDs for the PLDM firmware update package format
 *
 * @return const std::vector<PackageUuid> & The UUID table, one entry per known format revision
 */
const std::vector<PackageUuid> &knownPackageUuids()
{
	static const std::vector<PackageUuid> uuids = {
		// DSP0267 v1.0.x (format revision 1)
		{"DSP0267 v1.0.x",
		 1,
		 {0xf0, 0x18, 0x87, 0x8c, 0xcb, 0x7d, 0x49, 0x43, 0x98, 0x00, 0xa0, 0x2f, 0x05, 0x9a, 0xca, 0x02}},
		// DSP0267 v1.1.0 (format revision 2)
		{"DSP0267 v1.1.0",
		 2,
		 {0x12, 0x44, 0xd2, 0x64, 0x8d, 0x7d, 0x47, 0x18, 0xa0, 0x30, 0xfc, 0x8a, 0x56, 0x58, 0x7d, 0x5a}},
		// DSP0267 v1.2.0 (format revision 3)
		{"DSP0267 v1.2.0",
		 3,
		 {0x31, 0x19, 0xce, 0x2f, 0xe8, 0x0a, 0x44, 0x93, 0x98, 0x83, 0xc9, 0x35, 0x62, 0x9e, 0x6d, 0x59}},
		// DSP0267 v1.3.0 (format revision 4)
		{"DSP0267 v1.3.0",
		 4,
		 {0x7b, 0x29, 0x1c, 0x99, 0x6d, 0xb6, 0x42, 0x08, 0x80, 0x1b, 0x02, 0x02, 0x6e, 0x46, 0x3c, 0x78}},
	};
	return uuids;
}

/**
 * @brief Size of the signature block some vendors put ahead of the programmable payload
 *
 * The block is zero-padded out to this fixed size and only the payload behind it gets programmed.
 * Other vendors put the payload at offset 0, so findPayloadOffset() probes rather than assumes.
 */
constexpr size_t SIGNATURE_BLOCK_SIZE = 4096;

/**
 * @brief Magic numbers that mark the start of a programmable payload
 *
 * @return const std::vector<std::string> & The magics to probe for, in no particular order
 */
const std::vector<std::string> &payloadMagics()
{
	static const std::vector<std::string> magics = {"$FPT"}; // Intel IFWI flash partition table
	return magics;
}

/**
 * @brief Finds the offset within a component image at which the programmable payload starts
 *
 * @param image Start of the raw component image
 * @param size Length of the raw component image in bytes
 * @return size_t Offset of a known payload magic, or SIZE_MAX when no candidate offset carries one
 */
size_t findPayloadOffset(const uint8_t *image, size_t size)
{
	for (size_t offset : {size_t(0), SIGNATURE_BLOCK_SIZE}) {
		for (const auto &magic : payloadMagics()) {
			if (size >= offset + magic.size() && std::memcmp(image + offset, magic.data(), magic.size()) == 0) {
				return offset;
			}
		}
	}
	return SIZE_MAX;
}

/** @brief The parsed header: enough to locate and describe every component image */
struct ParsedPackage
{
	std::vector<ComponentImageInfo> components; /**< the Component Image Information Area */
	const uint8_t *raw = nullptr;				/**< the full package buffer */
	size_t rawLen = 0;							/**< length of the package buffer in bytes */
	const char *formatName = nullptr;			/**< human-readable format name */
	uint8_t formatRevision = 0;					/**< PackageHeaderFormatRevision */
	uint16_t headerSize = 0;					/**< PackageHeaderSize */
	uint32_t headerChecksum = 0;				/**< PackageHeaderChecksum (all revisions) */
	bool hasPayloadChecksum = false;			/**< PackagePayloadChecksum: revision >= 4 */
	uint32_t payloadChecksum = 0;				/**< PackagePayloadChecksum, when present */
};

/**
 * @brief Parses the package header far enough to locate every component image
 *
 * @param data Start of the package buffer
 * @param len Length of the package buffer in bytes
 * @return ParsedPackage The parsed header, with one entry per component image
 *
 * @note Throws std::runtime_error on any inconsistency; callers turn that into
 *       Result::MalformedPackage.
 */
ParsedPackage parsePackageHeader(const uint8_t *data, size_t len)
{
	PackageReader reader(data, len);

	// --- Package Header Information ---
	reader.skip(16);							   // PackageHeaderIdentifier (UUID)
	uint8_t formatRevision = reader.readU8();	   // PackageHeaderFormatRevision
	uint16_t packageHeaderSize = reader.readU16(); // total header size, up to & incl. checksum(s)
	reader.skip(13);							   // PackageReleaseDateTime
	reader.skip(2);								   // ComponentBitmapBitLength
	reader.skip(1);								   // PackageVersionStringType
	uint8_t packageVersionLen = reader.readU8();
	reader.skip(packageVersionLen); // PackageVersionString

	if (formatRevision == 0) {
		throw std::runtime_error("invalid PackageHeaderFormatRevision 0");
	}
	// A revision above PLDM_MAX_KNOWN_FORMAT_REVISION is parsed as that revision. Being a library,
	// we do not log; PackageInfo::formatRevision reports what was found so the caller can decide
	// whether to warn.

	// --- Firmware Device ID Records ---
	// Each record is prefixed by RecordLength, so we can skip whole records without decoding their
	// internals. That also covers the fields later revisions appended inside the record
	// (FirmwareDevicePackageData in v1.1.0, ReferenceManifestData in v1.2.0).
	uint8_t deviceRecordCount = reader.readU8();
	for (uint8_t i = 0; i < deviceRecordCount; ++i) {
		size_t recordStart = reader.position();
		uint16_t recordLength = reader.readU16();
		if (recordLength < 2) {
			throw std::runtime_error("bad FD record length");
		}
		reader.seek(recordStart + recordLength);
	}

	// --- Downstream Device ID Records ---
	// Added in PackageHeaderFormatRevision >= 2 (DSP0267 v1.1.0). Same layout as the FD records: a
	// count byte followed by RecordLength-prefixed records, so we skip each record whole.
	if (formatRevision >= 2) {
		uint8_t downstreamRecordCount = reader.readU8();
		for (uint8_t i = 0; i < downstreamRecordCount; ++i) {
			size_t recordStart = reader.position();
			uint16_t recordLength = reader.readU16();
			if (recordLength < 2) {
				throw std::runtime_error("bad downstream record length");
			}
			reader.seek(recordStart + recordLength);
		}
	}

	// --- Component Image Information Area ---
	ParsedPackage pkg;
	pkg.raw = data;
	pkg.rawLen = len;
	pkg.formatRevision = formatRevision;
	pkg.headerSize = packageHeaderSize;

	uint16_t componentCount = reader.readU16();
	pkg.components.reserve(componentCount);
	for (uint16_t i = 0; i < componentCount; ++i) {
		ComponentImageInfo component;
		component.classification = reader.readU16();
		component.identifier = reader.readU16();
		component.comparisonStamp = reader.readU32();
		component.options = reader.readU16();
		component.requestedActivation = reader.readU16();
		component.locationOffset = reader.readU32();
		component.size = reader.readU32();
		reader.skip(1); // ComponentVersionStringType
		uint8_t versionLen = reader.readU8();
		component.versionString = reader.readString(versionLen);

		// ComponentOpaqueDataLength + ComponentOpaqueData were added in PackageHeaderFormatRevision
		// >= 2. Skip them so the loop stays aligned on newer packages; older packages don't have
		// these bytes.
		if (formatRevision >= 2) {
			uint32_t opaqueLen = reader.readU32();
			reader.skip(opaqueLen);
		}

		pkg.components.push_back(std::move(component));
	}

	// --- Header trailer ---
	// Revisions 1-3 end the header with just the 4-byte PackageHeaderChecksum. DSP0267 v1.3.0
	// (revision 4) appends a 4-byte PackagePayloadChecksum, a CRC-32 over everything after the
	// header, making the trailer 8 bytes.
	const size_t trailerSize = (formatRevision >= 4) ? 8 : 4;

	// The trailer is all that follows the component area, so the current position + trailerSize
	// must land exactly on PackageHeaderSize. A mismatch means our layout drifted from the actual
	// package (wrong revision assumption, vendor deviation, or corruption).
	const size_t expectedEnd = reader.position() + trailerSize;
	if (expectedEnd != packageHeaderSize) {
		throw std::runtime_error("header parse misaligned: reached " + std::to_string(expectedEnd) +
								 " but PackageHeaderSize is " + std::to_string(packageHeaderSize) +
								 " (PackageHeaderFormatRevision " + std::to_string(formatRevision) + ")");
	}
	if (packageHeaderSize > len) {
		throw std::runtime_error("PackageHeaderSize exceeds package length");
	}

	pkg.headerChecksum = reader.readU32();
	if (trailerSize == 8) {
		pkg.payloadChecksum = reader.readU32();
		pkg.hasPayloadChecksum = true;
	}

	// The UUID already matched in isPldmType5(); recover its name for reporting.
	for (const auto &uuid : knownPackageUuids()) {
		if (std::memcmp(data, uuid.bytes, 16) == 0) {
			pkg.formatName = uuid.name;
			break;
		}
	}

	return pkg;
}

/**
 * @brief Validates the arguments, claims the format, and parses the package
 *
 * Shared front half of every public entry point.
 *
 * @param data Start of the package buffer
 * @param len Length of the package buffer in bytes
 * @param pkg Structure receiving the parsed header
 * @return Result Result::Success with *pkg filled in, or the Result the caller should return
 */
Result openPackage(const uint8_t *data, size_t len, ParsedPackage *pkg) noexcept
{
	if (!data || len == 0 || !pkg) {
		return Result::InvalidArgument;
	}
	if (!isPldmType5(data, len)) {
		return Result::NotPldmType5;
	}
	try {
		*pkg = parsePackageHeader(data, len);
	} catch (const std::exception &) {
		return Result::MalformedPackage;
	}
	return Result::Success;
}

/** @brief The bytes of one component the caller actually wants, signature block excluded */
struct ImageSpan
{
	const uint8_t *begin = nullptr; /**< first byte to hand back */
	size_t size = 0;				/**< bytes to hand back (signature excluded) */
	size_t signatureSize = 0;		/**< bytes skipped at the head */
};

/**
 * @brief Locates the programmable bytes of one component within the package buffer
 *
 * @param pkg The parsed package
 * @param component The component table entry to locate
 * @param opts Signature handling policy, see ExtractOptions
 * @param span Structure receiving the located byte range
 * @return Result Status of the lookup
 * @retval Result::Success *span describes the bytes to hand back
 * @retval Result::MalformedPackage The image runs past the end of the package buffer
 * @retval Result::NothingAfterSignature The signature skip covers the whole image
 */
Result resolveImage(const ParsedPackage &pkg, const ComponentImageInfo &component, const ExtractOptions &opts,
					ImageSpan *span)
{
	const size_t end = static_cast<size_t>(component.locationOffset) + component.size;
	if (end > pkg.rawLen || end < component.locationOffset) { // second test catches overflow
		return Result::MalformedPackage;
	}

	const uint8_t *image = pkg.raw + component.locationOffset;

	size_t signatureSize = opts.signatureSkip;
	if (signatureSize == PLDM_AUTO_DETECT_SIGNATURE) {
		const size_t found = findPayloadOffset(image, component.size);
		signatureSize = (found == SIZE_MAX) ? 0 : found; // no magic: take it as-is
	}
	if (signatureSize >= component.size) {
		return Result::NothingAfterSignature;
	}

	span->begin = image + signatureSize;
	span->size = component.size - signatureSize;
	span->signatureSize = signatureSize;
	return Result::Success;
}

/**
 * @brief Fills in a public ComponentInfo from a parsed table entry
 *
 * @param pkg The parsed package
 * @param index Position of the component in the package's component table
 * @param occurrence How many earlier components share this one's identifier
 * @param opts Signature handling policy, see ExtractOptions
 * @param out Structure receiving the component description
 * @return Result Result::Success when @p out was filled in, an error code otherwise
 */
Result describeComponent(const ParsedPackage &pkg, size_t index, uint8_t occurrence, const ExtractOptions &opts,
						 ComponentInfo *out)
{
	const ComponentImageInfo &component = pkg.components[index];

	ImageSpan span;
	const Result rc = resolveImage(pkg, component, opts, &span);
	if (rc != Result::Success) {
		return rc;
	}

	out->identifier = component.identifier;
	out->classification = component.classification;
	out->comparisonStamp = component.comparisonStamp;
	out->index = static_cast<uint16_t>(index);
	out->occurrence = occurrence;
	out->imageSize = component.size;
	out->signatureSize = static_cast<uint32_t>(span.signatureSize);
	out->requiredSize = static_cast<uint32_t>(span.size);

	const size_t versionLen = component.versionString.size() < PLDM_MAX_VERSION_STRING_LEN
								  ? component.versionString.size()
								  : PLDM_MAX_VERSION_STRING_LEN;
	std::memcpy(out->version, component.versionString.data(), versionLen);
	out->version[versionLen] = '\0';
	return Result::Success;
}

/**
 * @brief Counts how many earlier components share one component's identifier
 *
 * Identifiers are unique only per firmware device, so a package may legitimately repeat them.
 *
 * @param pkg The parsed package
 * @param index Position of the component in the package's component table
 * @return uint8_t The component's occurrence, saturated at 255
 */
uint8_t componentOccurrence(const ParsedPackage &pkg, size_t index)
{
	size_t occurrence = 0;
	for (size_t i = 0; i < index; ++i) {
		if (pkg.components[i].identifier == pkg.components[index].identifier) {
			++occurrence;
		}
	}
	return static_cast<uint8_t>(occurrence < 255 ? occurrence : 255);
}

} // namespace

/**
 * @brief Human-readable form of a Result, for logging
 *
 * @param result The result code to describe
 * @return const char * Static description of @p result, never null
 */
const char *resultToString(Result result) noexcept
{
	switch (result) {
	case Result::Success:
		return "success";
	case Result::InvalidArgument:
		return "invalid argument";
	case Result::NotPldmType5:
		return "not a PLDM Type 5 firmware package";
	case Result::MalformedPackage:
		return "malformed or truncated PLDM package";
	case Result::ComponentNotFound:
		return "no such component identifier in package";
	case Result::BufferTooSmall:
		return "supplied buffer is too small for the component image";
	case Result::NothingAfterSignature:
		return "nothing left in the component image after the signature block";
	case Result::ChecksumMismatch:
		return "package checksum mismatch";
	}
	return "unknown result code";
}

/**
 * @brief Checks whether a buffer is a PLDM Type 5 firmware update package
 *
 * Checks the 16-byte PackageHeaderIdentifier UUID against the ones DSP0267 defines and
 * sanity-checks PackageHeaderSize against the buffer length. Cheap: no full parse, so callers use
 * this to claim a file before parsing it.
 *
 * @param data Start of the package buffer
 * @param len Length of the package buffer in bytes
 * @param formatName Optional out parameter receiving a static string naming the revision on a match
 * @return bool true when the buffer is a known Type 5 package, false otherwise
 */
bool isPldmType5(const uint8_t *data, size_t len, const char **formatName) noexcept
{
	// Smallest conceivable header: UUID(16) + revision(1) + size(2) + date(13) + bitmapLen(2) +
	// verType(1) + verLen(1) = 36 bytes.
	constexpr size_t minimumHeaderSize = 36;
	if (!data || len < minimumHeaderSize) {
		return false;
	}

	for (const auto &uuid : knownPackageUuids()) {
		if (std::memcmp(data, uuid.bytes, 16) == 0) {
			// PackageHeaderSize (little-endian at offset 17) must fit the buffer.
			uint16_t headerSize = static_cast<uint16_t>(data[17]) | (static_cast<uint16_t>(data[18]) << 8);
			if (headerSize == 0 || headerSize > len) {
				return false;
			}
			if (formatName) {
				*formatName = uuid.name;
			}
			return true;
		}
	}
	return false;
}

/**
 * @brief Enumerates the components of a package into a caller-supplied array
 *
 * Reports each component's identifier and the buffer size its image needs with the signature block
 * excluded (ComponentInfo::requiredSize).
 *
 * @param data Start of the package buffer
 * @param len Length of the package buffer in bytes
 * @param out Array receiving the component descriptions, may be null when @p maxOut is 0
 * @param maxOut Number of entries @p out can hold
 * @param countOut Optional out parameter receiving the package's total component count
 * @param opts Signature handling policy, see ExtractOptions
 * @return Result Status of the enumeration
 * @retval Result::Success Every component was described into @p out
 * @retval Result::BufferTooSmall @p out cannot hold every component; *countOut holds the count needed
 * @retval Result::InvalidArgument @p out is null while @p maxOut is not 0, or the package buffer is empty
 *
 * @note *countOut is the number of entries written on Success, and the count needed on
 *       Result::BufferTooSmall, so a failed call can size the retry.
 * @note Pass @p out == nullptr with @p maxOut == 0 to query the count alone.
 */
Result getComponentList(const uint8_t *data, size_t len, ComponentInfo *out, size_t maxOut, size_t *countOut,
						const ExtractOptions &opts) noexcept
{
	if (countOut) {
		*countOut = 0;
	}
	if (!out && maxOut != 0) {
		return Result::InvalidArgument;
	}

	ParsedPackage pkg;
	const Result open = openPackage(data, len, &pkg);
	if (open != Result::Success) {
		return open;
	}

	// Report the full count even when it does not fit, so the caller can size a second call from
	// the first one's answer.
	if (countOut) {
		*countOut = pkg.components.size();
	}
	if (pkg.components.empty()) {
		return Result::Success;
	}
	// A null `out` implies maxOut == 0 (rejected above), so this also covers the count-probe call
	// spelled getComponentList(data, len, nullptr, 0, &count).
	if (!out || pkg.components.size() > maxOut) {
		return Result::BufferTooSmall;
	}

	for (size_t i = 0; i < pkg.components.size(); ++i) {
		const Result rc = describeComponent(pkg, i, componentOccurrence(pkg, i), opts, &out[i]);
		if (rc != Result::Success) {
			return rc;
		}
	}
	return Result::Success;
}

/**
 * @brief Enumerates the components of a package into a std::vector
 *
 * std::vector convenience form of getComponentList(): sizes the vector to the component count.
 *
 * @param data Start of the package buffer
 * @param len Length of the package buffer in bytes
 * @param out Vector receiving the component descriptions, cleared on failure
 * @param opts Signature handling policy, see ExtractOptions
 * @return Result Result::Success when every component was described, an error code otherwise
 *
 * @note Unlike the rest of the API this may throw std::bad_alloc, from the vector itself.
 */
Result getComponentList(const uint8_t *data, size_t len, std::vector<ComponentInfo> &out, const ExtractOptions &opts)
{
	out.clear();

	size_t count = 0;
	const Result probe = getComponentList(data, len, nullptr, 0, &count, opts);
	if (probe != Result::Success && probe != Result::BufferTooSmall) {
		return probe;
	}
	if (count == 0) {
		return Result::Success;
	}

	out.resize(count);
	const Result rc = getComponentList(data, len, out.data(), out.size(), &count, opts);
	if (rc != Result::Success) {
		out.clear();
	}
	return rc;
}

/**
 * @brief Copies the image of one component into the caller's buffer
 *
 * The signature block, if any, is excluded from the copy.
 *
 * @param data Start of the package buffer
 * @param len Length of the package buffer in bytes
 * @param componentId ComponentIdentifier of the component to extract
 * @param buffer Buffer receiving the component image
 * @param bufferSize Size of @p buffer in bytes
 * @param bytesWritten Optional out parameter receiving the number of bytes copied
 * @param occurrence Which of the components sharing @p componentId to take, in table order
 * @param opts Signature handling policy, see ExtractOptions
 * @return Result Status of the extraction
 * @retval Result::Success The image was copied and *bytesWritten holds its size
 * @retval Result::ComponentNotFound No component matches @p componentId and @p occurrence
 * @retval Result::BufferTooSmall @p buffer is too short; *bytesWritten holds the size it needs
 *
 * @note @p occurrence selects among components sharing an identifier, in table order (see
 *       ComponentInfo::occurrence); 0 is the first such component.
 * @note On Result::BufferTooSmall nothing is written, so a failed call can size the retry. On
 *       every other failure *bytesWritten is 0.
 */
Result extractComponent(const uint8_t *data, size_t len, uint16_t componentId, uint8_t *buffer, size_t bufferSize,
						size_t *bytesWritten, unsigned occurrence, const ExtractOptions &opts) noexcept
{
	if (bytesWritten) {
		*bytesWritten = 0;
	}
	if (!buffer && bufferSize != 0) {
		return Result::InvalidArgument;
	}

	ParsedPackage pkg;
	const Result open = openPackage(data, len, &pkg);
	if (open != Result::Success) {
		return open;
	}

	// Walk the table counting matches so `occurrence` picks among components that share an
	// identifier.
	unsigned seen = 0;
	for (size_t i = 0; i < pkg.components.size(); ++i) {
		const ComponentImageInfo &component = pkg.components[i];
		if (component.identifier != componentId) {
			continue;
		}
		if (seen++ != occurrence) {
			continue;
		}

		ImageSpan span;
		const Result rc = resolveImage(pkg, component, opts, &span);
		if (rc != Result::Success) {
			return rc;
		}

		// Report the needed size on a short buffer so the caller can retry.
		if (bufferSize < span.size) {
			if (bytesWritten) {
				*bytesWritten = span.size;
			}
			return Result::BufferTooSmall;
		}

		std::memcpy(buffer, span.begin, span.size);
		if (bytesWritten) {
			*bytesWritten = span.size;
		}
		return Result::Success;
	}
	return Result::ComponentNotFound;
}

/**
 * @brief Reads the package-wide header fields
 *
 * @param data Start of the package buffer
 * @param len Length of the package buffer in bytes
 * @param out Structure receiving the package header fields
 * @return Result Result::Success when the header was parsed, an error code otherwise
 */
Result getPackageInfo(const uint8_t *data, size_t len, PackageInfo *out) noexcept
{
	if (!out) {
		return Result::InvalidArgument;
	}

	ParsedPackage pkg;
	const Result open = openPackage(data, len, &pkg);
	if (open != Result::Success) {
		return open;
	}

	out->formatName = pkg.formatName;
	out->formatRevision = pkg.formatRevision;
	out->headerSize = pkg.headerSize;
	out->componentCount = static_cast<uint16_t>(pkg.components.size());
	return Result::Success;
}

/**
 * @brief Verifies the package's own checksums
 *
 * @param data Start of the package buffer
 * @param len Length of the package buffer in bytes
 * @param out Optional structure receiving the stored and computed checksums
 * @return Result Result::Success when every checksum present matches, Result::ChecksumMismatch when
 *         one does not, an error code when the package could not be parsed
 *
 * @note *out is filled in on both Success and ChecksumMismatch, so a caller can log stored vs
 *       computed values.
 */
Result verifyChecksums(const uint8_t *data, size_t len, ChecksumStatus *out) noexcept
{
	ParsedPackage pkg;
	const Result open = openPackage(data, len, &pkg);
	if (open != Result::Success) {
		return open;
	}

	const size_t trailerSize = pkg.hasPayloadChecksum ? 8u : 4u;
	if (pkg.headerSize < trailerSize || pkg.headerSize > pkg.rawLen) {
		return Result::MalformedPackage;
	}

	ChecksumStatus status;
	status.storedHeader = pkg.headerChecksum;
	status.computedHeader = crc32(pkg.raw, pkg.headerSize - trailerSize);
	status.headerOk = status.computedHeader == status.storedHeader;

	status.payloadPresent = pkg.hasPayloadChecksum;
	if (status.payloadPresent) {
		status.storedPayload = pkg.payloadChecksum;
		status.computedPayload = crc32(pkg.raw + pkg.headerSize, pkg.rawLen - pkg.headerSize);
		status.payloadOk = status.computedPayload == status.storedPayload;
	}

	if (out) {
		*out = status;
	}
	return (status.headerOk && (!status.payloadPresent || status.payloadOk)) ? Result::Success
																			 : Result::ChecksumMismatch;
}

} // namespace pldm_fw
