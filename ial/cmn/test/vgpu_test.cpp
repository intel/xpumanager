/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __linux__
#define DOCTEST_CONFIG_DISABLE
#endif

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "cmd_vgpu.h"

#include <string>
#include <vector>

std::string progName = "test";

namespace {

struct ArgvFixture
{
	std::vector<std::string> storage;
	std::vector<char *> argv;
	arg_struct args{};

	explicit ArgvFixture(std::initializer_list<const char *> values)
	{
		storage.reserve(values.size());
		argv.reserve(values.size());

		for (const char *value : values) {
			storage.emplace_back(value);
		}

		for (std::string &value : storage) {
			argv.push_back(value.data());
		}

		args.argc = static_cast<int>(argv.size());
		args.argv = argv.data();
	}
};

} // namespace

TEST_CASE("cmdVgpu run returns success for help")
{
	cmdVgpu command;
	ArgvFixture fixture{"xpum", "vgpu", "--help"};

	CHECK(command.run(&fixture.args) == ZE_RESULT_SUCCESS);
}

TEST_CASE("cmdVgpu run rejects create remove and list without a device")
{
	cmdVgpu command;

	SUBCASE("create")
	{
		ArgvFixture fixture{"xpum", "vgpu", "--create"};
		CHECK(command.run(&fixture.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	}

	SUBCASE("remove")
	{
		ArgvFixture fixture{"xpum", "vgpu", "--remove"};
		CHECK(command.run(&fixture.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	}

	SUBCASE("list")
	{
		ArgvFixture fixture{"xpum", "vgpu", "--list"};
		CHECK(command.run(&fixture.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	}
}

TEST_CASE("cmdVgpu run rejects extra positional arguments")
{
	cmdVgpu command;
	ArgvFixture fixture{"xpum", "vgpu", "stray"};

	CHECK(command.run(&fixture.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("cmdVgpu run reports invalid argument when requested device is not found")
{
	cmdVgpu command;
	ArgvFixture fixture{"xpum", "vgpu", "--device", "definitely-not-a-device", "--stats"};

	CHECK(command.run(&fixture.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("cmdVgpu run rejects invalid numeric device ids")
{
	cmdVgpu command;

	SUBCASE("negative device id")
	{
		ArgvFixture fixture{"xpum", "vgpu", "--device", "-1", "--stats"};
		CHECK(command.run(&fixture.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	}

	SUBCASE("out of range device id")
	{
		ArgvFixture fixture{"xpum", "vgpu", "--device", "4294967295", "--stats"};
		CHECK(command.run(&fixture.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	}

	SUBCASE("large device id beyond devices present")
	{
		ArgvFixture fixture{"xpum", "vgpu", "--device", "999999", "--stats"};
		CHECK(command.run(&fixture.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	}
}

TEST_CASE("cmdVgpu --utilization alias matches --stats without GPU target")
{
	cmdVgpu command;
	ArgvFixture canonical{"xpum", "vgpu", "--device", "definitely-not-a-device", "--stats"};
	ArgvFixture alias{"xpum", "vgpu", "--device", "definitely-not-a-device", "--utilization"};

	CHECK(command.run(&canonical.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	CHECK(command.run(&alias.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("cmdVgpu --supported alias matches --list without GPU target")
{
	cmdVgpu command;
	ArgvFixture canonical{"xpum", "vgpu", "--device", "definitely-not-a-device", "--list"};
	ArgvFixture alias{"xpum", "vgpu", "--device", "definitely-not-a-device", "--supported"};

	CHECK(command.run(&canonical.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	CHECK(command.run(&alias.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

TEST_CASE("cmdVgpu --query alias matches --precheck without GPU target")
{
	cmdVgpu command;
	ArgvFixture canonical{"xpum", "vgpu", "--device", "definitely-not-a-device", "--precheck"};
	ArgvFixture alias{"xpum", "vgpu", "--device", "definitely-not-a-device", "--query"};

	CHECK(command.run(&canonical.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
	CHECK(command.run(&alias.args) == ZE_RESULT_ERROR_INVALID_ARGUMENT);
}