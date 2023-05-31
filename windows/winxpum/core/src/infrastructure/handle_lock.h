/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file handle_lock.h
 */

#pragma once

#ifdef XPUM_ZE_HANDLE_LOCK_LOG
#include "logger.h"
#endif

#include <memory>
#include <mutex>
#include <unordered_map>

namespace xpum {
    class HandleLock {
    public:
        template <typename T>
        static std::shared_ptr<std::mutex> getHandleMutex(T& handle) {
            auto p = (void*)(handle);
            std::lock_guard<std::mutex> lock(handle_mutexes_mutex);
            auto it = handle_mutexes.find(p);
            if (it != handle_mutexes.end()) {
                return it->second;
            }
            else {
                handle_mutexes[p] = std::make_shared<std::mutex>();
                return handle_mutexes[p];
            }
        }

    private:
        static std::mutex handle_mutexes_mutex;
        static std::unordered_map<void*, std::shared_ptr<std::mutex>> handle_mutexes;
    };

} // namespace xpum

#ifdef XPUM_ZE_HANDLE_LOCK_LOG
#define XPUM_ZE_HANDLE_LOCK(handle, zefunc)                                                                                                              \
    {                                                                                                                                                    \
        using namespace std::chrono;                                                                                                                     \
        auto t0 = high_resolution_clock::now();                                                                                                          \
        std::lock_guard<std::mutex> lock(*HandleLock::getHandleMutex((handle)));                                                                         \
        auto t1 = high_resolution_clock::now();                                                                                                          \
        zefunc;                                                                                                                                          \
        auto t2 = high_resolution_clock::now();                                                                                                          \
        auto duration1 = duration_cast<microseconds>(t1 - t0);                                                                                           \
        auto duration2 = duration_cast<microseconds>(t2 - t1);                                                                                           \
        XPUM_LOG_INFO("{}({}): get lock for {} in {} us, exec in {} us", __FUNCTION__, __LINE__, (void*)(handle), duration1.count(), duration2.count()); \
    }
#else
#define XPUM_ZE_HANDLE_LOCK(handle, zefunc)                                      \
    {                                                                            \
        std::lock_guard<std::mutex> lock(*HandleLock::getHandleMutex((handle))); \
        zefunc;                                                                  \
    }
#endif