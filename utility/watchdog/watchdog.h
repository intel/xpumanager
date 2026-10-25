/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef UTILITY_WATCHDOG_WATCHDOG_H
#define UTILITY_WATCHDOG_WATCHDOG_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <ratio>
#include <span>
#include <stop_token>
#include <string_view>
#include <thread>

/*
 * Process-wide hang watchdog.
 *
 * A blocked Level Zero call cannot be cancelled, so the only cure for a hung driver is to end
 * the process. One Watchdog lives in main(); progress anywhere in the process (keepalive(),
 * sleepFor(), sleepUntil(), entering or leaving a Phase) pushes a single shared deadline out by
 * the timeout. When the deadline passes, the expiry handler runs on the watchdog thread.
 *
 * Invariants:
 * - The Watchdog outlives every thread that calls the free functions below. xpu-smi joins every
 *   worker before a command returns, so a Watchdog in main() satisfies this.
 * - This library is linked into exactly one module (the executable). The registration pointer
 *   lives in watchdog.cpp, so a second module would see "no watchdog armed".
 * - The deadline is shared: while one thread is in sleepFor(d), a hang in another thread is
 *   detected up to d later. Sampling intervals are short, so this is accepted.
 */
namespace xpum::watchdog {

using Clock = std::chrono::steady_clock;

inline constexpr auto DEFAULT_TIMEOUT = std::chrono::seconds{30};
inline constexpr auto MAX_TIMEOUT = std::chrono::seconds{86400};
inline constexpr int TIMEOUT_EXIT_CODE = 124; // same as coreutils timeout(1)

// Share of the timeout without progress after which the user is told an abort is coming
// (the pretimeout of the Linux watchdog API).
using PretimeoutFraction = std::ratio<1, 2>;

/**
 * @brief Returns how long without progress the watchdog waits before the pretimeout warning.
 *
 * Timeouts of 2 s and more are rounded down to whole seconds so the warning reads "15 s" rather
 * than "15500 ms" for odd timeouts.
 *
 * @param[in] timeout  The watchdog timeout.
 *
 * @retval duration  Time from the last progress to the pretimeout; zero when @p timeout is zero.
 */
[[nodiscard]] constexpr std::chrono::milliseconds pretimeoutAfter(std::chrono::milliseconds timeout) noexcept
{
	const std::chrono::milliseconds share = timeout * PretimeoutFraction::num / PretimeoutFraction::den;
	if (timeout >= std::chrono::seconds{2}) {
		return std::chrono::floor<std::chrono::seconds>(share);
	}
	return share;
}

/**
 * @brief Name of what the process is doing, reported when the watchdog fires.
 *
 * Only string literals are accepted: the pointer is printed on the watchdog thread after the
 * scope that set it may have gone, so it must refer to static storage.
 */
class PhaseName
{
public:
	/**
	 * @brief Wraps a string literal.
	 * @param[in] literal  A string literal.
	 */
	// Implicit so call sites read enter("driver init"); the array reference is what limits
	// this to literals.
	template <std::size_t N>
	consteval PhaseName(const char (&literal)[N]) // NOLINT(*-explicit-*,*-avoid-c-arrays)
		: text(literal)
	{}

	/**
	 * @brief Returns the wrapped literal.
	 * @retval text  Null-terminated, static storage.
	 */
	[[nodiscard]] constexpr const char *str() const noexcept { return text; }

private:
	const char *text;
};

enum class ExpiryKind : std::uint8_t
{
	Pretimeout, // no progress for pretimeoutAfter(timeout); once per stall, handler returns
	Timeout,	// no progress for the full timeout; the handler must not return in production
};

struct Expiry
{
	ExpiryKind kind;
	const char *phase; // from PhaseName, never null
	std::chrono::milliseconds timeout;
	// Pretimeout: time without progress; Timeout: == timeout.
	std::chrono::milliseconds elapsed;
	// Pretimeout: time left before Timeout; zero otherwise.
	std::chrono::milliseconds remaining;
};

// Runs on the watchdog thread while another thread may be hung holding the heap, stdio or
// logger locks: it must not allocate, lock or use stdio.
using ExpiryHandler = void (*)(const Expiry &) noexcept;

class Watchdog;

/**
 * @brief RAII guard that names the current phase; copy and move are deleted.
 *
 * Obtained from enter() or suspend(). Entering and leaving both count as progress.
 */
class [[nodiscard]] Phase
{
public:
	~Phase();
	Phase(const Phase &) = delete;
	Phase &operator=(const Phase &) = delete;
	Phase(Phase &&) = delete;
	Phase &operator=(Phase &&) = delete;

private:
	friend Phase enter(PhaseName name);
	friend Phase suspend(PhaseName name);
	Phase(Watchdog *armed, PhaseName phaseName, bool suspendsWatchdog) noexcept;

