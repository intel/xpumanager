/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

/**
 * @file offline_pages_test.cpp
 * @brief Doctest-based unit tests for OfflinePagesTextPrinter (cmd_stats.cpp).
 *
 * Regression coverage for GSD-13132: the offline-pages output used printf-style
 * conversion specifiers ("%u", "0x%X", "%s") while ERR()/PRINT() are backed by
 * std::format, which copies such specifiers through verbatim and drops the
 * arguments. Every message in this path therefore printed placeholder text
 * instead of values, and PRINT("%s", table...) dropped the whole table body.
 *
 * These tests render the printer against hand-built JSON, so no GPU is needed.
 *
 * Covered:
 *  - print(): header substitutes device index / BDF / count (no "%" or "{}" left)
 *  - print(): populated page list emits a table containing every address and size
 *  - print(): zero-count and missing/empty "offline_pages" emit the empty notice
 *  - print(): array input renders one section per device
 *  - print(): nullptr input is a no-op
 *  - print(): an annotated failure entry never renders a bogus page table
 *  - annotateOfflinePagesFailure(): "supported" only for UNSUPPORTED_FEATURE; error
 *    name + hex code for every failure; success leaves the entry untouched
 *  - summarizeOfflinePages(): exit status is the first failure and SUCCESS only when
 *    all devices succeed; the unsupported notice is emitted once, not per device
 */

// The clang-tidy CI step builds its compilation database from a default-options
// build, which excludes test targets, so doctest's include path is absent there and
// the file would fail to parse. Guard the body so that configuration lints an empty
// translation unit instead of reporting a hard error.
#if __has_include(<doctest/doctest.h>)

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// INFO may be defined by ze_api / level-zero headers; doctest also defines it.
// Undefine before including project headers to avoid the macro clash.
#ifdef INFO
#undef INFO
#endif

// progName is an extern defined by the CLI executable; provide a stub for tests.
#include <string>
std::string progName = "test";

#include "cmd_stats.h"
#include "utility/compat/format.h"
#include "utility/logger/log_record.h"
#include "utility/logger/logger.h"
#include "utility/logger/sink_base.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <ze_api.h>

namespace {

/**
 * @brief In-memory sink capturing every message body.
 *
 * emit() runs under Sink::mutex (via Sink::log()), so `captured` needs no
 * additional synchronization. Installing this via Logger::setSink() also
 * redirects PRINT(), which is what the printer under test uses.
 */
class CaptureSink final : public Sink
{
	std::string captured;

public:
	void emit(const LogRecord &record) override
	{
		captured += std::string(record.prefix);
		captured += std::string(record.msg);
	}

	void sync() noexcept override {}

	[[nodiscard]] const std::string &str() const noexcept { return captured; }
};

/**
 * @brief Renders `json` through OfflinePagesTextPrinter and returns the output.
 *
 * Resets the logger to its default sinks afterwards (setSink(nullptr)) so tests
 * stay independent of each other.
 */
std::string render(nlohmann::ordered_json json)
{
	auto sink = std::make_shared<CaptureSink>();
	Logger::instance().setSink(sink);

	OfflinePagesTextPrinter printer;
	printer.print(&json);

	Logger::instance().setSink(nullptr);
	return sink->str();
}

/// Address stride between consecutive synthetic offline pages (one 4 KiB page).
constexpr uint64_t PAGE_ADDRESS_STRIDE = 0x1000ULL;

/// Size reported for the first synthetic page; later pages are multiples of it.
constexpr uint32_t PAGE_SIZE_BYTES = 4096U;

/// Arbitrary non-zero offline_page_count used when the offline_pages array is absent.
constexpr uint32_t PAGE_COUNT_WITHOUT_ARRAY = 7U;

/// Builds one device entry with `count` synthetic offline pages.
nlohmann::ordered_json makeDevice(uint32_t index, const char *bdf, uint32_t count)
{
	nlohmann::ordered_json device;
	device["device_index"] = index;
	device["pci_bdf"] = bdf;
	device["offline_page_count"] = count;
	device["offline_pages"] = nlohmann::ordered_json::array();
	for (uint32_t i = 0; i < count; ++i) {
		nlohmann::ordered_json page;
		page["address"] = xpum::compat::format("0x{:016X}", PAGE_ADDRESS_STRIDE * (i + 1));
		page["size"] = PAGE_SIZE_BYTES * (i + 1);
		device["offline_pages"].push_back(page);
	}
	return device;
}

} // namespace

