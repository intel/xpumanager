/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "logger/sink_base.h"
#include "logger/log_record.h"
#include "logger/log_level.h"
#include "logger/ostream_sink.h"
#include "logger/filestream_sink.h"
#include "logger/logger.h"
#include <mutex>
#include <cstddef>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <memory>
#include <iterator>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
// doctest defines INFO(expr) for test context; undef it so debug.h can define
// INFO(fmt, ...) for log-level gating. We don't use doctest INFO in this file.
#undef INFO

#include "debug.h"
#include "logger/async_sink.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ── Test sinks ────────────────────────────────────────────────────────────────

/**
 * In-memory sink.  emit() is always called under Sink::mutex (via log()), so
 * captured_ is written without additional synchronization.
 */
class CaptureSink final : public Sink
{
	std::string captured;

public:
	void emit(const LogRecord &record) override
	{
		if (record.level == LogLevel::ERR) {
			captured += std::string(record.prefix) + record.loc.file_name() + ':' + std::to_string(record.loc.line()) +
						": " + std::string(record.msg);
		} else {
			captured += std::string(record.prefix);
			captured += std::string(record.msg);
		}
	}

	void sync() noexcept override {}

	[[nodiscard]] const std::string &str() const noexcept { return captured; }
	void clear() noexcept { captured.clear(); }
};

/**
 * Counting sink for thread-safety tests.
 * counter_ is incremented inside emit(), which is called under Sink::mutex,
 * so no separate synchronization is needed.
 */
class CountingSink final : public Sink
{
	int counter = 0;

public:
	void emit(const LogRecord & /*record*/) override { ++counter; }

	void sync() noexcept override {}

	[[nodiscard]] int count() const noexcept { return counter; }
};

// ── RAII helpers ──────────────────────────────────────────────────────────────

/**
 * Saves and restores the active Logger sink and dbgLvl around a test case so
 * that tests remain hermetic even when sharing the process-global Logger.
 */
struct LoggerGuard
{
	std::shared_ptr<Sink> savedSink = Logger::instance().getSink();
	LogLevel savedLevel = getDbgLvl();

	~LoggerGuard()
	{
		Logger::instance().setSink(savedSink);
		setDbgLvl(savedLevel);
	}
};

// ── OStreamSink ───────────────────────────────────────────────────────────────

TEST_CASE("OStreamSink: plain message written with prefix")
{
	std::ostringstream ss;
	OStreamSink s(ss);
	s.log(LogLevel::INFO, {}, "[Info] ", "hello\n");
	CHECK(ss.str() == "[Info] hello\n");
}

TEST_CASE("OStreamSink: empty prefix omitted")
{
	std::ostringstream ss;
	OStreamSink s(ss);
	s.log(LogLevel::TRACE, {}, "", "trace\n");
	CHECK(ss.str() == "trace\n");
}

TEST_CASE("OStreamSink: ERR level does not include file name and line number")
{
	std::ostringstream ss;
	OStreamSink s(ss);
	auto loc = std::source_location::current();
	s.log(LogLevel::ERR, loc, "[Error] ", "oops\n");
	const auto out = ss.str();
	CHECK(out.find("[Error] ") != std::string::npos);
	// Source location must NOT be present
	CHECK(out.find(loc.file_name()) == std::string::npos);
	CHECK(out.find(std::to_string(loc.line())) == std::string::npos);
	CHECK(out.find("oops") != std::string::npos);
}

TEST_CASE("OStreamSink: sync does not throw or crash")
{
	std::ostringstream ss;
	OStreamSink s(ss);
	s.log(LogLevel::INFO, {}, "", "data");
	s.sync();
	CHECK(ss.str() == "data\n");
}

// ── FileStreamSink ────────────────────────────────────────────────────────────

