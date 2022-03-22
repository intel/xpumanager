/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file timer.h
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

namespace xpum {

class Timer {
   public:
    Timer();

    ~Timer();

    void scheduleAtFixedRate(long delay, int interval, std::function<void()> task);

    void schedule(int delay, std::function<void()> task);

    void cancel() noexcept;
    bool isCanceld();

   private:
    Timer(const Timer& timer);

   private:
    std::atomic<bool> canceled;

    std::atomic<bool> to_cancel;

    std::mutex mutex;

    std::condition_variable cancel_condition;
};
} // end namespace xpum