// --- Header line -------------------------------------------------------------

TEST_CASE("OfflinePagesTextPrinter: header substitutes index, BDF and count")
{
	const std::string out = render(makeDevice(3, "0000:04:00.0", 2));

	CHECK(out.find("Device 3 (0000:04:00.0) - Offline Memory Pages: 2") != std::string::npos);
}

TEST_CASE("OfflinePagesTextPrinter: output leaves no unsubstituted placeholders")
{
	const std::string out = render(makeDevice(0, "0000:03:00.0", 3));

	// GSD-13132: printf-style specifiers were emitted literally by std::format.
	CHECK(out.find('%') == std::string::npos);
	// A stray "{}" would mean an argument was not supplied.
	CHECK(out.find("{}") == std::string::npos);
}

// --- Table body --------------------------------------------------------------

TEST_CASE("OfflinePagesTextPrinter: table header is rendered")
{
	// PRINT("%s", table...) previously discarded the entire table.
	const std::string out = render(makeDevice(1, "0000:04:00.0", 3));

	CHECK(out.find("Address") != std::string::npos);
	CHECK(out.find("Size (bytes)") != std::string::npos);
}

TEST_CASE("OfflinePagesTextPrinter: every page address is rendered")
{
	const nlohmann::ordered_json device = makeDevice(1, "0000:04:00.0", 3);
	const std::string out = render(device);

	for (const auto &page : device["offline_pages"]) {
		CHECK(out.find(page["address"].get<std::string>()) != std::string::npos);
	}
}

TEST_CASE("OfflinePagesTextPrinter: every page size is rendered")
{
	const nlohmann::ordered_json device = makeDevice(1, "0000:04:00.0", 3);
	const std::string out = render(device);

	for (const auto &page : device["offline_pages"]) {
		CHECK(out.find(std::to_string(page["size"].get<uint32_t>())) != std::string::npos);
	}
}

// --- Empty / degenerate inputs ----------------------------------------------

TEST_CASE("OfflinePagesTextPrinter: zero count reports no offline pages")
{
	const std::string out = render(makeDevice(0, "0000:04:00.0", 0));

	CHECK(out.find("No offline memory pages found.") != std::string::npos);
	CHECK(out.find("Offline Memory Pages: 0") != std::string::npos);
}

TEST_CASE("OfflinePagesTextPrinter: missing offline_pages key reports no pages")
{
	nlohmann::ordered_json device;
	device["device_index"] = 2;
	device["pci_bdf"] = "0000:05:00.0";
	// A non-zero count with no page array: the printer must trust the array, not the count.
	device["offline_page_count"] = PAGE_COUNT_WITHOUT_ARRAY;

	const std::string out = render(device);

	CHECK(out.find("No offline memory pages found.") != std::string::npos);
}

TEST_CASE("OfflinePagesTextPrinter: absent fields fall back to defaults")
{
	const std::string out = render(nlohmann::ordered_json::object());

	CHECK(out.find("Device 0 (N/A) - Offline Memory Pages: 0") != std::string::npos);
	CHECK(out.find("No offline memory pages found.") != std::string::npos);
}

// --- Multi-device ------------------------------------------------------------

TEST_CASE("OfflinePagesTextPrinter: array input renders one section per device")
{
	nlohmann::ordered_json devices = nlohmann::ordered_json::array();
	devices.push_back(makeDevice(0, "0000:03:00.0", 1));
	devices.push_back(makeDevice(1, "0000:04:00.0", 0));

	const std::string out = render(devices);

	CHECK(out.find("Device 0 (0000:03:00.0) - Offline Memory Pages: 1") != std::string::npos);
	CHECK(out.find("Device 1 (0000:04:00.0) - Offline Memory Pages: 0") != std::string::npos);
	CHECK(out.find('%') == std::string::npos);
}

// --- Failure annotations (shape produced by cmdStats::run) -------------------

/**
 * @brief Builds the failure entry cmdStats::run attaches when the HAL reports
 *        an unsupported/failed query, so JSON consumers see every device.
 */
