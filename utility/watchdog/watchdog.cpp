/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "watchdog.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
#include <system_error>

namespace xpum::watchdog {

namespace {

// Defined here rather than as an inline variable in the header: an inline variable gets one copy
// per shared module, and a module with its own copy would never see the armed instance.
constinit std::atomic<Watchdog *> active{nullptr}; // NOLINT(*-avoid-non-const-global-variables)

// Set in Watchdog::suspendState from the moment the watchdog commits to a Timeout; the low bits
// count the open suspend() phases.
constexpr std::uint32_t FIRING = 1U << 31;

// Appends into a fixed buffer, keeping the last byte for the newline so truncated text still
// ends a line.
class LineWriter
{
public:
	explicit LineWriter(std::span<char> dest) noexcept : buf(dest) {}

	void put(std::string_view text) noexcept
	{
		const std::size_t room = capacity() - used;
		const std::size_t n = std::min(room, text.size());
		std::copy_n(text.data(), n, buf.data() + used);
		used += n;
	}

	void put(std::chrono::milliseconds duration) noexcept
	{
		const bool wholeSeconds = duration.count() % 1000 == 0 && duration.count() != 0;
		std::array<char, std::numeric_limits<long long>::digits10 + 2> digits{};
		const auto value = wholeSeconds ? duration.count() / 1000 : duration.count();
		const auto [end, ec] = std::to_chars(digits.data(), digits.data() + digits.size(), value);
		if (ec == std::errc{}) {
			put(std::string_view{digits.data(), static_cast<std::size_t>(end - digits.data())});
		}
		put(wholeSeconds ? " s" : " ms");
	}

	[[nodiscard]] std::string_view finish() noexcept
	{
		if (buf.empty()) {
			return {};
		}
		buf[used++] = '\n';
		return {buf.data(), used};
	}

private:
	[[nodiscard]] std::size_t capacity() const noexcept { return buf.empty() ? 0 : buf.size() - 1; }