TEST_CASE("FileStreamSink: valid path opens and writes successfully")
{
	const auto tmp = std::filesystem::temp_directory_path() / "xpu_logger_test.log";
	std::filesystem::remove(tmp); // start clean

	{
		FileStreamSink s(tmp.string());
		REQUIRE(s.isOpen());
		s.log(LogLevel::INFO, {}, "[Info] ", "file write test\n");
		s.sync();
	}

	std::ifstream f(tmp);
	REQUIRE(f.is_open());
	std::string line;
	std::getline(f, line);
	CHECK(line.find("file write test") != std::string::npos);

	std::filesystem::remove(tmp);
}

TEST_CASE("FileStreamSink: invalid path yields isOpen() == false")
{
	FileStreamSink s("/no/such/dir/xpu_logger_test.log");
	CHECK(!s.isOpen());
	// emit on closed stream must not crash
	s.log(LogLevel::INFO, {}, "[Info] ", "silent\n");
}

TEST_CASE("FileStreamSink: appends across instances")
{
	const auto tmp = std::filesystem::temp_directory_path() / "xpu_logger_append_test.log";
	std::filesystem::remove(tmp);

	{
		FileStreamSink s(tmp.string());
		s.log(LogLevel::INFO, {}, "", "line1\n");
	}
	{
		FileStreamSink s(tmp.string());
		s.log(LogLevel::INFO, {}, "", "line2\n");
	}

	std::ifstream f(tmp);
	std::string const content((std::istreambuf_iterator<char>(f)), {});
	CHECK(content.find("line1") != std::string::npos);
	CHECK(content.find("line2") != std::string::npos);

	std::filesystem::remove(tmp);
}

// ── Logger sink management ────────────────────────────────────────────────────

TEST_CASE("Logger: setSink replaces the active sink")
{
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	CHECK(Logger::instance().getSink().get() == cap.get());
}

TEST_CASE("Logger: setSink(nullptr) resets to a non-null default")
{
	LoggerGuard const guard;
	Logger::instance().setSink(nullptr);
	CHECK(Logger::instance().getSink() != nullptr);
}

TEST_CASE("Logger: getSink round-trips the same object")
{
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	CHECK(Logger::instance().getSink() == cap);
}

// ── Level gating ──────────────────────────────────────────────────────────────

TEST_CASE("Logger: PRINT always emits regardless of dbgLvl")
{
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::NO_PRINT);

	PRINT("unconditional\n");

	CHECK(cap->str().find("unconditional") != std::string::npos);
}

TEST_CASE("Logger: ERR suppressed when dbgLvl < ERR")
{
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::NO_PRINT);

	ERR("should not appear\n");

	CHECK(cap->str().empty());
}

TEST_CASE("Logger: ERR emits at dbgLvl == ERR")
{
	if constexpr (!detail::DEBUG_ENABLED)
		return;
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::ERR);

	ERR("error msg\n");

	CHECK(cap->str().find("error msg") != std::string::npos);
}

TEST_CASE("Logger: INFO suppressed when dbgLvl < INFO")
{
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::ERR);

	INFO("hidden\n");

	CHECK(cap->str().empty());
}

TEST_CASE("Logger: INFO emits at dbgLvl >= INFO")
{
	if constexpr (!detail::DEBUG_ENABLED)
		return;
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::INFO);

	INFO("visible\n");

	CHECK(cap->str().find("visible") != std::string::npos);
}

TEST_CASE("Logger: DBG suppressed when dbgLvl < DBG")
{
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::INFO);

	DBG("hidden dbg\n");

	CHECK(cap->str().empty());
}

TEST_CASE("Logger: DBG emits at dbgLvl >= DBG")
{
	if constexpr (!detail::DEBUG_ENABLED)
		return;
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::DBG);

	DBG("visible dbg\n");

	CHECK(cap->str().find("visible dbg") != std::string::npos);
}

// ── ERR source location ───────────────────────────────────────────────────────