static nlohmann::ordered_json makeUnsupportedDevice(uint32_t index, const char *bdf)
{
	nlohmann::ordered_json device;
	device["device_index"] = index;
	device["pci_bdf"] = bdf;
	device["supported"] = false;
	device["error"] = "ZE_RESULT_ERROR_UNSUPPORTED_FEATURE";
	device["error_code"] = "0x78000003";
	return device;
}

TEST_CASE("OfflinePagesTextPrinter: unsupported entry renders no page table")
{
	// run() does not print these in text mode, but rendering one must never
	// invent a table or leak a placeholder if it ever is printed.
	const std::string out = render(makeUnsupportedDevice(0, "0000:04:00.0"));

	CHECK(out.find("No offline memory pages found.") != std::string::npos);
	CHECK(out.find("Size (bytes)") == std::string::npos);
	CHECK(out.find('%') == std::string::npos);
	CHECK(out.find("{}") == std::string::npos);
}

TEST_CASE("OfflinePagesTextPrinter: mixed array of ok and unsupported devices")
{
	nlohmann::ordered_json devices = nlohmann::ordered_json::array();
	devices.push_back(makeDevice(0, "0000:03:00.0", 2));
	devices.push_back(makeUnsupportedDevice(1, "0000:04:00.0"));

	const std::string out = render(devices);

	CHECK(out.find("Device 0 (0000:03:00.0) - Offline Memory Pages: 2") != std::string::npos);
	CHECK(out.find("Device 1 (0000:04:00.0) - Offline Memory Pages: 0") != std::string::npos);
	CHECK(out.find('%') == std::string::npos);
}

// --- annotateOfflinePagesFailure ---------------------------------------------

TEST_CASE("annotateOfflinePagesFailure: unsupported marks supported=false with code")
{
	nlohmann::ordered_json device;
	device["device_index"] = 0;
	device["pci_bdf"] = "0000:04:00.0";

	cmdStats::annotateOfflinePagesFailure(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device);

	REQUIRE(device.contains("supported"));
	CHECK(device["supported"].get<bool>() == false);
	CHECK(device["error"].get<std::string>() == "ZE_RESULT_ERROR_UNSUPPORTED_FEATURE");
	CHECK(device["error_code"].get<std::string>() == "0x78000003");
}

TEST_CASE("annotateOfflinePagesFailure: device identity survives annotation")
{
	// The identity must survive, otherwise JSON consumers cannot attribute the entry.
	nlohmann::ordered_json device;
	device["device_index"] = 0;
	device["pci_bdf"] = "0000:04:00.0";

	cmdStats::annotateOfflinePagesFailure(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, device);

	CHECK(device["device_index"].get<uint32_t>() == 0);
	CHECK(device["pci_bdf"].get<std::string>() == "0000:04:00.0");
}

TEST_CASE("annotateOfflinePagesFailure: a genuine fault does not claim unsupported")
{
	// A lost device says nothing about whether the feature exists; asserting
	// "supported": false here would misreport a transient fault as a capability gap.
	nlohmann::ordered_json device;
	cmdStats::annotateOfflinePagesFailure(ZE_RESULT_ERROR_DEVICE_LOST, device);

	CHECK_FALSE(device.contains("supported"));
	CHECK(device["error"].get<std::string>() == "ZE_RESULT_ERROR_DEVICE_LOST");
	CHECK(device["error_code"].get<std::string>() == "0x70000001");
}

TEST_CASE("annotateOfflinePagesFailure: success is a no-op")
{
	nlohmann::ordered_json device;
	device["offline_page_count"] = 0;

	cmdStats::annotateOfflinePagesFailure(ZE_RESULT_SUCCESS, device);

	CHECK_FALSE(device.contains("supported"));
	CHECK_FALSE(device.contains("error"));
	CHECK_FALSE(device.contains("error_code"));
	CHECK(device.size() == 1); // success output shape must not gain fields
}

// --- summarizeOfflinePages ---------------------------------------------------

TEST_CASE("summarizeOfflinePages: all devices succeed -> success, no notice")
{
	const OfflinePagesOutcome out = cmdStats::summarizeOfflinePages({ZE_RESULT_SUCCESS, ZE_RESULT_SUCCESS});

	CHECK(out.exitResult == ZE_RESULT_SUCCESS);
	CHECK(out.summaryMessage.empty());
}

