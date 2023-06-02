/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file scheduled_thread_pool.h
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace xpum {

class ScheduledThreadPoolTask {
    friend class SchedulingQueue;

   public:
    /**
     * @brief Constructs a new Scheduled Thread Pool Task object
     * 
     * @param delay the time in milliseconds to delay first execution
     * @param interval the interval in milliseconds between successive executions
     * @param execution_times the execution times of the task (-1 indicates run forever)
     * @param func the function to execute
     */
    ScheduledThreadPoolTask(uint64_t delay, uint32_t interval, int execution_times, std::function<void()> func) : interval(interval), remaining_exe_time(execution_times), exe_time(execution_times), func(func), cancelled(false) {
        scheduled_time = std::chrono::steady_clock::now() + std::chrono::milliseconds{delay};
    }

    ~ScheduledThreadPoolTask() {
        func = nullptr;
    };

    /**
     * @brief Advances the scheduled_time to its next run if it's a repeated task
     * 
     * @return true the scheduled_time has been advanced
     * @return false this task is not a repetaed task
     */
    bool next();

    /**
     * @brief Runs the task
     * 
     */
    void run();

    // std::shared_ptr<ScheduledThreadPoolTask> getNext

    /**
     * @brief Checks whether this task should be run after the other task
     * 
     * @param otherTask 
     * @return true 
     * @return false 
     */
    bool after(std::shared_ptr<ScheduledThreadPoolTask> other);

    /**
     * @brief Cancels this task to make sure it will not been run again. If this task is running, it will run to the end. No effect on already cancelled task.
     * 
     */
    void cancel();

    int getExeTime();

   private:
    uint32_t interval;
    // One design option is that we can use remaining_exe_time to judge whether is the task finished.
    // But then this member will need to be public and should be sync. So, we choose the member exe_time to finish the work.
    int remaining_exe_time;
    const int exe_time;
    std::function<void()> func;
    std::chrono::steady_clock::time_point scheduled_time;
    std::atomic<bool> cancelled;
};

class SchedulingQueue {
   public:
    SchedulingQueue() : stop(false) {}
    ~SchedulingQueue() { close(); }

    /**
    * @brief Puts a task to the queue
    * 
    * @param p_new_task 
    */
    void enqueue(std::shared_ptr<ScheduledThreadPoolTask> p_new_task);
    /**
     * @brief Blocks the current thread until there is a task whose scheduled running time has reached
     * 
     * @return std::shared_ptr<ScheduledThreadPoolTask> the task whose scheduled running time has reached
     */
    std::shared_ptr<ScheduledThreadPoolTask> dequeue();

    void close();

   private:
    std::list<std::shared_ptr<ScheduledThreadPoolTask>> q;
    std::mutex q_mutex;
    std::condition_variable cv;
    std::atomic<bool> stop;
};

class ScheduledThreadPool {
   public:
    ScheduledThreadPool(uint32_t size);
    ~ScheduledThreadPool();

    template <class F, class... Args>
    std::shared_ptr<ScheduledThreadPoolTask> scheduleAtFixedRate(uint64_t delay, uint32_t interval, int execution_times, F &&f, Args &&... args) {
        auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto p_task = std::make_shared<ScheduledThreadPoolTask>(delay, interval, execution_times, func);
        p_taskqueue->enqueue(p_task);
        return p_task;
    }

    void close();

   private:
    void init(uint32_t &size);

    std::vector<std::thread> workers;
    std::shared_ptr<SchedulingQueue> p_taskqueue;
    std::atomic<bool> stop;
};

} // end namespace xpum