/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LOGGER_ASYNC_SINK_H
#define LOGGER_ASYNC_SINK_H

#include "log_level.h"
#include "log_record.h"
#include "sink_base.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <mutex>
#include <source_location>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Non-blocking asynchronous sink.
 *
 * Wraps any synchronous Sink so every log call returns immediately.
 * All I/O is performed on a dedicated background thread, eliminating
 * contention between logging callers and I/O latency.
 *
 * Ordering guarantee: messages from a single thread always reach the backend
 * in call order.  Messages from different threads may interleave.
 *
 * Shutdown: the destructor requests stop and joins the worker before any
 * member is torn down, ensuring all queued messages are delivered.
 *
 * @code
 *     auto file = std::make_shared<FileStreamSink>("app.log");
 *     auto asink = std::make_shared<AsyncSink>(file);
 *     Logger::instance().setSink(asink);
 * @endcode
 */
class AsyncSink final : public Sink
{
public:
	/// What to do when the queue has reached max_queue_size.
	enum class OverflowPolicy
	{
		Block,		///< Block the caller until space is available (backpressure).
		DropOldest, ///< Discard the oldest queued message to make room.
		DropNewest, ///< Discard the incoming message silently.
	};

	/// Cumulative statistics since construction.
	struct Stats
	{
		std::uint64_t queued;	 ///< Total messages placed in the queue.
		std::uint64_t processed; ///< Total messages successfully sent to backend.
		std::uint64_t dropped;	 ///< Total lost (overflow or backend error).
		std::size_t queueSize;	 ///< Current queue depth.
	};

	/**
	 * @param backend          Synchronous sink that performs actual I/O. Must not be null.
	 * @param max_queue_size   Maximum buffered messages before overflow policy fires.
	 * @param overflow_policy  Action taken when queue is full.
	 * @param flush_interval   Maximum time the worker sleeps before draining a partial batch.
	 */
	explicit AsyncSink(std::shared_ptr<Sink> backend, std::size_t maxQueueSize = 8192,
					   OverflowPolicy overflowPolicy = OverflowPolicy::DropOldest,
					   std::chrono::milliseconds flushInterval = std::chrono::milliseconds(100))
		: mBackend{std::move(backend)}, mMaxQueueSize{maxQueueSize}, mOverflowPolicy{overflowPolicy},
		  mFlushInterval{flushInterval}
	{
		if (!mBackend) {
			throw std::invalid_argument("AsyncSink: backend cannot be null");
		}
		if (mMaxQueueSize == 0) {
			throw std::invalid_argument("AsyncSink: max_queue_size must be > 0");
		}
		workerThread = std::jthread([this](std::stop_token st) { workerLoop(std::move(st)); });
	}

	/**
	 * @brief Stop the worker thread and drain all queued messages.
	 *
	 * worker_thread_ is declared last so its destructor—which joins the
	 * thread—runs before any other member is torn down.  The explicit
	 * request_stop()+join() in the body ensures the join completes inside
	 * the destructor body itself, before the implicit member destructors.
	 */
	~AsyncSink() override
	{
		workerThread.request_stop();
		queueCv.notify_all();
		workerThread.join();
	}

	/**
	 * Override log() to bypass Sink::mutex entirely.
	 *
	 * Sink::log() serialises emit()+sync() under a per-sink mutex, which
	 * would make all producers contend on a single lock before reaching the
	 * queue — defeating the non-blocking design.  By overriding here we skip
	 * that mutex; thread safety is provided by queue_mutex_ for the brief
	 * deque-push only.
	 */
	void log(LogLevel level, std::source_location loc, std::string_view prefix, std::string_view msg) noexcept override
	{
		try {
			LogRecord const record{
				level, loc, prefix, msg, std::chrono::system_clock::now(), std::this_thread::get_id()};
			enqueue(record);
		} catch (...) {
		} // allocation failure: drop silently
	}

	/// queue a log record.  Returns immediately; never throws.
	void emit(const LogRecord &record) override { enqueue(record); }

	/// Flushing is deferred to the worker.  Call flushNow() for synchronous delivery.
	void sync() noexcept override {}

	/**
	 * @brief Block until all messages queued before this call are delivered.
	 *
	 * Uses a retire counter rather than polling queue_.empty() so the call
	 * returns only after the worker has actually processed the messages, not
	 * merely dequeued them into its internal batch.
	 */
	void flushNow()
	{
		// Snapshot total queued so far; every one will eventually be retired.
		std::uint64_t const target = nQueued.load(std::memory_order_acquire);
		queueCv.notify_one(); // wake worker if sleeping
		std::unique_lock lock(queueMutex);
		// Guard against a stopped worker that will never retire remaining items.
		idleCv.wait(lock, [&] {
			return nRetired.load(std::memory_order_acquire) >= target || !workerThread.joinable() ||
				   workerThread.get_stop_token().stop_requested();
		});
	}