TEST_CASE("summarizeOfflinePages: all unsupported exits non-zero")
{
	// GSD-13132: this previously returned SUCCESS (exit 0) and printed two lines per device.
	const OfflinePagesOutcome out =
		cmdStats::summarizeOfflinePages({ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
										 ZE_RESULT_ERROR_UNSUPPORTED_FEATURE});

	CHECK(out.exitResult == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	CHECK(out.exitResult != ZE_RESULT_SUCCESS); // must map to a non-zero process exit
}

TEST_CASE("summarizeOfflinePages: all unsupported reports exactly one notice")
{
	const OfflinePagesOutcome out =
		cmdStats::summarizeOfflinePages({ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE,
										 ZE_RESULT_ERROR_UNSUPPORTED_FEATURE});

	CHECK(out.summaryMessage.find("not supported on this device or driver") != std::string::npos);
	CHECK(out.summaryMessage.find("0x78000003") != std::string::npos);
	// Exactly one notice, regardless of how many devices reported unsupported.
	CHECK(std::count(out.summaryMessage.begin(), out.summaryMessage.end(), '\n') == 1);
	CHECK(out.summaryMessage.find('%') == std::string::npos);
}

TEST_CASE("summarizeOfflinePages: single unsupported device still reports once")
{
	const OfflinePagesOutcome out = cmdStats::summarizeOfflinePages({ZE_RESULT_ERROR_UNSUPPORTED_FEATURE});

	CHECK(out.exitResult == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	CHECK(out.summaryMessage.find("not supported on this device or driver") != std::string::npos);
	CHECK(std::count(out.summaryMessage.begin(), out.summaryMessage.end(), '\n') == 1);
}

TEST_CASE("summarizeOfflinePages: partial support reports the affected count")
{
	const OfflinePagesOutcome out =
		cmdStats::summarizeOfflinePages({ZE_RESULT_SUCCESS, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, ZE_RESULT_SUCCESS,
										 ZE_RESULT_ERROR_UNSUPPORTED_FEATURE});

	// Partial data is still incomplete data, so the exit status must not be success.
	CHECK(out.exitResult == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
	CHECK(out.summaryMessage.find("2 of 4 selected devices") != std::string::npos);
	CHECK(out.summaryMessage.find("this device or driver") == std::string::npos);
}

TEST_CASE("summarizeOfflinePages: exit status is the FIRST failure encountered")
{
	const OfflinePagesOutcome out =
		cmdStats::summarizeOfflinePages({ZE_RESULT_ERROR_DEVICE_LOST, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE});

	CHECK(out.exitResult == ZE_RESULT_ERROR_DEVICE_LOST);
	// A real fault mixed in must not be presented as a blanket capability gap.
	CHECK(out.summaryMessage.find("1 of 2 selected devices") != std::string::npos);
}

TEST_CASE("summarizeOfflinePages: a non-unsupported failure yields no capability notice")
{
	const OfflinePagesOutcome out = cmdStats::summarizeOfflinePages({ZE_RESULT_ERROR_DEVICE_LOST});

	CHECK(out.exitResult == ZE_RESULT_ERROR_DEVICE_LOST);
	CHECK(out.summaryMessage.empty()); // that device was reported individually instead
}

TEST_CASE("summarizeOfflinePages: empty input is success with no notice")
{
	// run() rejects an empty device list earlier; guard the boundary so a future
	// refactor cannot turn it into a spurious "not supported" claim or a 0/0 message.
	const OfflinePagesOutcome out = cmdStats::summarizeOfflinePages({});

	CHECK(out.exitResult == ZE_RESULT_SUCCESS);
	CHECK(out.summaryMessage.empty());
}

TEST_CASE("OfflinePagesTextPrinter: nullptr input is a no-op")
{
	auto sink = std::make_shared<CaptureSink>();
	Logger::instance().setSink(sink);

	OfflinePagesTextPrinter printer;
	printer.print(nullptr);

	Logger::instance().setSink(nullptr);

	CHECK(sink->str().empty());
}

#endif // __has_include(<doctest/doctest.h>)
