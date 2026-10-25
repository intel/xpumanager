/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "terminal.h"
#include "watchdog_os.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/mman.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace {

// Points @p fd at an in-memory file for the guard's lifetime and returns what was written.
class Capture
{
public:
	explicit Capture(int fd) : target(fd), saved(::dup(fd)), file(::memfd_create("capture", MFD_CLOEXEC))
	{
		REQUIRE(saved >= 0);
		REQUIRE(file >= 0);
		std::fflush(nullptr);
		REQUIRE(::dup2(file, target) == target);
	}
	~Capture()
	{
		restore();
		::close(file);
		::close(saved);
	}
	Capture(const Capture &) = delete;
	Capture &operator=(const Capture &) = delete;
	Capture(Capture &&) = delete;
	Capture &operator=(Capture &&) = delete;

	[[nodiscard]] std::string text()
	{
		std::fflush(nullptr);
		restore();
		std::string out;
		std::array<char, 4096> buf{};
		::lseek(file, 0, SEEK_SET);
		for (ssize_t n = 0; (n = ::read(file, buf.data(), buf.size())) > 0;) {
			out.append(buf.data(), static_cast<std::size_t>(n));
		}
		return out;
	}

private:
	void restore()
	{
		if (!restored) {
			::dup2(saved, target);
			restored = true;
		}
	}

	int target;
	int saved;
	int file;
	bool restored{false};
};

[[nodiscard]] std::string threadLine(pid_t tid) { return "  thread " + std::to_string(tid) + " state "; }

} // namespace

TEST_CASE("writeStderrRaw writes exactly the bytes given")
{
	Capture err{STDERR_FILENO};
	writeStderrRaw(std::string_view{"a\0b\n", 4});
	CHECK(err.text() == std::string{"a\0b\n", 4});
}

TEST_CASE("writeThreadReport lists every thread with its state")
{
	std::mutex mutex;
	std::condition_variable cv;
	bool started = false;
	bool done = false;
	pid_t workerTid = 0;
	std::jthread worker{[&] {
		std::unique_lock lock{mutex};
		workerTid = ::gettid();
		started = true;
		cv.notify_all();
		cv.wait(lock, [&] { return done; });
	}};
	{
		std::unique_lock lock{mutex};
		cv.wait(lock, [&] { return started; });
	}

	Capture err{STDERR_FILENO};
	writeThreadReport();
	const std::string report = err.text();
	{
		std::scoped_lock lock{mutex};
		done = true;
	}
	cv.notify_all();

	CAPTURE(report);
	CHECK(report.find(threadLine(::gettid())) != std::string::npos);
	const auto workerLine = report.find(threadLine(workerTid));
	REQUIRE(workerLine != std::string::npos);
	// Blocked on the condition variable: sleeping, in a futex wait.
	CHECK(report.substr(workerLine + threadLine(workerTid).size(), 1) == "S");
	CHECK(report.find(" wchan ", workerLine) != std::string::npos);
}

TEST_CASE("a write to a pipe nobody drains gives up instead of blocking")
{
	std::array<int, 2> fds{};
	REQUIRE(::pipe2(fds.data(), O_CLOEXEC) == 0);
	const int savedErr = ::dup(STDERR_FILENO);
	REQUIRE(savedErr >= 0);
	REQUIRE(::dup2(fds[1], STDERR_FILENO) == STDERR_FILENO);
	// Fill the pipe through a non-blocking copy of the write end.
	const int filler = ::open("/proc/self/fd/2", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
	REQUIRE(filler >= 0);
	const std::array<char, 4096> junk{};
	while (::write(filler, junk.data(), junk.size()) > 0) {
	}
	::close(filler);

	const auto start = std::chrono::steady_clock::now();
	writeStderrRaw("lost\n");
	const auto firstWait = std::chrono::steady_clock::now() - start;
	writeThreadReport();
	const auto total = std::chrono::steady_clock::now() - start;

	// Drained, the stream takes writes again.
	std::array<char, 65536> drain{};
	REQUIRE(::fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
	while (::read(fds[0], drain.data(), drain.size()) > 0) {
	}
	writeStderrRaw("back\n");
	std::array<char, 16> got{};
	const ssize_t n = ::read(fds[0], got.data(), got.size());

	::dup2(savedErr, STDERR_FILENO);
	::close(savedErr);
	::close(fds[0]);
	::close(fds[1]);

	CHECK(firstWait >= std::chrono::milliseconds{200});
	// The thread report does not wait again for every line.
	CHECK(total < std::chrono::seconds{2});
	REQUIRE(n == 5);
	CHECK(std::string_view{got.data(), 5} == "back\n");
}

TEST_CASE("leaveAlternateScreenForExit restores the screen once")
{
	Capture out{STDOUT_FILENO};
	{
		AlternateScreen screen{true};
		leaveAlternateScreenForExit();
		leaveAlternateScreenForExit();
	}
	CHECK(out.text() == "\033[?1049h\033[?25h\033[?1049l");
}

TEST_CASE("a disabled guard writes nothing")
{
	Capture out{STDOUT_FILENO};
	{
		AlternateScreen screen{false};
		leaveAlternateScreenForExit();
	}
	CHECK(out.text().empty());
}

TEST_CASE("a screen entered with the caller's own output is restored once it is noted")
{
	Capture out{STDOUT_FILENO};
	noteAlternateScreen(true);
	leaveAlternateScreenForExit();
	leaveAlternateScreenForExit();
	noteAlternateScreen(true);
	noteAlternateScreen(false);
	leaveAlternateScreenForExit();
	CHECK(out.text() == "\033[?25h\033[?1049l");
}

TEST_CASE("restoreInputModeForExit turns line input and echo back on only after getch changed them")
{
	const int master = ::posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
	REQUIRE(master >= 0);
	REQUIRE(::grantpt(master) == 0);
	REQUIRE(::unlockpt(master) == 0);
	const int terminal = ::open(::ptsname(master), O_RDWR | O_NOCTTY | O_CLOEXEC);
	REQUIRE(terminal >= 0);
	const int savedIn = ::dup(STDIN_FILENO);
	REQUIRE(savedIn >= 0);
	REQUIRE(::dup2(terminal, STDIN_FILENO) == STDIN_FILENO);

	const auto setRaw = [] {
		termios mode{};
		REQUIRE(::tcgetattr(STDIN_FILENO, &mode) == 0);
		mode.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
		REQUIRE(::tcsetattr(STDIN_FILENO, TCSANOW, &mode) == 0);
	};
	const auto lineInputAndEcho = [] {
		termios mode{};
		REQUIRE(::tcgetattr(STDIN_FILENO, &mode) == 0);
		return (mode.c_lflag & (ICANON | ECHO)) == (ICANON | ECHO);
	};

	setRaw();
	restoreInputModeForExit();
	const bool untouchedWhenNotNoted = !lineInputAndEcho();

	noteRawInput(true);
	restoreInputModeForExit();
	const bool restored = lineInputAndEcho();

	setRaw();
	restoreInputModeForExit();
	const bool onlyOnce = !lineInputAndEcho();

	::dup2(savedIn, STDIN_FILENO);
	::close(savedIn);
	::close(terminal);
	::close(master);

	CHECK(untouchedWhenNotNoted);
	CHECK(restored);
	CHECK(onlyOnce);
}
