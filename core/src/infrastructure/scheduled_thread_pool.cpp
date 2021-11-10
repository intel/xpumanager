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
    init(size);
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
    }
}

void ScheduledThreadPool::close() {
    if (this->stop.load(std::memory_order_acquire)) return;

    this->stop.store(true, std::memory_order_release);
    this->p_taskqueue->close();
    for (auto& worker_thread : workers) {
        worker_thread.join();
    }
    workers.clear();
    this->p_taskqueue = nullptr;
}

/// ScheduledThreadPoolTask

bool ScheduledThreadPoolTask::after(std::shared_ptr<ScheduledThreadPoolTask> other) {
    return this->scheduled_time > other->scheduled_time;
}

bool ScheduledThreadPoolTask::next() {
    if (this->interval == 0) return false;
    this->scheduled_time = this->scheduled_time + std::chrono::milliseconds{this->interval};
    return true;
}

void ScheduledThreadPoolTask::run() {
#ifdef TRACE_SCHEDULED_TASK_RUN
    auto duration = std::chrono::steady_clock::now() - this->scheduled_time;
    XPUM_LOG_WARN("calling user function in worker thread, scheduled_time delayed: {}ns", duration.count());
#endif
    this->func();
}

void ScheduledThreadPoolTask::cancel() {
    this->cancelled.store(true, std::memory_order_release);
}

/// SchedulingQueue

void SchedulingQueue::enqueue(std::shared_ptr<ScheduledThreadPoolTask> p_new_task) {
    {
        std::lock_guard<std::mutex> lock(q_mutex);
        if (this->stop.load(std::memory_order_acquire)) {
            XPUM_LOG_TRACE("trying to enqueue after queue has stopped");
            return;
        }
        auto itr = std::find_if(q.rbegin(), q.rend(), [&p_new_task](auto& item) {
            return p_new_task->after(item);
        });

        if (itr == q.rbegin()) {
            // if at rbegin then p_new_task's priority is now the lowest
            q.emplace_back(p_new_task);
        } else if (itr == q.rend()) {
            // if at rend then p_new_task's priority is now the highest
            q.emplace_front(p_new_task);
        } else {
            // if at rend then p_new_task's priority is now the highest
            q.emplace(itr.base(), p_new_task);
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
            cv.wait_for(lock, now - first->scheduled_time);
        } else {
            cv.wait(lock);
        }
    }
    return nullptr;
}

void SchedulingQueue::close() {
    if (this->stop.load(std::memory_order_acquire)) return;
    {
        std::lock_guard<std::mutex> lock(q_mutex);
        this->stop.store(true, std::memory_order_release);
        this->q.clear();
    }
    cv.notify_all();
}
} // namespace xpum