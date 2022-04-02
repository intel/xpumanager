/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file shared_queue.h
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>

#include "infrastructure/exception/base_exception.h"

namespace xpum {

template <class T>
class SharedQueue {
   public:
    SharedQueue() : stop(false) {}

    virtual ~SharedQueue() { close(); }

    void add(T msg) {
        if (stop) {
            throw BaseException("add task to a stopped shared queue");
        }

        std::unique_lock<std::mutex> lock(q_mutex);
        q.push(msg);
        cv.notify_one();
    }

    T remove() {
        std::unique_lock<std::mutex> lock(q_mutex);
        while (q.empty() && !stop) {
            cv.wait(lock);
        }

        if (stop && q.empty()) {
            return nullptr;
        }

        T msg = std::move(this->q.front());
        q.pop();
        return msg;
    }

    void close() {
        stop = true;
        cv.notify_all();
    }

   private:
    std::queue<T> q;

    std::mutex q_mutex;

    std::condition_variable cv;

    std::atomic<bool> stop;
};

} // end namespace xpum
