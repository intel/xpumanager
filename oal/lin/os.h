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

#ifndef _OS_H
#define _OS_H

#include <cstddef>
#include <unistd.h>
#include <cstring>
#include <pthread.h>
#include <pdev.h>

#define TESTING 1

#define LIBXPUM_API
#define UNUSED(x)                          (void)(x)
#define TWO_MB                             (2 * 1024 * 1024)
#define P2P_MEM_TEST                       p2p_mem_test
#define STRCPY_S(dest, sz, src)            strcpy(dest, src)
#define STRNCPY_S(dest, src, sz)           strncpy(dest, src, sz)
#define STRTOK_S(str, delimiters, context) strtok(str, delimiters)
#define STRCASECMP                         strcasecmp
#define THREAD_RET                         void *
#define GETOPT                             getopt
#define IS_GRAPHICS_CLASS(device_class)    (((device_class >> 16) & 0x0ff) == 3)
#define PCI_PATH_BAR_GENERIC               "/sys/bus/pci/devices/0000:"
#define MMIO_SIZE                          (2*1024*1024)

#if !TESTING
#define GET_PCI_DEV(devs)                  intel_get_pci_device(devs)
#define PCI_CLEANUP(devs, found_dev)       intel_pci_cleanup(devs, found_dev)
#else
#define GET_PCI_DEV(devs)                  (1)
#define PCI_CLEANUP(devs, found_dev)
#endif

typedef void* (*funcptr)(void* input_params);
void *align_alloc(size_t size);

class thread_id {

protected:
    pthread_t thread_hdl;

public:
    thread_id(pthread_t hdl) { thread_hdl = hdl; }
    pthread_t ret_thread_uid() { return thread_hdl; }
};

thread_id *create_thread(funcptr thread, void *args);
void wait_for_thread(thread_id *tid);
int intel_get_pci_device(p_dev *devs);
int intel_mmio_use_pci_bar(p_dev *dev);
void intel_pci_cleanup(p_dev *devs, int found_dev);

#endif
