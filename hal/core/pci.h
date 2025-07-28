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
#ifndef PCI_H
#define PCI_H

#include "sysman.h"
#include <string>

#define BDF_STR_LEN 64

typedef struct pci_addr_mei_device
{
	zes_pci_properties_t pciProps;
	std::string meiDevicePath;
	std::string fwStatus;
} pci_addr_mei_device;

class LIBXPUM_API pci : public sysman
{
private:
	pci_addr_mei_device deviceProperties;
	char bdfStr[BDF_STR_LEN];

public:
	pci() : deviceProperties{} { deviceProperties.fwStatus = "unknown"; }
	~pci() {}
	ze_result_t init(zes_device_handle_t device);
	ze_result_t getProperties(zes_device_handle_t device, zes_pci_properties_t *pciProperties);
	ze_result_t getBars(zes_device_handle_t device);
	ze_result_t getState(zes_device_handle_t device);
	ze_result_t getStats(zes_device_handle_t device, zes_pci_stats_t *pciStats);
	ze_result_t zesRun(zes_device_handle_t device);
	bool isBDF(const char *bdf);
	std::string getMeiDevicePath() { return deviceProperties.meiDevicePath; }
	std::string getFWStatus() { return deviceProperties.fwStatus; }
	const char *getBDF() const { return bdfStr; }
};

#endif
