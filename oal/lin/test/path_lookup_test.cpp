/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the $PATH executable lookup helper (isExecutableInPath).
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "lin.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

// Restores $PATH and the working directory when it goes out of scope, so a
// test that rewrites them cannot leak state into the next one.
class EnvGuard
{
	std::string savedPath;
	bool hadPath;
	fs::path savedCwd;

public:
	EnvGuard() : hadPath(false), savedCwd(fs::current_path())
	{
		if (const char *p = std::getenv("PATH")) {
			savedPath = p;
			hadPath = true;
		}
	}
	~EnvGuard()
	{
		if (hadPath) {
			setenv("PATH", savedPath.c_str(), 1);
		} else {
			unsetenv("PATH");
		}
		std::error_code ec;
		fs::current_path(savedCwd, ec);
	}
	EnvGuard(const EnvGuard &) = delete;
	EnvGuard &operator=(const EnvGuard &) = delete;
};

// Create a unique, empty scratch directory for one test case.
fs::path makeScratchDir(const std::string &tag)
{
	fs::path dir = fs::temp_directory_path() / ("xpum_path_lookup_" + tag + "_" + std::to_string(::getpid()));
	std::error_code ec;
	fs::remove_all(dir, ec);
	fs::create_directories(dir);
	return dir;
}

// Write a file and optionally mark it owner-executable.
void writeFile(const fs::path &p, bool executable)
{
	std::ofstream(p) << "#!/bin/sh\n";
	fs::perms perms = fs::perms::owner_read | fs::perms::owner_write;
	if (executable) {
		perms |= fs::perms::owner_exec;
	}
	fs::permissions(p, perms);
}

} // namespace

TEST_CASE("empty name is never found")
{
	EnvGuard guard;
	CHECK_FALSE(isExecutableInPath(""));
}

TEST_CASE("executable on PATH is found, missing name is not")
{
	EnvGuard guard;
	const fs::path dir = makeScratchDir("found");
	writeFile(dir / "mytool", /*executable=*/true);
	setenv("PATH", dir.c_str(), 1);

	CHECK(isExecutableInPath("mytool"));
	CHECK_FALSE(isExecutableInPath("definitely_absent_tool"));

	fs::remove_all(dir);
}

TEST_CASE("a non-executable regular file on PATH is not a match")
{
	EnvGuard guard;
	const fs::path dir = makeScratchDir("nonexec");
	writeFile(dir / "data", /*executable=*/false);
	setenv("PATH", dir.c_str(), 1);

	CHECK_FALSE(isExecutableInPath("data"));

	fs::remove_all(dir);
}

TEST_CASE("a directory whose name matches is not a match")
{
	EnvGuard guard;
	const fs::path dir = makeScratchDir("dirmatch");
	// Directories carry the execute (search) bit, so the regular-file guard is
	// what must reject this, not the access(X_OK) check.
	fs::create_directory(dir / "toolshaped");
	setenv("PATH", dir.c_str(), 1);

	CHECK_FALSE(isExecutableInPath("toolshaped"));

	fs::remove_all(dir);
}

TEST_CASE("symlinks are resolved to their target")
{
	EnvGuard guard;
	const fs::path dir = makeScratchDir("symlink");
	writeFile(dir / "realtool", /*executable=*/true);

	// A symlink to a regular executable is a match (both access(X_OK) and
	// is_regular_file follow the link to the target).
	fs::create_symlink("realtool", dir / "link_to_tool");
	// A symlink to a directory must still be rejected by the regular-file guard.
	fs::create_directory(dir / "realdir");
	fs::create_symlink("realdir", dir / "link_to_dir");
	// A dangling symlink resolves to nothing and is not a match.
	fs::create_symlink("does_not_exist", dir / "link_dangling");
	setenv("PATH", dir.c_str(), 1);

	CHECK(isExecutableInPath("link_to_tool"));
	CHECK_FALSE(isExecutableInPath("link_to_dir"));
	CHECK_FALSE(isExecutableInPath("link_dangling"));

	fs::remove_all(dir);
}

TEST_CASE("the first matching directory across PATH fields wins")
{
	EnvGuard guard;
	const fs::path first = makeScratchDir("multi_a");
	const fs::path second = makeScratchDir("multi_b");
	writeFile(second / "onlyhere", /*executable=*/true);
	setenv("PATH", (first.string() + ":" + second.string()).c_str(), 1);

	CHECK(isExecutableInPath("onlyhere"));

	fs::remove_all(first);
	fs::remove_all(second);
}

TEST_CASE("an empty PATH field means the current directory")
{
	EnvGuard guard;
	const fs::path dir = makeScratchDir("cwdfield");
	writeFile(dir / "cwdtool", /*executable=*/true);
	fs::current_path(dir);
	// Leading empty field, then a directory that does not contain the tool.
	const fs::path other = makeScratchDir("cwdfield_other");
	setenv("PATH", (":" + other.string()).c_str(), 1);

	CHECK(isExecutableInPath("cwdtool"));

	fs::remove_all(dir);
	fs::remove_all(other);
}

TEST_CASE("a name containing a slash is tested as a path, not searched")
{
	EnvGuard guard;
	const fs::path dir = makeScratchDir("slash");
	writeFile(dir / "direct", /*executable=*/true);
	writeFile(dir / "plain", /*executable=*/false);
	// PATH is irrelevant for a slashed name; point it somewhere empty.
	const fs::path empty = makeScratchDir("slash_empty");
	setenv("PATH", empty.c_str(), 1);

	// A directory carries the search (execute) bit; the regular-file guard must
	// still reject it when named as a direct path.
	fs::create_directory(dir / "subdir");

	CHECK(isExecutableInPath((dir / "direct").string()));
	CHECK_FALSE(isExecutableInPath((dir / "plain").string()));
	CHECK_FALSE(isExecutableInPath((dir / "missing").string()));
	CHECK_FALSE(isExecutableInPath((dir / "subdir").string()));

	fs::remove_all(dir);
	fs::remove_all(empty);
}

TEST_CASE("no PATH set means nothing is found by name")
{
	EnvGuard guard;
	unsetenv("PATH");
	CHECK_FALSE(isExecutableInPath("sh"));
}