	std::span<char> buf;
	std::size_t used{0};
};

} // namespace

/**
 * @brief Arms the watchdog, unless @p timeoutValue is zero or another instance is already armed.
 *
 * @param[in] timeoutValue  Time without progress before @p handler gets a Timeout; 0 = disabled.
 * @param[in] handler       Called on the watchdog thread for every Expiry.
 */
Watchdog::Watchdog(std::chrono::milliseconds timeoutValue, ExpiryHandler handler)
	: deadline(Clock::now() + timeoutValue), timeout(timeoutValue), pretimeoutAt(pretimeoutAfter(timeoutValue)),
	  onExpiry(handler)
{
	if (timeout <= std::chrono::milliseconds::zero()) {
		return;
	}
	// Started in the body, not the initializer list, so every member exists whatever the
	// declaration order.
	thread = std::jthread{[this](const std::stop_token &stop) { run(stop); }};
	// Registered last so the free functions never see a half-built instance. A second instance
	// must not take over: guards from the first would then update the wrong deadline.
	Watchdog *expected = nullptr;
	if (!active.compare_exchange_strong(expected, this)) {
		thread.request_stop();
		thread.join();
	}
}

/**
 * @brief Disarms the watchdog and joins its thread.
 */
Watchdog::~Watchdog()
{
	Watchdog *self = this;
	active.compare_exchange_strong(self, nullptr);
	// Joined in the body, before any member is destroyed; leaving it to the jthread destructor
	// would only be safe while `thread` is the last member.
	thread.request_stop();
	if (thread.joinable()) {
		thread.join();
	}
}

/**
 * @brief Moves the deadline to @p when, unless it is already later.
 *
 * Never shortening is what lets concurrent workers call keepalive() without one undoing a
 * longer extension made by a sleeping thread.
 *
 * @param[in] when  Proposed deadline.
 */
void Watchdog::extendTo(Clock::time_point when) noexcept
{
	Clock::time_point current = deadline.load(std::memory_order_relaxed);
	while (current < when && !deadline.compare_exchange_weak(current, when, std::memory_order_relaxed)) {
	}
}

/**
 * @brief Calls the expiry handler for the current phase.
 *
 * @param[in] kind       What happened.
 * @param[in] elapsed    See Expiry::elapsed.
 * @param[in] remaining  See Expiry::remaining.
 */
void Watchdog::report(ExpiryKind kind, std::chrono::milliseconds elapsed, std::chrono::milliseconds remaining) noexcept
{
	onExpiry(Expiry{
		.kind = kind,
		.phase = phase.load(),
		.timeout = timeout,
		.elapsed = elapsed,
		.remaining = remaining,
	});
}

/**
 * @brief Watchdog thread: waits for the pretimeout and the deadline, reports each.
 *
 * The deadline minus the timeout is the time of the last progress, so the pretimeout is due at
 * that point plus pretimeoutAt. It is reported once per deadline value; any progress moves the
 * deadline and so allows a new warning for the next stall. While suspended, the
 * deadline is re-armed silently.
 *
 * A Timeout is committed by setting FIRING in suspendState only while no suspend() phase is
 * open; a suspend() that starts after that waits for the handler, which in production ends the
 * process. Deciding from a plain load instead would let a firmware write begin between the load
 * and the abort.
 *
 * @param[in] stop  Stop request from the destructor; wakes the wait.
 */
void Watchdog::run(const std::stop_token &stop)
{
	std::unique_lock lock{mutex};
	Clock::time_point warnedFor{};
	while (!stop.stop_requested()) {
		const Clock::time_point now = Clock::now();
		const Clock::time_point due = deadline.load(std::memory_order_relaxed);
		const bool suspended = suspendState.load() != 0;

		if (now >= due) {
			std::uint32_t idle = 0;
			if (suspended || !suspendState.compare_exchange_strong(idle, FIRING)) {
				extendTo(now + timeout);
				continue;
			}
			// In production the handler ends the process; a test handler returns, and the
			// thread must then stop rather than report the same timeout again.
			report(ExpiryKind::Timeout, timeout, std::chrono::milliseconds::zero());
			suspendState.fetch_and(~FIRING);
			suspendState.notify_all();
			return;
		}

		const Clock::time_point warnAt = due - timeout + pretimeoutAt;
		const bool warnPending = !suspended && warnedFor != due;
		if (warnPending && now >= warnAt) {
			report(ExpiryKind::Pretimeout, pretimeoutAt, timeout - pretimeoutAt);
			warnedFor = due;
			continue;
		}
		// Progress only moves the deadline later, so waking at the old value is never late; the
		// loop then re-reads it. Nothing notifies the condition variable except the stop token.
		wake.wait_until(lock, stop, warnPending ? warnAt : due, [] { return false; });
	}
}

/**
 * @brief Enters a phase on @p armed, or does nothing when @p armed is null.
 *
 * @param[in] armed      The armed watchdog, or null.
 * @param[in] phaseName  Phase name.
 * @param[in] suspendsWatchdog  True for a phase that suspends the watchdog.
 */
Phase::Phase(Watchdog *armed, PhaseName phaseName, bool suspendsWatchdog) noexcept
	: owner(armed), name(phaseName.str()), suspends(suspendsWatchdog)
{
	if (owner == nullptr) {
		return;
	}
	if (suspends) {
		// Counted first: from here on the watchdog cannot commit to a Timeout, and one it
		// committed to earlier is waited out before the caller starts its work. Before the
		// phase name changes, so that Timeout still names the phase that stalled.
		std::uint32_t state = owner->suspendState.fetch_add(1) + 1;
		while ((state & FIRING) != 0) {
			owner->suspendState.wait(state);
			state = owner->suspendState.load();
		}
	}
	previous = owner->phase.exchange(name);
	owner->extendTo(Clock::now() + owner->timeout);
}

/**
 * @brief Restores the previous phase name, unless another thread has entered a phase since.
 */
Phase::~Phase()
{
	if (owner == nullptr) {
		return;
	}
	// Compare-exchange so a phase entered concurrently on another thread keeps its name.
	const char *expected = name;
	owner->phase.compare_exchange_strong(expected, previous);
	if (suspends) {
		owner->suspendState.fetch_sub(1);
	}
	owner->extendTo(Clock::now() + owner->timeout);
}

/**
 * @brief Records progress: pushes the deadline to now + timeout.
 */
void keepalive() noexcept
{
	if (Watchdog *dog = active.load(); dog != nullptr) {
		dog->extendTo(Clock::now() + dog->timeout);
	}
}

/**
 * @brief Sleeps for @p duration with the deadline extended past the end of the sleep.
 *
 * @param[in] duration  How long to sleep.
 */
void sleepFor(Clock::duration duration)
{
	if (Watchdog *dog = active.load(); dog != nullptr) {
		dog->extendTo(Clock::now() + duration + dog->timeout);
	}
	std::this_thread::sleep_for(duration);
}

/**
 * @brief Sleeps until @p when with the deadline extended past the end of the sleep.
 *
 * @param[in] when  When to wake.
 */
void sleepUntil(Clock::time_point when)
{
	if (Watchdog *dog = active.load(); dog != nullptr) {
		dog->extendTo(std::max(when, Clock::now()) + dog->timeout);
	}
	std::this_thread::sleep_until(when);
}

/**
 * @brief Names the current phase until the returned guard is destroyed.
 *
 * @param[in] name  Phase name.
 *
 * @retval Phase  Guard; a no-op when no watchdog is armed.
 */
Phase enter(PhaseName name) { return Phase{active.load(), name, false}; }

/**
 * @brief Names a phase during which the watchdog neither warns nor times out.
 *
 * @param[in] name  Phase name.
 *
 * @retval Phase  Guard; a no-op when no watchdog is armed.
 */
Phase suspend(PhaseName name) { return Phase{active.load(), name, true}; }

/**
 * @brief Formats the one-line report for @p expiry into @p buf, without allocating.
 *
 * @param[in]  expiry   What happened.
 * @param[in]  command  What xpu-smi was asked to do; omitted when empty.
 * @param[out] buf      Destination.
 *
 * @retval text  View into @p buf, ending in a newline; empty only when @p buf is empty.
 */
std::string_view formatMessage(const Expiry &expiry, std::string_view command, std::span<char> buf) noexcept
{
	LineWriter out{buf};
	const auto putCommand = [&out, command] {
		if (!command.empty()) {
			out.put(" while running '");
			out.put(command);
			out.put("'");
		}
	};
	switch (expiry.kind) {
	case ExpiryKind::Pretimeout:
		out.put("[Warning] xpu-smi: no progress for ");
		out.put(expiry.elapsed);
		out.put(" in phase '");
		out.put(expiry.phase);
		out.put("'");
		putCommand();
		out.put("; aborting in ");
		out.put(expiry.remaining);
		out.put(" unless it completes (see --timeout).");
		break;
	case ExpiryKind::Timeout:
		out.put("[Error] xpu-smi: no progress for ");
		out.put(expiry.elapsed);
		out.put(" in phase '");
		out.put(expiry.phase);
		out.put("'");
		putCommand();
		out.put("; aborting (exit 124).");
		break;
	}
	return out.finish();
}

} // namespace xpum::watchdog
