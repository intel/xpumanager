/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file scheduled_thread_pool.cpp
 */

#include "scheduled_thread_pool.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>

#include "logger.h"

// #define TRACE_SCHEDULED_TASK_RUN

namespace xpum {

// ScheduledThreadPool
ScheduledThreadPool::ScheduledThreadPool(uint32_t size) : stop(false) {
    XPUM_LOG_TRACE("constructing scheduled thread pool");
    init(size);
    XPUM_LOG_TRACE("scheduled thread pool constructed");
}

ScheduledThreadPool::~ScheduledThreadPool() {
    XPUM_LOG_TRACE("destructing scheduled thread pool");
    close();
    XPUM_LOG_TRACE("scheduled thread pool destructed");
}

void ScheduledThreadPool::init(uint32_t& size) {
    this->p_taskqueue = std::make_shared<SchedulingQueue>();

    for (uint32_t i = 0; i < size; ++i) {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////
        auto worker_thread_proc = [this]() -> void {
            XPUM_LOG_TRACE("ScheduledThreadPool worker thread started");
            while (true) {
                if (this->stop.load(std::memory_order_acquire)) break;
                // dequeue the first task which has reached its scheduled running time
                auto task = this->p_taskqueue->dequeue();
                if (this->stop.load(std::memory_order_acquire)) break;
                // if task queue offers a nullptr, it's closing
                if (task == nullptr) continue;
                try {
                    task->run();
                } catch (std::exception& e) {
                    XPUM_LOG_ERROR("Failed to execute scheduled threadpool task: {}", e.what());
                } catch (...) {
                    XPUM_LOG_ERROR("Failed to execute scheduled threadpool task: unexpected exception");
                }

                if (task->next()) {
                    // enqueue again the task which is scheduled to be run repeatly
                    this->p_taskqueue->enqueue(task);
                }
            }
            XPUM_LOG_TRACE("ScheduledThreadPool worker thread exit");
        };
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////
        this->workers.emplace_back(std::thread(worker_thread_proc));
        XPUM_LOG_TRACE("workder thread created in scheduled thread pool");
    }
}

void ScheduledThreadPool::wait() {
    while (this->p_taskqueue->getTaskSize() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void ScheduledThreadPool::close() {
    if (this->stop.load(std::memory_order_acquire)) return;

    XPUM_LOG_TRACE("closing scheduled thread pool");

    this->stop.store(true, std::memory_order_release);
    this->p_taskqueue->close();
    for (auto& worker_thread : workers) {
        worker_thread.join();
    }
    workers.clear();
    this->p_taskqueue = nullptr;

    XPUM_LOG_TRACE("scheduled thread pool closed");
}

/// ScheduledThreadPoolTask

bool ScheduledThreadPoolTask::after(std::shared_ptr<ScheduledThreadPoolTask> other) {
    return this->scheduled_time > other->scheduled_time;
}

bool ScheduledThreadPoolTask::next() {
    if (this->interval == 0) return false;
    this->scheduled_time += std::chrono::milliseconds{this->interval};
    auto now = std::chrono::steady_clock::now();
    if (now > this->scheduled_time) {
        // the scheduled time is too far before now, advance it to a near time
        auto gap_to_now = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->scheduled_time);
        auto gaps = gap_to_now.count() / this->interval;
        auto advanced_ms = std::chrono::milliseconds{this->interval * gaps};
        this->scheduled_time += advanced_ms;
    }
    return true;
}

void ScheduledThreadPoolTask::run() {
#ifdef TRACE_SCHEDULED_TASK_RUN
    auto start = std::chrono::steady_clock::now();
    auto delay = start - this->scheduled_time;
    XPUM_LOG_DEBUG("calling user function in worker thread, scheduled_time delayed: {}us",
                   std::chrono::duration_cast<std::chrono::microseconds>(delay).count());

#endif
    this->func();
#ifdef TRACE_SCHEDULED_TASK_RUN
    auto duration = std::chrono::steady_clock::now() - start;
    XPUM_LOG_DEBUG("user function runs for {}ms",
                   std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
#endif
}

void ScheduledThreadPoolTask::cancel() {
    this->cancelled.store(true, std::memory_order_release);
}

/// SchedulingQueue

void SchedulingQueue::enqueue(std::shared_ptr<ScheduledThreadPoolTask> newTask) {
    {
        std::lock_guard<std::mutex> lock(q_mutex);
        if (this->stop.load(std::memory_order_acquire)) {
            XPUM_LOG_TRACE("trying to enqueue after queue has stopped");
            return;
        }
        auto itr = std::find_if(q.rbegin(), q.rend(), [&newTask](auto& item) {
            return newTask->after(item);
        });

        if (itr == q.rbegin()) {
            // if at rbegin then newTask's priority is now the lowest
            q.emplace_back(newTask);
        } else if (itr == q.rend()) {
            // if at rend then newTask's priority is now the highest
            q.emplace_front(newTask);
        } else {
            // insert in middle
            q.emplace(itr.base(), newTask);
        }
    }
    // notify all waiters on task queue change so that they can try to get new task from the queue
    cv.notify_all();
}

std::shared_ptr<ScheduledThreadPoolTask> SchedulingQueue::dequeue() {
    std::unique_lock<std::mutex> lock(q_mutex);
    while (!this->stop.load(std::memory_order_acquire)) {
        if (!q.empty()) {
            auto first = q.front();
            auto now = std::chrono::steady_clock::now();
            if (first->cancelled) {
                // if the first task has been cancelled, remove it from the queue and continue to lookup next task in the queue
                q.pop_front();
                continue;
            }
            if (now >= first->scheduled_time) {
                // if the task at the head of the queue has reached its scheduled time
                q.pop_front();
                return first;
            }
            cv.wait_for(lock, first->scheduled_time - now);
        } else {
            cv.wait(lock);
        }
    }
    return nullptr;
}

void SchedulingQueue::close() {
    if (this->stop.load(std::memory_order_acquire)) return;
    XPUM_LOG_TRACE("closing scheduling queue");
    {
        std::lock_guard<std::mutex> lock(q_mutex);
        this->stop.store(true, std::memory_order_release);
        this->q.clear();
    }
    cv.notify_all();
    XPUM_LOG_TRACE("scheduling queue closed");
}
} // namespace xpum