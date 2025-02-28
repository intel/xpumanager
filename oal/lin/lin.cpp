/*
 * Copyright � 2024 Intel Corporation
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

#include "os.h"
#include "../ptp.h"
#include <sys/mman.h>
#include <math.h>

/**
 * @brief Allocates memory aligned to a specified boundary and advises the kernel to use huge pages.
 *
 * @param size The size of the memory to allocate.
 * @return A pointer to the allocated memory.
 */
void *align_alloc(size_t size)
{
	void *buffer;
	size = ceil(size / TWO_MB) * TWO_MB + TWO_MB; // Round up size to to be even multiple of TWO_MB.
	buffer = aligned_alloc(TWO_MB, size);		  // Allocate aligned memory.
	madvise(buffer, size, MADV_HUGEPAGE);		  // Accept huge pages.
	return buffer;
}

/**
 * @brief Placeholder function for P2P memory test.
 *
 * @param p Pointer to the parameters for the P2P memory test.
 * @return Always returns 0.
 */
int ptp::p2p_mem_test(params *p)
{
	return 0;
}

/**
 * @brief Creates a new thread.
 *
 * @param thread Function pointer to the thread function.
 * @param args Arguments to pass to the thread function.
 * @return A pointer to the thread_id object representing the created thread.
 */
thread_id *create_thread(funcptr thread, void *args)
{
	pthread_t thread_hdl;
	pthread_create(&thread_hdl, NULL, thread, args);

	DBG("%s: thread handle is %ld\n", __func__, thread_hdl);
	thread_id *new_thread_id = new thread_id(thread_hdl);
	return new_thread_id;
}

/**
 * @brief Waits for the specified thread to complete.
 *
 * @param tid Pointer to the thread_id object representing the thread to wait for.
 */
void wait_for_thread(thread_id *tid)
{
	if (tid->ret_thread_uid()) {
		DBG("%s: thread handle is %ld\n", __func__, tid->ret_thread_uid());
		pthread_join(tid->ret_thread_uid(), NULL);
	}
}
