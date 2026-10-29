/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef UTILITY_CIRCUIT_BREAKER_CIRCUIT_BREAKER_H
#define UTILITY_CIRCUIT_BREAKER_CIRCUIT_BREAKER_H

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

namespace xpum {

/**
 * @brief A call that can run on another thread and be abandoned there.
 *
 * It is moved to a worker thread that may outlive the caller, so it must own everything it
 * touches: capture handles and inputs by value, and return its outputs instead of writing
 * through captured references or pointers. It returns a value, not void, so the caller can
 * tell an answer from an abandoned call.
 */
template <typename F>
concept AbandonableCall =
	std::invocable<F &> && std::move_constructible<std::decay_t<F>> && !std::is_void_v<std::invoke_result_t<F &>> &&
	!std::is_reference_v<std::invoke_result_t<F &>> && std::move_constructible<std::invoke_result_t<F &>>;

/** What a CircuitBreaker observer is told. */
enum class BreakerEvent
{
	Tripped, ///< A call missed the limit; the breaker is open and the group's calls are not made.
	Reset,	 ///< The abandoned call returned; the breaker is closed and the group's calls are made again.
};

/**
 * @brief Runs a group of calls that can block forever on one worker thread, each with a time
 *        limit.
 *
 * Some driver calls never return when the driver or firmware below them stops answering, and
 * the thread that made them cannot be interrupted. A breaker runs the calls of one group, one at
 * a time and in order, on its own worker thread; the caller waits at most the limit from the
 * moment its call starts.
 *
 * A call that misses the limit is abandoned on the worker and trips the breaker open: the calls
 * queued behind it and every later call fail fast without running, since they would wait on the
 * same worker, and calls that share the state that blocked would block as well. If the abandoned
 * call returns after all, the breaker resets (closes) and runs calls again.
 *
 * This is the circuit breaker stability pattern, with the abandoned call's return as the probe
 * that closes it, and a bulkhead: each group's calls have a worker of their own.
 *
 * Use one breaker per group of calls that share such state, e.g. one for every RAS call of the
 * process. The worker starts on the first call. A breaker destroyed while a call is still running
 * leaves its worker behind rather than wait for it.
 *
 * Thread-safe.
 */
class CircuitBreaker
{
public:
	using Clock = std::chrono::steady_clock;
	/// Told of each trip and reset, on the thread that saw it; must not block. A plain function:
	/// it may run after the breaker is gone, so it cannot depend on objects destroyed before then.
	using Observer = void(std::string_view group, std::chrono::milliseconds limit, BreakerEvent event) noexcept;

	/**
	 * @brief Creates a breaker for one group of calls.
	 *
	 * @param[in] name     The group, for messages, e.g. "RAS".
	 * @param[in] limit    How long each call may take once it has started.
	 * @param[in] observer Told of trips and resets; may be null.
	 */
	CircuitBreaker(std::string_view name, std::chrono::milliseconds limit, Observer *observer = nullptr)
		: shared(std::make_shared<Shared>(std::string{name}, limit, observer))
	{}

	CircuitBreaker(const CircuitBreaker &) = delete;
	CircuitBreaker &operator=(const CircuitBreaker &) = delete;
	CircuitBreaker(CircuitBreaker &&) = delete;
	CircuitBreaker &operator=(CircuitBreaker &&) = delete;

	~CircuitBreaker()
	{
		if (!worker.joinable()) {
			return;
		}
		const bool busy = [this] {
			const std::lock_guard lock{shared->mutex};
			return shared->running != nullptr;
		}();
		// The worker owns the shared state, so it can be left inside a call that may never return.
		// Asked to stop first, it ends once that call returns instead of waiting for more calls.
		if (busy) {
			worker.request_stop();
			worker.detach();
		}
		// Otherwise ~jthread asks the idle worker to stop and joins it.
	}

	/**
	 * @brief Runs @p call on the breaker's worker and waits for it at most limit() from its start.
	 *
	 * An exception thrown by @p call is rethrown here.
	 *
	 * @param[in] call The call; see AbandonableCall for what it may capture.
	 *
	 * @retval result  What @p call returned within the limit.
	 * @retval nullopt The breaker was open, so @p call did not run; or @p call missed the limit,
	 *                 is abandoned, and has tripped the breaker open.
	 * @throws std::system_error The worker thread could not be started.
	 */
	template <AbandonableCall F> [[nodiscard]] std::optional<std::invoke_result_t<F &>> run(F &&call)
	{
		using Result = std::invoke_result_t<F &>;
		// Shared with the worker, which keeps the call and its result alive if it is abandoned.
		auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(call));
		std::future<Result> result = task->get_future();

