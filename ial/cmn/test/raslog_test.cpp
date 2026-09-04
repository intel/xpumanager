/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Option-parsing tests for cmdRasLog::run().
 * These tests exercise argument validation without a real GPU or driver.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#ifdef INFO
#undef INFO
#endif

#include "cmd_raslog.h"
#include <ze_api.h>

std::string progName = "test";

namespace {

struct FakeArgs
{
	std::vector<std::string> storage;
	std::vector<char *> ptrs;
	arg_struct args{};

	explicit FakeArgs(std::initializer_list<const char *> argv_list)
	{
		for (const char *s : argv_list) {
			storage.emplace_back(s);
		}
		for (auto &s : storage) {
			ptrs.push_back(s.data());
		}
		args.argc = static_cast<int>(ptrs.size());
		args.argv = ptrs.data();
	}
};

} // namespace

TEST_SUITE("cmdRasLog::run — argument validation")
{
	TEST_CASE("no arguments → peek is default, passes argument parsing")
	{
		// Peek is the default; --file is not required. Fails past CLI at the driver layer
		// (no real GPU in tests), but the exit code must not be INVALID_ARGUMENT.
		FakeArgs fa{"xpu-smi", "raslog"};
		CHECK(cmdRasLog{}.run(&fa.args) != static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--help flag shows help → ZE_RESULT_SUCCESS")
	{
		FakeArgs fa{"xpu-smi", "raslog", "--help"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("-h flag shows help → ZE_RESULT_SUCCESS")
	{
		FakeArgs fa{"xpu-smi", "raslog", "-h"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("-t cper without --file passes argument parsing")
	{
		// Peek is default; -t cper without --file is valid. Fails at driver layer, not CLI layer.
		FakeArgs fa{"xpu-smi", "raslog", "-t", "cper"};
		CHECK(cmdRasLog{}.run(&fa.args) != static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("unknown --type is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "raslog", "-t", "unknown_type", "-f", "/tmp/out.bin"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--buffer-size-kb 0 is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// Zero is silently treated as "driver default" without validation.
		// The validator rejects it so invalid input cannot change semantics unexpectedly.
		FakeArgs fa{"xpu-smi", "raslog", "-f", "/tmp/out.bin", "--buffer-size-kb", "0"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--buffer-size-kb with non-integer string is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "raslog", "-f", "/tmp/out.bin", "--buffer-size-kb", "bad"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--buffer-size-kb with negative value is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "raslog", "-f", "/tmp/out.bin", "--buffer-size-kb", "-1"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("unknown flag is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "raslog", "--nonexistent"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--peek is rejected (unrecognized flag) → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// --peek was removed; peek is now the default and the flag no longer exists.
		FakeArgs fa{"xpu-smi", "raslog", "--peek"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--peek --drain -f /dev/null is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// --peek is unrecognized; the combination is always rejected.
		FakeArgs fa{"xpu-smi", "raslog", "--peek", "--drain", "-f", "/dev/null"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--drain without --file is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		// Drain consumes records; --file is required so they are not lost.
		FakeArgs fa{"xpu-smi", "raslog", "--drain"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--drain with --file passes argument parsing")
	{
		FakeArgs fa{"xpu-smi", "raslog", "--drain", "-f", "/dev/null"};
		CHECK(cmdRasLog{}.run(&fa.args) != static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("-f /dev/null passes argument parsing")
	{
		// Argument parsing must succeed; the run will fail past the CLI layer because
		// there is no real driver, but the exit code must not be INVALID_ARGUMENT.
		FakeArgs fa{"xpu-smi", "raslog", "-f", "/dev/null"};
		CHECK(cmdRasLog{}.run(&fa.args) != static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("--type cper with -f /dev/null passes argument parsing")
	{
		FakeArgs fa{"xpu-smi", "raslog", "-t", "cper", "-f", "/dev/null"};
		CHECK(cmdRasLog{}.run(&fa.args) != static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}

	TEST_CASE("unknown --type with -f is rejected → ZE_RESULT_ERROR_INVALID_ARGUMENT")
	{
		FakeArgs fa{"xpu-smi", "raslog", "-t", "unknown_type", "-f", "/dev/null"};
		CHECK(cmdRasLog{}.run(&fa.args) == static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT));
	}
}
