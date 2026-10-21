/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for stdoutSize(), run against a pseudo-terminal so the size is known.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "terminal.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

/// A pseudo-terminal of a chosen size, and stdout pointed at it while the object lives.
class PseudoTerminalStdout
{
	int master = -1;
	int slave = -1;
	int savedStdout = -1;

public:
	PseudoTerminalStdout(unsigned short rows, unsigned short columns)
	{
		master = posix_openpt(O_RDWR | O_NOCTTY);
		REQUIRE(master >= 0);
		REQUIRE(grantpt(master) == 0);
		REQUIRE(unlockpt(master) == 0);
		const char *name = ptsname(master);
		REQUIRE(name != nullptr);
		slave = open(name, O_RDWR | O_NOCTTY);
		REQUIRE(slave >= 0);
		winsize size{};
		size.ws_row = rows;
		size.ws_col = columns;
		REQUIRE(ioctl(master, TIOCSWINSZ, &size) == 0);
		savedStdout = dup(STDOUT_FILENO);
		REQUIRE(savedStdout >= 0);
		REQUIRE(dup2(slave, STDOUT_FILENO) >= 0);
	}

	~PseudoTerminalStdout()
	{
		dup2(savedStdout, STDOUT_FILENO);
		close(savedStdout);
		close(slave);
		close(master);
	}

	PseudoTerminalStdout(const PseudoTerminalStdout &) = delete;
	PseudoTerminalStdout &operator=(const PseudoTerminalStdout &) = delete;
	PseudoTerminalStdout(PseudoTerminalStdout &&) = delete;
	PseudoTerminalStdout &operator=(PseudoTerminalStdout &&) = delete;
};

} // namespace

TEST_CASE("stdoutSize reads the window of the terminal stdout is attached to")
{
	std::optional<TerminalSize> seen;
	{
		const PseudoTerminalStdout terminal{42, 133};
		seen = stdoutSize();
	}
	REQUIRE(seen.has_value());
	CHECK(seen->rows == 42U);
	CHECK(seen->columns == 133U);
}

TEST_CASE("stdoutSize is unknown when the terminal reports an empty window")
{
	std::optional<TerminalSize> seen;
	{
		const PseudoTerminalStdout terminal{0, 0};
		seen = stdoutSize();
	}
	CHECK_FALSE(seen.has_value());
}

TEST_CASE("stdoutSize is unknown when stdout is not a terminal")
{
	const int saved = dup(STDOUT_FILENO);
	REQUIRE(saved >= 0);
	const int sink = open("/dev/null", O_WRONLY);
	REQUIRE(sink >= 0);
	REQUIRE(dup2(sink, STDOUT_FILENO) >= 0);
	const auto seen = stdoutSize();
	dup2(saved, STDOUT_FILENO);
	close(saved);
	close(sink);
	CHECK_FALSE(seen.has_value());
}
