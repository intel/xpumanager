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

#include "threading.h"
#include "os.h"
#include <exception>

/**
 * @brief Wrapper structure for thread execution
 *
 * Contains the thread function and arguments needed for std::thread execution.
 * Handles the conversion between the legacy ThreadFunction interface and
 * modern std::thread requirements.
 */
struct ThreadExecutionContext
{
	ThreadFunction func;
	void *arg;
	ThreadHandle *handle;
};

/**
 * @brief Thread execution wrapper function
 *
 * Adapts the legacy ThreadFunction interface to work with std::thread.
 * Executes the user-provided function and stores the return value.
 *
 * @param context Pointer to ThreadExecutionContext containing function and arguments
 */
static void threadExecutionWrapper(ThreadExecutionContext *context)
{
	try {
		if (context && context->func && context->handle) {
			context->handle->returnValue = context->func(context->arg);
		}
	} catch (...) {
		// Ensure threads don't terminate due to unhandled exceptions
		if (context && context->handle) {
			context->handle->returnValue = nullptr;
		}
	}

	// Clean up the context that was allocated in createThread
	delete context;
}

/**
 * @brief Constructor for Threading class
 *
 * Initializes the modern C++11-based threading interface.
 */
Threading::Threading() {}

/**
 * @brief Destructor for Threading class
 *
 * Performs cleanup of threading resources.
 */
Threading::~Threading() {}

/**
 * @brief Create a new thread with specified function and arguments
 *
 * Creates a new thread using std::thread and stores the thread handle
 * for later management operations like joining or cleanup.
 *
 * @param handle Pointer to ThreadHandle pointer that will receive the new thread handle
 * @param func Thread function to execute (must match ThreadFunction signature)
 * @param arg Argument to pass to the thread function
 *
 * @return Status of thread creation
 * @retval 0 Thread created successfully
 * @retval -1 Thread creation failed or invalid parameters
 *
 * @note Caller must call joinThread() and cleanupThread() to properly manage the thread
 * @note Thread handle is allocated dynamically and must be cleaned up
 */
int Threading::createThread(ThreadHandle **handle, ThreadFunction func, void *arg)
{
	if (!handle || !func) {
		return -1;
	}

	try {
		ThreadHandle *threadHandle = new ThreadHandle();
		ThreadExecutionContext *context = new ThreadExecutionContext();

		context->func = func;
		context->arg = arg;
		context->handle = threadHandle;

		threadHandle->thread = std::make_unique<std::thread>(threadExecutionWrapper, context);
		threadHandle->valid = true;
		threadHandle->returnValue = nullptr;

		*handle = threadHandle;
		return 0;
	} catch (const std::exception &) {
		*handle = nullptr;
		return -1;
	}
}

/**
 * @brief Wait for thread completion and retrieve its exit status
 *
 * Blocks the calling thread until the specified thread completes execution.
 * Uses std::thread::join() to synchronize thread completion.
 *
 * @param handle Pointer to ThreadHandle for the thread to join
 * @param retval Pointer to store the thread's return value (optional, can be nullptr)
 *
 * @return Status of thread join operation
 * @retval 0 Thread joined successfully
 * @retval -1 Join failed or invalid thread handle
 *
 * @note Must be called before cleanupThread() to properly synchronize
 * @note Calling thread will block until target thread completes
 */
int Threading::joinThread(ThreadHandle *handle, void **retval)
{
	if (!handle || !handle->valid || !handle->thread) {
		return -1;
	}

	try {
		if (handle->thread->joinable()) {
			handle->thread->join();
			if (retval) {
				*retval = handle->returnValue;
			}
			return 0;
		}
		return -1;
	} catch (const std::exception &) {
		return -1;
	}
}

/**
 * @brief Clean up thread handle and associated resources
 *
 * Releases memory allocated for the thread handle and marks it as invalid.
 * Should be called after joinThread() to complete thread cleanup.
 *
 * @param handle Pointer to ThreadHandle to clean up
 *
 * @note Safe to call with null handle pointer
 * @note Does not terminate the thread - use joinThread() first
 */
void Threading::cleanupThread(ThreadHandle *handle)
{
	if (handle) {
		handle->valid = false;
		handle->thread.reset(); // Destroys the std::thread
		delete handle;
	}
}

/**
 * @brief Create a new mutex for thread synchronization
 *
 * Creates a new mutex using std::mutex. The mutex can be used for
 * protecting shared resources in multi-threaded code.
 *
 * @param handle Pointer to MutexHandle pointer that will receive the new mutex handle
 *
 * @return Status of mutex creation
 * @retval 0 Mutex created successfully
 * @retval -1 Mutex creation failed or invalid parameters
 *
 * @note Caller must call destroyMutex() to properly clean up the mutex
 * @note Mutex handle is allocated dynamically and must be cleaned up
 */
int Threading::createMutex(MutexHandle **handle)
{
	if (!handle) {
		return -1;
	}

	try {
		MutexHandle *mutexHandle = new MutexHandle();
		mutexHandle->mutex = std::make_unique<std::mutex>();
		mutexHandle->initialized = true;

		*handle = mutexHandle;
		return 0;
	} catch (const std::exception &) {
		*handle = nullptr;
		return -1;
	}
}

/**
 * @brief Acquire exclusive lock on mutex
 *
 * Blocks the calling thread until the mutex can be locked exclusively.
 * Uses std::mutex::lock() for synchronization.
 *
 * @param handle Pointer to MutexHandle for the mutex to lock
 *
 * @return Status of mutex lock operation
 * @retval 0 Mutex locked successfully
 * @retval -1 Lock failed or invalid mutex handle
 *
 * @note Must be paired with unlockMutex() call
 * @note Calling thread will block until mutex is available
 */
int Threading::lockMutex(MutexHandle *handle)
{
	if (!handle || !handle->initialized || !handle->mutex) {
		return -1;
	}

	try {
		handle->mutex->lock();
		return 0;
	} catch (const std::exception &) {
		return -1;
	}
}

/**
 * @brief Release exclusive lock on mutex
 *
 * Releases the mutex lock, allowing other threads to acquire it.
 * Uses std::mutex::unlock() for synchronization.
 *
 * @param handle Pointer to MutexHandle for the mutex to unlock
 *
 * @return Status of mutex unlock operation
 * @retval 0 Mutex unlocked successfully
 * @retval -1 Unlock failed or invalid mutex handle
 *
 * @note Must be called by the same thread that acquired the lock
 * @note Should be called to match every successful lockMutex() call
 */
int Threading::unlockMutex(MutexHandle *handle)
{
	if (!handle || !handle->initialized || !handle->mutex) {
		return -1;
	}

	try {
		handle->mutex->unlock();
		return 0;
	} catch (const std::exception &) {
		return -1;
	}
}

/**
 * @brief Destroy mutex and release associated resources
 *
 * Destroys the mutex using std::unique_ptr automatic cleanup and frees allocated memory.
 * Should be called when the mutex is no longer needed.
 *
 * @param handle Pointer to MutexHandle to destroy
 *
 * @note Safe to call with null handle pointer
 * @note Mutex should not be locked when destroyed
 * @note Frees dynamically allocated mutex handle memory
 */
void Threading::destroyMutex(MutexHandle *handle)
{
	if (handle) {
		handle->initialized = false;
		handle->mutex.reset(); // Destroys the std::mutex
		delete handle;
	}
}
