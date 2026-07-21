/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

/**
 * @file pldmextraction.h
 * @brief Public interface for reading DSP0267 PLDM firmware update ("Type 5") packages
 *
 * The library never allocates for image data and never throws: the caller supplies every output
 * buffer, and every operation reports a Result code. Package bytes are borrowed, not copied --
 * each call takes a (data, len) pair that must stay valid and unmodified for the duration of the
 * call.
 *
 * Typical use, sizing the buffer from the package itself:
 * @code
 *     if (!pldm_fw::isPldmType5(data, len))
 *         return; // not our format
 *
 *     std::vector<pldm_fw::ComponentInfo> comps;
 *     if (pldm_fw::getComponentList(data, len, comps) != pldm_fw::Result::Success)
 *         return;
 *
 *     for (const auto &c : comps) {
 *         std::vector<uint8_t> image(c.requiredSize); // signature excluded
 *         size_t written = 0;
 *         auto rc = pldm_fw::extractComponent(data, len, c.identifier, image.data(), image.size(),
 *                                             &written, c.occurrence);
 *         if (rc == pldm_fw::Result::Success)
 *             consume(image.data(), written);
 *     }
 * @endcode
 *
 * @note Package header format revisions understood: 1 (DSP0267 v1.0.x) through 4 (DSP0267 v1.3.0).
 * @note Newer revisions are parsed as revision 4.
 */

#ifndef _PLDMEXTRACTION_H_
#define _PLDMEXTRACTION_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pldm_fw {

/**
 * @brief Outcome of a library call
 *
 * Success is 0; every other value is a failure and leaves caller-supplied output buffers
 * unmodified, except where noted.
 */
enum class Result : int
{
	Success = 0,
	InvalidArgument,	   /**< null pointer, or zero-length package buffer */
	NotPldmType5,		   /**< PackageHeaderIdentifier is not a known Type 5 UUID */
	MalformedPackage,	   /**< truncated, misaligned, or self-inconsistent header */
	ComponentNotFound,	   /**< no component with the requested id (and occurrence) */
	BufferTooSmall,		   /**< caller's buffer is shorter than requiredSize */
	NothingAfterSignature, /**< the signature skip covers the whole image */
	ChecksumMismatch	   /**< a CRC-32 the package stores over itself is wrong */
};

// Human-readable form of a Result, for logging. Never null.
const char *resultToString(Result result) noexcept;

/**
 * @brief Passed as ExtractOptions::signatureSkip to probe for a vendor signature block
 *
 * Selects auto-detection instead of the library being told the signature size.
 */
constexpr size_t PLDM_AUTO_DETECT_SIGNATURE = SIZE_MAX;

/** @brief DSP0267 caps ComponentVersionString at 255 bytes */
constexpr size_t PLDM_MAX_VERSION_STRING_LEN = 255;

/**
 * @brief Highest PackageHeaderFormatRevision whose layout this library knows
 *
 * Newer revisions are parsed as this one rather than rejected, on the assumption that DSP0267
 * keeps appending fields. The library stays silent about it; compare PackageInfo::formatRevision
 * against this to warn or refuse as you see fit.
 */
constexpr uint8_t PLDM_MAX_KNOWN_FORMAT_REVISION = 4;

/**
 * @brief How to treat the head of a component image
 *
 * Some vendors wrap the programmable payload in a signature block, zero-padded to a fixed size;
 * only the payload is meant to be programmed. By default the library probes for a known payload
 * magic and treats anything ahead of it as signature. Set signatureSkip to a byte count to
 * override that (0 disables trimming and yields the image exactly as stored).
 *
 * @note Pass the SAME options to getComponentList() and extractComponent(): the signature policy
 *       is what makes requiredSize match the bytes written.
 */
struct ExtractOptions
{
	size_t signatureSkip = PLDM_AUTO_DETECT_SIGNATURE;
};

/**
 * @brief One entry of the package's Component Image Information Area
 *
 * Carries the buffer size the component's image needs. Plain data, safe to memcpy or store in C
 * structures.
 */
