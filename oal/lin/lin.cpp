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
#include <sys/mman.h>
#include <math.h>
#include <pciaccess.h>
#include <debug.h>
#include <sysinfo.h>

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

int intel_get_pci_device(p_dev *devs)
{
	struct pci_device_iterator *iter;
	struct pci_device *pci_dev;
	int error, found = 0;

	error = pci_system_init();
	if (error) {
		ERR("Couldn't initialize PCI system\n");
		return found;
	}

	iter = pci_slot_match_iterator_create(NULL);

	while ((pci_dev = pci_device_next(iter)) != NULL) {
		if (IS_GRAPHICS_CLASS(pci_dev->device_class)) {
			DBG("Found device: %04x:%04x @ %02x:%02x.%x\n", pci_dev->vendor_id, pci_dev->device_id,
				pci_dev->bus, pci_dev->dev, pci_dev->func);
			error = pci_device_probe(pci_dev);
			if (error) {
				ERR("Couldn't probe PCI device\n");
				break;
			}
			devs[found].dev = pci_dev;
			sprintf(devs[found].resource_name, "%s%02x:%02x.%x/resource2", PCI_PATH_BAR_GENERIC,
					pci_dev->bus, pci_dev->dev, pci_dev->func);
			DBG("Resource name: %s\n", devs[found].resource_name);

			found++;
			if (found == MAX_DEVS) {
				break;
			}
		}
	}
	pci_iterator_destroy(iter);

	return found;
}

void intel_pci_cleanup()
{
	pci_system_cleanup();
}