TEST_CASE("Logger: ERR embeds call-site file name and line number")
{
	if constexpr (!detail::DEBUG_ENABLED)
		return;
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::ERR);

	const unsigned expectedLine = __LINE__ + 1;
	ERR("location test\n");

	const auto &s = cap->str();
	CHECK(s.find("[Error] ") != std::string::npos);
	CHECK(s.find(std::to_string(expectedLine)) != std::string::npos);
	CHECK(s.find("location test") != std::string::npos);
}

// ── Format arguments ──────────────────────────────────────────────────────────

TEST_CASE("Logger: format arguments are substituted correctly")
{
	if constexpr (!detail::DEBUG_ENABLED)
		return;
	LoggerGuard const guard;
	auto cap = std::make_shared<CaptureSink>();
	Logger::instance().setSink(cap);
	setDbgLvl(LogLevel::INFO);

	INFO("val={} name={}\n", 42, "xpu");

	const auto &s = cap->str();
	CHECK(s.find("val=42") != std::string::npos);
	CHECK(s.find("name=xpu") != std::string::npos);
}

// ── Thread safety ─────────────────────────────────────────────────────────────

TEST_CASE("Thread safety: concurrent INFO calls are all received")
{
	if constexpr (!detail::DEBUG_ENABLED)
		return;
	LoggerGuard const guard;

	constexpr int kThreads = 8;
	constexpr int kMsgsPerThread = 250;

	auto counter = std::make_shared<CountingSink>();
	Logger::instance().setSink(counter);
	setDbgLvl(LogLevel::INFO);

	std::vector<std::jthread> threads;
	threads.reserve(kThreads);
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([t] {
			for (int m = 0; m < kMsgsPerThread; ++m) {
				INFO("t={} m={}\n", t, m);
			}
		});
	}
	for (auto &th : threads) {
		th.join();
	}

	// Every message from every thread must arrive exactly once.
	CHECK(counter->count() == kThreads * kMsgsPerThread);
}

TEST_CASE("Thread safety: setSink during concurrent logging does not crash or data-race")
{
	LoggerGuard const guard;

	setDbgLvl(LogLevel::INFO);
	std::atomic<bool> done{false};

	// Swapper: continually replaces the sink while loggers run.
	std::jthread swapper([&done] {
		while (!done.load(std::memory_order_relaxed)) {
			Logger::instance().setSink(std::make_shared<CountingSink>());
		}
	});

	// Logger threads: log while the sink is being swapped beneath them.
	std::vector<std::jthread> loggers;
	loggers.reserve(4);
	for (int t = 0; t < 4; ++t) {
		loggers.emplace_back([t] {
			for (int i = 0; i < 500; ++i) {
				INFO("swap t={} i={}\n", t, i);
			}
		});
	}
	for (auto &th : loggers) {
		th.join();
	}

	done.store(true);
	swapper.join();

	// No crash or sanitizer report = pass
}

TEST_CASE("Thread safety: mixed levels from multiple threads")
{
	if constexpr (!detail::DEBUG_ENABLED)
		return;
	LoggerGuard const guard;

	constexpr int kThreads = 4;
	constexpr int kIter = 100;

	auto counter = std::make_shared<CountingSink>();
	Logger::instance().setSink(counter);
	setDbgLvl(LogLevel::DBG); // allow ERR, INFO, and DBG

	std::vector<std::jthread> threads;
	threads.reserve(kThreads);
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([t] {
			for (int i = 0; i < kIter; ++i) {
				ERR("e t={} i={}\n", t, i);
				INFO("i t={} i={}\n", t, i);
				DBG("d t={} i={}\n", t, i);
			}
		});
	}
	for (auto &th : threads) {
		th.join();
	}

	// 3 messages per iteration per thread
	CHECK(counter->count() == kThreads * kIter * 3);
}

// ── AsyncSink ─────────────────────────────────────────────────────────────────

/**
 * Thread-safe capturing backend for AsyncSink tests.
 * emit() is called on the worker thread under the backend's Sink::mutex, but
 * accessed from the test thread after flushNow() establishes happens-before.
 * The extra mutex guards the vector for tests that read before flushNow().
 */