		std::unique_lock lock{shared->mutex};
		if (shared->abandoned) {
			return std::nullopt;
		}
		if (!worker.joinable()) {
			worker = std::jthread{work, shared};
		}
		const auto progress = std::make_shared<Progress>();
		shared->pending.push_back({.progress = progress, .run = [task] { (*task)(); }});
		shared->changed.notify_all();
		// Calls queued behind others wait for their turn without a limit: each one before them is
		// bounded, or trips the breaker and drops the rest.
		shared->changed.wait(lock, [&] { return progress->started || progress->dropped; });
		if (progress->dropped) {
			return std::nullopt;
		}
		const Clock::time_point deadline = progress->startedAt + shared->limit;
		lock.unlock();

		if (result.wait_until(deadline) == std::future_status::timeout) {
			lock.lock();
			if (!progress->finished) {
				const bool trippedNow = !shared->abandoned;
				if (trippedNow) {
					shared->abandoned = true;
					++abandonedCalls;
					for (const auto &queued : shared->pending) {
						queued.progress->dropped = true;
					}
					shared->pending.clear();
					shared->changed.notify_all();
				}
				lock.unlock();
				if (trippedNow && shared->observer != nullptr) {
					shared->observer(shared->name, shared->limit, BreakerEvent::Tripped);
				}
				return std::nullopt;
			}
			// It finished just after the deadline: take the answer.
		}
		return result.get();
	}

	/// Whether the breaker is open: a call missed the limit and has not returned since.
	[[nodiscard]] bool isOpen() const
	{
		const std::lock_guard lock{shared->mutex};
		return shared->abandoned;
	}
	[[nodiscard]] std::string_view name() const noexcept { return shared->name; }
	[[nodiscard]] std::chrono::milliseconds limit() const noexcept { return shared->limit; }

	/**
	 * @brief Whether a call of any breaker in this module is abandoned and still running.
	 *
	 * Such a call may be inside a library the process is about to tear down, so a process that
	 * sees this at exit should end without running static destructors.
	 */
	[[nodiscard]] static bool anyAbandoned() noexcept { return abandonedCalls > 0; }

private:
	/// How far one call has got; guarded by Shared::mutex.
	struct Progress
	{
		bool started = false;
		bool finished = false;
		bool dropped = false; ///< Never started: dropped from the queue when the breaker tripped.
		Clock::time_point startedAt;
	};

	struct Pending
	{
		std::shared_ptr<Progress> progress;
		std::function<void()> run;
	};

	struct Shared
	{
		Shared(std::string groupName, std::chrono::milliseconds callLimit, Observer *callObserver)
			: name(std::move(groupName)), limit(callLimit), observer(callObserver)
		{}

		const std::string name;
		const std::chrono::milliseconds limit;
		Observer *const observer;

		std::mutex mutex;
		std::condition_variable_any changed;
		std::deque<Pending> pending;
		std::shared_ptr<Progress> running; ///< The call on the worker, if any.
		bool abandoned = false;
	};

	// Calls of every breaker in this module that are abandoned and still running.
	static inline std::atomic<int> abandonedCalls{0};

	static void work(std::stop_token stop, const std::shared_ptr<Shared> &state)
	{
		std::unique_lock lock{state->mutex};
		while (state->changed.wait(lock, stop, [&] { return !state->pending.empty(); })) {
			Pending next = std::move(state->pending.front());
			state->pending.pop_front();
			next.progress->started = true;
			next.progress->startedAt = Clock::now();
			state->running = next.progress;
			state->changed.notify_all();

			lock.unlock();
			next.run();
			lock.lock();

			next.progress->finished = true;
			state->running.reset();
			if (state->abandoned) {
				state->abandoned = false;
				--abandonedCalls;
				if (state->observer != nullptr) {
					lock.unlock();
					state->observer(state->name, state->limit, BreakerEvent::Reset);
					lock.lock();
				}
			}
		}
	}

	std::shared_ptr<Shared> shared;
	std::jthread worker;
};

} // namespace xpum

#endif /* UTILITY_CIRCUIT_BREAKER_CIRCUIT_BREAKER_H */
