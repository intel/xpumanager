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

TEST_CASE("buildVgpuListJson emits an empty vgpu_list array for no VFs")
{
	nlohmann::ordered_json result = buildVgpuListJson({});

	CHECK(result.contains("vgpu_list"));
	CHECK(result["vgpu_list"].is_array());
	CHECK(result["vgpu_list"].empty());
}

TEST_CASE("buildVgpuListJson serializes function type, bdf and raw memory bytes")
{
	DeviceSriovInfo physical{};
	physical.bdfAddress = "0000:4d:00.0";
	physical.functionType = DEVICE_FUNCTION_TYPE_PHYSICAL;
	physical.vGpuMemorySize = 0;

	DeviceSriovInfo virt{};
	virt.bdfAddress = "0000:4d:00.1";
	virt.functionType = DEVICE_FUNCTION_TYPE_VIRTUAL;
	virt.vGpuMemorySize = 512ULL * ONE_MB_IN_BYTES;

	nlohmann::ordered_json result = buildVgpuListJson({physical, virt});

	REQUIRE(result["vgpu_list"].size() == 2);

	const auto &first = result["vgpu_list"][0];
	CHECK(first["bdf_address"] == "0000:4d:00.0");
	CHECK(first["function_type"] == "Physical");
	CHECK(first["memory_physical_size_byte"] == 0);

	const auto &second = result["vgpu_list"][1];
	CHECK(second["bdf_address"] == "0000:4d:00.1");
	CHECK(second["function_type"] == "Virtual");
	// Memory is reported in raw bytes, not MiB, so callers can format as needed.
	CHECK(second["memory_physical_size_byte"] == 512ULL * ONE_MB_IN_BYTES);
}

TEST_CASE("buildVgpuListJson treats unknown function type as Physical")
{
	DeviceSriovInfo unknown{};
	unknown.bdfAddress = "0000:4d:00.2";
	unknown.functionType = DEVICE_FUNCTION_TYPE_UNKNOWN;
	unknown.vGpuMemorySize = 0;

	nlohmann::ordered_json result = buildVgpuListJson({unknown});

	REQUIRE(result["vgpu_list"].size() == 1);
	CHECK(result["vgpu_list"][0]["function_type"] == "Physical");
}

TEST_CASE("buildVgpuListJson preserves entry ordering and keys")
{
	DeviceSriovInfo a{};
	a.bdfAddress = "0000:00:02.0";
	a.functionType = DEVICE_FUNCTION_TYPE_PHYSICAL;

	DeviceSriovInfo b{};
	b.bdfAddress = "0000:00:02.1";
	b.functionType = DEVICE_FUNCTION_TYPE_VIRTUAL;

	nlohmann::ordered_json result = buildVgpuListJson({a, b});

	// Insertion order of the array must match the input order.
	CHECK(result["vgpu_list"][0]["bdf_address"] == "0000:00:02.0");
	CHECK(result["vgpu_list"][1]["bdf_address"] == "0000:00:02.1");

	// Each entry must expose exactly the documented keys.
	for (const auto &entry : result["vgpu_list"]) {
		CHECK(entry.size() == 3);
		CHECK(entry.contains("bdf_address"));
		CHECK(entry.contains("function_type"));
		CHECK(entry.contains("memory_physical_size_byte"));
	}
}