class AsyncCaptureSink final : public Sink
{
	mutable std::mutex m;
	std::vector<std::string> msgs;

public:
	void emit(const LogRecord &r) override
	{
		std::lock_guard const lock(m);
		msgs.emplace_back(r.msg);
	}
	void sync() noexcept override {}

	[[nodiscard]] std::vector<std::string> captured() const
	{
		std::lock_guard const lock(m);
		return msgs;
	}
	[[nodiscard]] std::size_t count() const
	{
		std::lock_guard const lock(m);
		return msgs.size();
	}
	void clear()
	{
		std::lock_guard const lock(m);
		msgs.clear();
	}
};

/**
 * Backend that blocks inside the first emit() until release() is called.
 *
 * Usage in overflow tests:
 *   1. Create AsyncSink with a PinningSink backend.
 *   2. Log a "primer" message so the worker dequeues it and enters emit().
 *   3. Call waitPinned() — returns only once the worker is inside emit(),
 *      holding no queue lock.  The queue slot the primer occupied is now free.
 *   4. Log the test payloads; overflow fires deterministically because the
 *      worker cannot drain while it is pinned in emit().
 *   5. Call release() then flushNow().
 */
class PinningSink final : public Sink
{
	mutable std::mutex m;
	std::vector<std::string> msgs;
	std::mutex pinM;
	std::condition_variable pinnedCv;
	std::condition_variable releaseCv;
	bool pinned = false;
	bool released = false;

public:
	void emit(const LogRecord &r) override
	{
		{
			std::unique_lock lock(pinM);
			if (!released) {
				pinned = true;
				pinnedCv.notify_all();
				releaseCv.wait(lock, [this] { return released; });
			}
		}
		std::lock_guard const lock(m);
		msgs.emplace_back(r.msg);
	}
	void sync() noexcept override {}

	/** Block the calling thread until the worker is inside emit(). */
	void waitPinned()
	{
		std::unique_lock lock(pinM);
		pinnedCv.wait(lock, [this] { return pinned; });
	}

	/** Allow the worker to continue; all subsequent emit() calls are unlatched. */
	void release()
	{
		std::lock_guard const lock(pinM);
		released = true;
		releaseCv.notify_all();
	}

	[[nodiscard]] std::vector<std::string> captured() const
	{
		std::lock_guard const lock(m);
		return msgs;
	}
};

// Helper: build a minimal LogRecord with just a message string.
[[maybe_unused]] static LogRecord makeRecord(std::string_view msg, LogLevel level = LogLevel::INFO)
{
	return LogRecord{level, {}, "", msg, std::chrono::system_clock::now(), std::this_thread::get_id()};
}

// ── Construction / validation ─────────────────────────────────────────────────

TEST_CASE("AsyncSink: null backend throws invalid_argument")
{
	CHECK_THROWS_AS(AsyncSink(nullptr), std::invalid_argument);
}

TEST_CASE("AsyncSink: zero max_queue_size throws invalid_argument")
{
	auto backend = std::make_shared<AsyncCaptureSink>();
	CHECK_THROWS_AS(AsyncSink(backend, 0), std::invalid_argument);
}

// ── Basic delivery ────────────────────────────────────────────────────────────

TEST_CASE("AsyncSink: single message is delivered to backend")
{
	auto backend = std::make_shared<AsyncCaptureSink>();
	AsyncSink asink(backend);

	asink.log(LogLevel::INFO, {}, "", "hello async\n");
	asink.flushNow();

	const auto msgs = backend->captured();
	REQUIRE(msgs.size() == 1);
	CHECK(msgs[0] == "hello async\n");
}

