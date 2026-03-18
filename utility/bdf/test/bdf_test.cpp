/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "bdf.h"

TEST_CASE("Valid canonical BDF addresses") {
	CHECK(isValidBdf("0000:4d:00.0") == true);
	CHECK(isValidBdf("0000:00:01.0") == true);
	CHECK(isValidBdf("ffff:ff:ff.f") == true);
	CHECK(isValidBdf("FFFF:FF:FF.F") == true);
	CHECK(isValidBdf("AbCd:eF:01.a") == true);
}

TEST_CASE("Invalid BDF - wrong length") {
	CHECK(isValidBdf("") == false);
	CHECK(isValidBdf("0000:4d:00.00") == false);
	CHECK(isValidBdf("0000:4d:00.") == false);
	CHECK(isValidBdf("000:4d:00.0") == false);
}

TEST_CASE("Invalid BDF - wrong separators") {
	CHECK(isValidBdf("0000-4d:00.0") == false);
	CHECK(isValidBdf("0000:4d-00.0") == false);
	CHECK(isValidBdf("0000:4d:00:0") == false);
	CHECK(isValidBdf("0000.4d.00:0") == false);
}

TEST_CASE("Invalid BDF - non-hex characters") {
	CHECK(isValidBdf("000g:4d:00.0") == false);
	CHECK(isValidBdf("0000:4x:00.0") == false);
	CHECK(isValidBdf("0000:4d:0z.0") == false);
	CHECK(isValidBdf("0000:4d:00.g") == false);
}

TEST_CASE("Invalid BDF - special / adversarial inputs") {
	CHECK(isValidBdf("../.:..:.0.0") == false);
	CHECK(isValidBdf("000/../00.00") == false);
	CHECK(isValidBdf("            ") == false);
}
