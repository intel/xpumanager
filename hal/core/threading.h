/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
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