TEST_CASE("AsyncSink: messages are delivered in FIFO order")
{
	auto backend = std::make_shared<AsyncCaptureSink>();
	AsyncSink asink(backend);

	constexpr int kN = 10;
	for (int i = 0; i < kN; ++i) {
		asink.log(LogLevel::INFO, {}, "", std::to_string(i) + "\n");
	}
	asink.flushNow();

	const auto msgs = backend->captured();
	REQUIRE(static_cast<int>(msgs.size()) == kN);
	for (int i = 0; i < kN; ++i) {
		CHECK(msgs[static_cast<std::size_t>(i)] == std::to_string(i) + "\n");
	}
}

TEST_CASE("AsyncSink: destructor drains queued messages")
{
	auto backend = std::make_shared<AsyncCaptureSink>();

	{
		// Use a long flush_interval so the worker won't drain on its own.
		AsyncSink asink(backend, 8192, AsyncSink::OverflowPolicy::DropOldest, std::chrono::milliseconds(10000));
		for (int i = 0; i < 5; ++i) {
			asink.log(LogLevel::INFO, {}, "", std::to_string(i) + "\n");
		}
		// destructor runs here — must drain all 5 messages
	}

	CHECK(backend->count() == 5);
}

// ── flushNow() ────────────────────────────────────────────────────────────────

TEST_CASE("AsyncSink: flushNow returns only after all prior messages are processed")
{
	auto backend = std::make_shared<AsyncCaptureSink>();
	AsyncSink asink(backend);

	constexpr int kN = 100;
	for (int i = 0; i < kN; ++i) {
		asink.log(LogLevel::INFO, {}, "", "x\n");
	}
	asink.flushNow();

	// Every message must be processed by the time flushNow() returns.
	CHECK(static_cast<int>(backend->count()) == kN);
}

TEST_CASE("AsyncSink: flushNow on empty queue returns immediately")
{
	auto backend = std::make_shared<AsyncCaptureSink>();
	AsyncSink asink(backend);
	asink.flushNow(); // no messages queued — must not block
	CHECK(backend->count() == 0);
}

// ── Statistics ────────────────────────────────────────────────────────────────

TEST_CASE("AsyncSink: stats reflect delivered messages")
{
	auto backend = std::make_shared<AsyncCaptureSink>();
	AsyncSink asink(backend);

	constexpr int kN = 7;
	for (int i = 0; i < kN; ++i) {
		asink.log(LogLevel::INFO, {}, "", "s\n");
	}
	asink.flushNow();

	const auto s = asink.getStats();
	CHECK(s.queued == static_cast<std::uint64_t>(kN));
	CHECK(s.processed == static_cast<std::uint64_t>(kN));
	CHECK(s.dropped == 0);
}

// ── Overflow: DropNewest ──────────────────────────────────────────────────────

TEST_CASE("AsyncSink: DropNewest keeps oldest messages and drops newest")
{
	// PinningSink pins the worker inside emit() while we fill the queue, so
	// overflow fires deterministically.  See PinningSink for the pattern.
	auto backend = std::make_shared<PinningSink>();
	AsyncSink asink(backend, 2, AsyncSink::OverflowPolicy::DropNewest, std::chrono::milliseconds(5000));

	// Pin the worker: primer is dequeued into the worker's local batch then
	// emit() blocks.  The queue slot the primer occupied is now free.
	asink.log(LogLevel::INFO, {}, "", "__primer__\n");
	backend->waitPinned();

	// Queue is empty and worker is blocked.  keep0/keep1 fill the two slots;
	// drop0/drop1 hit DropNewest and are discarded without entering the queue.
	asink.log(LogLevel::INFO, {}, "", "keep0\n");
	asink.log(LogLevel::INFO, {}, "", "keep1\n");
	asink.log(LogLevel::INFO, {}, "", "drop0\n");
	asink.log(LogLevel::INFO, {}, "", "drop1\n");

	backend->release();
	asink.flushNow();

	const auto s = asink.getStats();
	// primer + keep0 + keep1 reached the queue; drop0/drop1 were discarded.
	CHECK(s.queued == 3);
	CHECK(s.dropped == 2);

	const auto msgs = backend->captured();
	REQUIRE(msgs.size() == 3); // primer + keep0 + keep1
	CHECK(msgs[1] == "keep0\n");
	CHECK(msgs[2] == "keep1\n");
}