	Watchdog *owner;
	const char *name;
	const char *previous{nullptr};
	bool suspends;
};

/**
 * @brief Owner of the watchdog thread; one per process, constructed in main().
 *
 * A timeout of zero disables the watchdog: no thread is started and the free functions stay
 * no-ops. A second instance while one is armed stays inert and never fires.
 */
class Watchdog
{
public:
	/**
	 * @brief Arms the watchdog.
	 * @param[in] timeoutValue  Time without progress before @p handler gets a Timeout; 0 = disabled.
	 * @param[in] handler       Called on the watchdog thread for every Expiry.
	 */
	Watchdog(std::chrono::milliseconds timeoutValue, ExpiryHandler handler);
	~Watchdog();
	Watchdog(const Watchdog &) = delete;
	Watchdog &operator=(const Watchdog &) = delete;
	Watchdog(Watchdog &&) = delete; // the thread holds `this`
	Watchdog &operator=(Watchdog &&) = delete;

private:
	friend class Phase;
	friend void keepalive() noexcept;
	friend void sleepFor(Clock::duration duration);
	friend void sleepUntil(Clock::time_point when);

	void run(const std::stop_token &stop);
	void extendTo(Clock::time_point when) noexcept; // only ever moves the deadline later
	void report(ExpiryKind kind, std::chrono::milliseconds elapsed, std::chrono::milliseconds remaining) noexcept;

	std::atomic<Clock::time_point> deadline;
	std::atomic<const char *> phase{"startup"};
	// Count of open suspend() phases, plus FIRING while a Timeout is being reported. suspend()
	// waits for FIRING to clear, so flashing never starts once the abort is committed.
	std::atomic<std::uint32_t> suspendState{0};
	std::chrono::milliseconds timeout;
	std::chrono::milliseconds pretimeoutAt; // since the last progress
	ExpiryHandler onExpiry;
	std::mutex mutex;
	std::condition_variable_any wake;
	std::jthread thread;
};
static_assert(std::atomic<Clock::time_point>::is_always_lock_free);

/**
 * @brief Records progress: pushes the deadline to now + timeout. Safe from any thread.
 */
void keepalive() noexcept;

/**
 * @brief Sleeps like std::this_thread::sleep_for without the sleep counting as a stall.
 * @param[in] duration  How long to sleep.
 */
void sleepFor(Clock::duration duration);

/**
 * @brief Sleeps like std::this_thread::sleep_until without the sleep counting as a stall.
 * @param[in] when  When to wake.
 */
void sleepUntil(Clock::time_point when);

/**
 * @brief Names the current phase until the returned guard is destroyed.
 * @param[in] name  Phase name, reported if the watchdog fires.
 * @retval Phase  Guard; restores the previous phase name on destruction.
 */
[[nodiscard]] Phase enter(PhaseName name);

/**
 * @brief Like enter(), but the watchdog neither warns nor times out until the guard ends.
 *
 * For work that must never be interrupted and may legitimately take minutes, such as writing
 * firmware. Leaving the phase counts as progress, so a full timeout starts again afterwards.
 * If the watchdog has already committed to a timeout, this waits for the expiry handler, which
 * in production ends the process, so the guarded work never starts.
 *
 * @param[in] name  Phase name.
 * @retval Phase  Guard; ends the suspension on destruction.
 */
[[nodiscard]] Phase suspend(PhaseName name);

/**
 * @brief Formats the one-line report for @p expiry into @p buf, without allocating.
 *
 * Durations are printed in whole seconds when they are a whole number of seconds, else in ms.
 * Output that does not fit is truncated but still ends with a newline.
 *
 * @param[in]  expiry   What happened.
 * @param[in]  command  What xpu-smi was asked to do; omitted from the text when empty.
 * @param[out] buf      Destination.
 *
 * @retval text  View into @p buf; empty only when @p buf is empty.
 */
[[nodiscard]] std::string_view formatMessage(const Expiry &expiry, std::string_view command,
											 std::span<char> buf) noexcept;

} // namespace xpum::watchdog

#endif // UTILITY_WATCHDOG_WATCHDOG_H
