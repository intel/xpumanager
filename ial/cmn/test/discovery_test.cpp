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
