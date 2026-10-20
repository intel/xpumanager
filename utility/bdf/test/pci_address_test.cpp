/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for PciAddress: parsing, canonical formatting and ordering.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "pci_address.h"

#include <set>
#include <string>

TEST_CASE("parse reads each field as a number")
{
	const auto address = PciAddress::parse("0001:3a:1f.7");
	REQUIRE(address.has_value());
	CHECK(address->domain == 0x1);
	CHECK(address->bus == 0x3a);
	CHECK(address->device == 0x1f);
	CHECK(address->function == 7);
}

TEST_CASE("toString is canonical: lower case, domain of at least four digits")
{
	CHECK(PciAddress{0, 3, 0, 0}.toString() == "0000:03:00.0");
	CHECK(PciAddress{0x10000, 1, 0, 0}.toString() == "10000:01:00.0");
	CHECK(PciAddress{0xabcdef12, 0xff, 0x1f, 6}.toString() == "abcdef12:ff:1f.6");
}

TEST_CASE("every spelling of an address parses to the same value")
{
	const auto canonical = PciAddress::parse("0000:3a:0f.7");
	REQUIRE(canonical.has_value());
	CHECK(PciAddress::parse("0000:3A:0F.7") == canonical);
	CHECK(PciAddress::parse("00000:3a:0f.7") == canonical);
	CHECK(PciAddress::parse("00000000:3a:0f.7") == canonical);
	CHECK(PciAddress::parse("00000000:3A:0F.7")->toString() == "0000:3a:0f.7");
}

TEST_CASE("parse accepts domains of four to eight digits")
{
	CHECK(PciAddress::parse("10000:01:00.0")->domain == 0x10000);
	CHECK(PciAddress::parse("ffffffff:01:00.0")->domain == 0xffffffff);
	CHECK_FALSE(PciAddress::parse("000:03:00.0").has_value());
	CHECK_FALSE(PciAddress::parse("123456789:03:00.0").has_value());
}

TEST_CASE("parse rejects a device or function out of range")
{
	CHECK(PciAddress::parse("0000:00:1f.7").has_value());
	CHECK_FALSE(PciAddress::parse("0000:00:20.0").has_value());
	CHECK_FALSE(PciAddress::parse("0000:00:00.8").has_value());
	CHECK_FALSE(PciAddress::parse("0000:00:ff.f").has_value());
}

TEST_CASE("parse rejects anything that is not a PCI address")
{
	CHECK_FALSE(PciAddress::parse("").has_value());
	CHECK_FALSE(PciAddress::parse("0000:03:00:0").has_value());
	CHECK_FALSE(PciAddress::parse("0000.03:00.0").has_value());
	CHECK_FALSE(PciAddress::parse("0000:0g:00.0").has_value());
	CHECK_FALSE(PciAddress::parse("-000:03:00.0").has_value());
	CHECK_FALSE(PciAddress::parse("+000:03:00.0").has_value());
	CHECK_FALSE(PciAddress::parse("0x00:03:00.0").has_value());
	CHECK_FALSE(PciAddress::parse(" 0000:03:00.0").has_value());
	CHECK_FALSE(PciAddress::parse("../../etc/x").has_value());
	CHECK_FALSE(PciAddress::parse("0000:03:00.0/").has_value());
}

TEST_CASE("addresses order by domain, bus, device, function")
{
	const PciAddress a{0, 3, 0, 0};
	const PciAddress b{0, 3, 0, 1};
	const PciAddress c{1, 0, 0, 0};
	CHECK(a < b);
	CHECK(b < c);
	CHECK(a == PciAddress{0, 3, 0, 0});
	const std::set<PciAddress> unique{c, a, b, a};
	CHECK(unique.size() == 3);
	CHECK(*unique.begin() == a);
}

TEST_CASE("{} formats an address canonically")
{
	CHECK(xpum::compat::format("{}", PciAddress{0, 0x3a, 0, 0}) == "0000:3a:00.0");
	CHECK(xpum::compat::format("[{}]", PciAddress{0x10000, 1, 2, 3}) == "[10000:01:02.3]");
}
