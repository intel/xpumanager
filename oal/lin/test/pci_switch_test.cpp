/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Tests for the PCIe switch name detection predicate (nameIndicatesSwitch)
 * defined in pci_switch_name.h.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "pci_switch_name.h"

TEST_CASE("nameIndicatesSwitch: names with trailing space after Switch")
{
	CHECK(nameIndicatesSwitch("PEX Switch with DMA") == true);
	CHECK(nameIndicatesSwitch("Broadcom PCI Switch") == true);
	CHECK(nameIndicatesSwitch("PLX Technology PCI Express Switch") == true);
}

TEST_CASE("nameIndicatesSwitch: names ending exactly with Switch")
{
	CHECK(nameIndicatesSwitch("PEX Gen 5 Switch") == true);
	CHECK(nameIndicatesSwitch("PCIe Gen4 Switch") == true);
}

TEST_CASE("nameIndicatesSwitch: names with Switch followed by revision")
{
	CHECK(nameIndicatesSwitch("PEX Switch[rev 10]") == true);
	CHECK(nameIndicatesSwitch("PEX Switch [rev 10]") == true);
}

TEST_CASE("nameIndicatesSwitch: case-insensitive matching")
{
	CHECK(nameIndicatesSwitch("PEX SWITCH") == true);
	CHECK(nameIndicatesSwitch("pex switch") == true);
	CHECK(nameIndicatesSwitch("PeX sWiTcH") == true);
}

TEST_CASE("nameIndicatesSwitch: skips false prefix and finds later match")
{
	CHECK(nameIndicatesSwitch("Foo Switcheroo PCI Switch") == true);
}

TEST_CASE("nameIndicatesSwitch: names that must NOT match")
{
	CHECK(nameIndicatesSwitch("Switched Hub") == false);
	CHECK(nameIndicatesSwitch("Switcheroo") == false);
	CHECK(nameIndicatesSwitch("SwitchNIC") == false);
	CHECK(nameIndicatesSwitch("") == false);
	CHECK(nameIndicatesSwitch("No match here") == false);
}