// ── Overflow: DropOldest ──────────────────────────────────────────────────────

TEST_CASE("AsyncSink: DropOldest keeps newest messages and drops oldest")
{
	auto backend = std::make_shared<PinningSink>();
	AsyncSink asink(backend, 2, AsyncSink::OverflowPolicy::DropOldest, std::chrono::milliseconds(5000));

	// Pin the worker (see DropNewest test for the rationale).
	asink.log(LogLevel::INFO, {}, "", "__primer__\n");
	backend->waitPinned();

	// Queue is empty and worker is blocked.  drop0/drop1 fill the two slots;
	// keep0 evicts drop0, keep1 evicts drop1 — both drops are counted.
	asink.log(LogLevel::INFO, {}, "", "drop0\n");
	asink.log(LogLevel::INFO, {}, "", "drop1\n");
	asink.log(LogLevel::INFO, {}, "", "keep0\n");
	asink.log(LogLevel::INFO, {}, "", "keep1\n");

	backend->release();
	asink.flushNow();

	const auto s = asink.getStats();
	// primer + drop0 + drop1 + keep0 + keep1 all incremented n_queued_;
	// drop0 and drop1 were evicted from the front.
	CHECK(s.queued == 5);
	CHECK(s.dropped == 2);

	const auto msgs = backend->captured();
	REQUIRE(msgs.size() == 3); // primer + keep0 + keep1
	CHECK(msgs[1] == "keep0\n");
	CHECK(msgs[2] == "keep1\n");
}

// ── Thread safety ─────────────────────────────────────────────────────────────

TEST_CASE("AsyncSink: concurrent emits from multiple threads all delivered")
{
	auto backend = std::make_shared<AsyncCaptureSink>();
	AsyncSink asink(backend);

	constexpr int kThreads = 8;
	constexpr int kPerThread = 200;

	std::vector<std::jthread> threads;
	threads.reserve(kThreads);
	for (int t = 0; t < kThreads; ++t) {
		threads.emplace_back([&asink, t] {
			for (int i = 0; i < kPerThread; ++i) {
				asink.log(LogLevel::INFO, {}, "", std::to_string(t) + ":" + std::to_string(i) + "\n");
			}
		});
	}
	for (auto &th : threads) {
		th.join();
	}

	asink.flushNow();

	CHECK(backend->count() == static_cast<std::size_t>(kThreads * kPerThread));

	const auto s = asink.getStats();
	CHECK(s.queued == static_cast<std::uint64_t>(kThreads * kPerThread));
	CHECK(s.processed == static_cast<std::uint64_t>(kThreads * kPerThread));
	CHECK(s.dropped == 0);
}

TEST_CASE("AsyncSink: multiple flushNow() calls are safe")
{
	auto backend = std::make_shared<AsyncCaptureSink>();
	AsyncSink asink(backend);

	for (int i = 0; i < 5; ++i) {
		asink.log(LogLevel::INFO, {}, "", "m\n");
	}
	asink.flushNow();
	asink.flushNow(); // second call on already-drained queue
	CHECK(backend->count() == 5);
}

TEST_CASE("AsyncSink: via Logger integration")
{
	if constexpr (!detail::DEBUG_ENABLED)
		return;
	LoggerGuard const guard;

	auto backend = std::make_shared<AsyncCaptureSink>();
	auto asink = std::make_shared<AsyncSink>(backend);
	Logger::instance().setSink(asink);
	setDbgLvl(LogLevel::INFO);

	INFO("async via Logger\n");

	asink->flushNow();

	const auto msgs = backend->captured();
	REQUIRE(!msgs.empty());
	CHECK(msgs.back().find("async via Logger") != std::string::npos);
}