	/// Snapshot of cumulative statistics.
	[[nodiscard]] Stats getStats() const noexcept
	{
		std::lock_guard const lock(queueMutex);
		return Stats{nQueued.load(std::memory_order_relaxed), nProcessed.load(std::memory_order_relaxed),
					 nDropped.load(std::memory_order_relaxed), queue.size()};
	}

private:
	// ── LogEvent ──────────────────────────────────────────────────────────────
	// LogRecord holds string_views that may reference temporaries.  LogEvent
	// owns the strings so they are valid for the lifetime of the queue entry.
	struct LogEvent
	{
		LogLevel level;
		std::source_location loc;
		std::string prefix;
		std::string message;
		std::chrono::system_clock::time_point timestamp;
		std::thread::id threadId;

		explicit LogEvent(const LogRecord &r)
			: level(r.level), loc(r.loc), prefix(r.prefix), message(r.msg), timestamp(r.timestamp), threadId(r.threadId)
		{}
	};

	// ── Configuration (immutable after construction — no lock needed) ─────────
	std::shared_ptr<Sink> mBackend;
	std::size_t mMaxQueueSize;
	OverflowPolicy mOverflowPolicy;
	std::chrono::milliseconds mFlushInterval;

	// ── Queue state (all access via queue_mutex_) ─────────────────────────────
	mutable std::mutex queueMutex;
	std::deque<LogEvent> queue;
	std::condition_variable queueCv; ///< producers → worker
	std::condition_variable idleCv;	 ///< worker → flushNow()

	// ── Counters (atomic; no lock required) ───────────────────────────────────
	std::atomic<std::uint64_t> nQueued{0};	  ///< messages pushed to queue
	std::atomic<std::uint64_t> nRetired{0};	  ///< messages removed from queue (processed or dropped)
	std::atomic<std::uint64_t> nProcessed{0}; ///< successfully delivered to backend
	std::atomic<std::uint64_t> nDropped{0};	  ///< lost: overflow + backend errors

	// ── Worker thread ─────────────────────────────────────────────────────────
	// DECLARED LAST: its destructor joins the worker, which must finish
	// before any of the above members are destroyed.
	std::jthread workerThread;

	// ─────────────────────────────────────────────────────────────────────────
	static constexpr std::size_t K_BATCH_CAP = 256;

	void workerLoop(std::stop_token stoken)
	{
		std::vector<LogEvent> batch;
		batch.reserve(K_BATCH_CAP);

		while (true) {
			{
				std::unique_lock lock(queueMutex);
				queueCv.wait_for(lock, mFlushInterval, [&] { return !queue.empty() || stoken.stop_requested(); });

				if (queue.empty()) {
					if (stoken.stop_requested()) {
						break;
					}
					continue;
				}

				const std::size_t n = std::min(queue.size(), K_BATCH_CAP);
				const auto first = queue.begin();
				const auto last = first + static_cast<std::ptrdiff_t>(n);
				batch.assign(std::make_move_iterator(first), std::make_move_iterator(last));
				queue.erase(first, last);
				queueCv.notify_all(); // unblock producers blocked on Block policy
			}

			processBatch(batch);
			batch.clear();
		}

		drainQueue();
	}

	void processBatch(const std::vector<LogEvent> &batch)
	{
		for (const auto &ev : batch) {
			try {
				mBackend->log(ev.level, ev.loc, ev.prefix, ev.message);
				nProcessed.fetch_add(1, std::memory_order_relaxed);
			} catch (...) {
				nDropped.fetch_add(1, std::memory_order_relaxed);
			}
			nRetired.fetch_add(1, std::memory_order_release);
		}
		idleCv.notify_all(); // wake any flushNow() waiters
	}

	void drainQueue()
	{
		std::vector<LogEvent> remaining;
		{
			std::lock_guard const lock(queueMutex);
			remaining.assign(std::make_move_iterator(queue.begin()), std::make_move_iterator(queue.end()));
			queue.clear();
		}
		processBatch(remaining);
	}

	void enqueue(const LogRecord &record)
	{
		std::unique_lock lock(queueMutex);
		if (queue.size() >= mMaxQueueSize) {
			if (!handleOverflow(lock)) {
				return; // DropNewest: silently discard
			}
		}
		queue.emplace_back(record);
		nQueued.fetch_add(1, std::memory_order_release);
		lock.unlock();
		queueCv.notify_one();
	}

	/// Returns true if the caller should proceed to enqueue; false to discard.
	/// Pre-condition: queue_mutex_ is locked by the caller.
	bool handleOverflow(std::unique_lock<std::mutex> &lock)
	{
		switch (mOverflowPolicy) {
		case OverflowPolicy::Block:
			queueCv.wait(
				lock, [&] { return queue.size() < mMaxQueueSize || workerThread.get_stop_token().stop_requested(); });
			// If woken by stop rather than by available space, drop the message.
			if (queue.size() >= mMaxQueueSize) {
				nDropped.fetch_add(1, std::memory_order_relaxed);
				return false;
			}
			return true;

		case OverflowPolicy::DropOldest:
			queue.pop_front();
			nDropped.fetch_add(1, std::memory_order_relaxed);
			nRetired.fetch_add(1, std::memory_order_release);
			return true;

		case OverflowPolicy::DropNewest:
			nDropped.fetch_add(1, std::memory_order_relaxed);
			return false;
		}
		return true; // unreachable; suppresses -Wreturn-type
	}
};

#endif /* LOGGER_ASYNC_SINK_H */