struct ComponentInfo
{
	uint16_t identifier = 0;							/**< ComponentIdentifier */
	uint16_t classification = 0;						/**< ComponentClassification */
	uint32_t comparisonStamp = 0;						/**< ComponentComparisonStamp */
	uint16_t index = 0;									/**< position in the component table */
	uint8_t occurrence = 0;								/**< 0 for the first component carrying this
															 identifier, 1 for the next, and so on;
															 identifiers need not be unique in a package */
	uint32_t imageSize = 0;								/**< bytes stored in the package */
	uint32_t signatureSize = 0;							/**< leading bytes classified as signature */
	uint32_t requiredSize = 0;							/**< imageSize - signatureSize: what to allocate */
	char version[PLDM_MAX_VERSION_STRING_LEN + 1] = {}; /**< ComponentVersionString, NUL-terminated */
};

/** @brief Package-wide header fields */
struct PackageInfo
{
	const char *formatName = nullptr; /**< e.g. "DSP0267 v1.3.0"; never null on success */
	uint8_t formatRevision = 0;		  /**< PackageHeaderFormatRevision */
	uint16_t headerSize = 0;		  /**< PackageHeaderSize */
	uint16_t componentCount = 0;	  /**< number of entries in the component table */
};

/** @brief Result of checking the CRC-32s the package stores over itself */
struct ChecksumStatus
{
	bool headerOk = false;		  /**< PackageHeaderChecksum matches */
	bool payloadPresent = false;  /**< PackagePayloadChecksum exists: revision >= 4 */
	bool payloadOk = false;		  /**< meaningful only when payloadPresent */
	uint32_t storedHeader = 0;	  /**< PackageHeaderChecksum as stored */
	uint32_t computedHeader = 0;  /**< CRC-32 computed over the header */
	uint32_t storedPayload = 0;	  /**< PackagePayloadChecksum as stored */
	uint32_t computedPayload = 0; /**< CRC-32 computed over the payload */
};

// Every function below is documented on its definition in pldmextraction.cpp.

// Is this buffer a PLDM Type 5 firmware update package? Cheap: no full parse, so call this to
// claim a file before parsing it. On a match, *formatName (when non-null) receives a static
// string naming the revision.
bool isPldmType5(const uint8_t *data, size_t len, const char **formatName = nullptr) noexcept;

// Enumerates the components, reporting each one's identifier and the buffer size its image needs
// with the signature block excluded (ComponentInfo::requiredSize). Writes up to maxOut entries to
// out; *countOut (when non-null) receives the package's total component count, so a
// BufferTooSmall return sizes the retry. Pass out == nullptr with maxOut == 0 to query the count
// alone.
Result getComponentList(const uint8_t *data, size_t len, ComponentInfo *out, size_t maxOut, size_t *countOut,
						const ExtractOptions &opts = ExtractOptions()) noexcept;

// std::vector convenience form: sizes the vector to the component count. Unlike the rest of the
// API this may throw std::bad_alloc, from the vector itself.
Result getComponentList(const uint8_t *data, size_t len, std::vector<ComponentInfo> &out,
						const ExtractOptions &opts = ExtractOptions());

// Copies the image of the component with ComponentIdentifier == componentId into buffer, excluding
// any signature block, and reports the byte count via *bytesWritten (when non-null). occurrence
// selects among components sharing an identifier, in table order (see ComponentInfo::occurrence).
// On BufferTooSmall nothing is written and *bytesWritten receives the size the buffer must have.
Result extractComponent(const uint8_t *data, size_t len, uint16_t componentId, uint8_t *buffer, size_t bufferSize,
						size_t *bytesWritten, unsigned occurrence = 0,
						const ExtractOptions &opts = ExtractOptions()) noexcept;

// Reads the package-wide header fields.
Result getPackageInfo(const uint8_t *data, size_t len, PackageInfo *out) noexcept;

// Verifies the package's own checksums. *out (when non-null) is filled in on both Success and
// ChecksumMismatch, so a caller can log stored vs computed values.
Result verifyChecksums(const uint8_t *data, size_t len, ChecksumStatus *out) noexcept;

} // namespace pldm_fw

#endif
