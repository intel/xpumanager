/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file timer.cpp
 */

#include "timer.h"

#include <chrono>
#include <ctime>
#include <mutex>
#include <thread>

#include "infrastructure/exception/ilegal_parameter_exception.h"
#include "infrastructure/exception/ilegal_state_exception.h"
#include "logger.h"
#include "utility.h"

namespace xpum {

Timer::Timer() : canceled(true), to_cancel(false) {
}

Timer::~Timer() {
    this->cancel();
}

void Timer::scheduleAtFixedRate(long delay, int interval,
                                std::function<void()> task) {
    if (delay < 0 || interval <= 0) {
        XPUM_LOG_WARN("invalid parameter in scheduleAtFixedRate");
        throw IlegalParameterException("invalid parameter when schedule a timer");
    }

    if (this->canceled == false) {
        XPUM_LOG_WARN("invalid timer status");
        throw IlegalStateException("the timer has been started");
    }

    this->canceled = false;

    std::thread([this, delay, interval, task]() {
        int wait = delay;
        std::this_thread::sleep_for(std::chrono::milliseconds(wait));
        while (!this->to_cancel) {
            long begin = Utility::getCurrentMillisecond();
            task();
            long end = Utility::getCurrentMillisecond();
            long spent = end - begin;
            if (interval >= spent) {
                wait = interval - spent;
                std::this_thread::sleep_for(std::chrono::milliseconds(wait));
            } else {
                XPUM_LOG_DEBUG("The timer interval will not be accurate");
                wait = 0;
            }
        }

        std::unique_lock<std::mutex> locker(this->mutex);
        this->canceled = true;
        this->cancel_condition.notify_one();
    }).detach();
}

void Timer::schedule(int delay, std::function<void()> task) {
    std::thread([delay, task]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        task();
    }).detach();
}

bool Timer::isCanceld() {
    return this->canceled;
}
void Timer::cancel() noexcept {
    if (this->canceled) {
        return;
    }

    if (this->to_cancel) {
        return;
    }

    this->to_cancel = true;

    std::unique_lock<std::mutex> locker(this->mutex);
    this->cancel_condition.wait(locker, [this] {
        return this->canceled == true;
    });

    if (this->canceled == true) {
        this->to_cancel = false;
    }
}

} // end namespace xpum
