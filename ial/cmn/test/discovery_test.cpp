/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "gfx_firmware_status.h"
#include "printer.h"

#include <string>

TEST_CASE("normal GFX firmware status requires a readable version")
{
	CHECK(discovery::resolveGfxFirmwareStatus("", "normal") == "unknown");
	CHECK(discovery::resolveGfxFirmwareStatus("", "Success") == "unknown");
	CHECK(discovery::resolveGfxFirmwareStatus("   ", "normal") == "unknown");
	CHECK(discovery::resolveGfxFirmwareStatus("DG02_1.2.3", "normal") == "normal");
	CHECK(discovery::resolveGfxFirmwareStatus("DG02_1.2.3", "Success") == "normal");
}

TEST_CASE("GFX firmware errors remain visible when the version is unavailable")
{
	CHECK(discovery::resolveGfxFirmwareStatus("", "Firmware recovery mode") == "Firmware recovery mode");
	CHECK(discovery::resolveGfxFirmwareStatus("DG02_1.2.3", "Firmware recovery mode") == "Firmware recovery mode");
}

TEST_CASE("empty GFX firmware status is unknown")
{
	CHECK(discovery::resolveGfxFirmwareStatus("", "") == "unknown");
	CHECK(discovery::resolveGfxFirmwareStatus("DG02_1.2.3", "") == "unknown");
}

TEST_CASE("JSON output replaces bytes that are not UTF-8 instead of failing")
{
	// The AMC version string of a damaged reply: "1.2.7" and an MCTP header byte
	nlohmann::ordered_json json;
	json["amc_firmware_version"] = std::string("1.2.7\x81");
	std::string text;
	CHECK_NOTHROW(text = jsonText(json, 4));
	CHECK(text.find("1.2.7\xEF\xBF\xBD") != std::string::npos); // U+FFFD
	CHECK(jsonText(nlohmann::ordered_json{{"version", "1.2.7.0"}}) == R"({"version":"1.2.7.0"})");
}
