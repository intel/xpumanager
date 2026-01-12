/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _AMC_THREADING_H
#define _AMC_THREADING_H

#include <thread>
#include <mutex>
#include <memory>
#include "os.h"

// Cross-platform thread and mutex handles using modern C++
struct ThreadHandle
{
	std::unique_ptr<std::thread> thread;
	void *returnValue;
	bool valid;

	ThreadHandle() : returnValue(nullptr), valid(false) {}
};

struct MutexHandle
{
	std::unique_ptr<std::mutex> mutex;
	bool initialized;

	MutexHandle() : initialized(false) {}
};

// Generic thread function pointer (maintains compatibility with existing code)
typedef void *(*ThreadFunction)(void *arg);

/**
 * @brief Modern C++11-based threading interface
 *
 * Provides a unified threading and synchronization interface using standard C++11
 * std::thread and std::mutex primitives. Replaces platform-specific threading
 * implementations with portable standard library components.
 *
 * @note Uses std::thread and std::mutex for cross-platform compatibility
 * @note Maintains API compatibility with legacy OAL threading interface
 */
class LIBXPUM_API Threading
{
public:
	Threading();
	~Threading();

	// Basic thread operations
	int createThread(ThreadHandle **handle, ThreadFunction func, void *arg);
	int joinThread(ThreadHandle *handle, void **retval);
	void cleanupThread(ThreadHandle *handle);

	// Mutex operations
	int createMutex(MutexHandle **handle);
	int lockMutex(MutexHandle *handle);
	int unlockMutex(MutexHandle *handle);
	void destroyMutex(MutexHandle *handle);
};

#endif // _AMC_THREADING_H
