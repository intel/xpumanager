/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef PCI_H
#define PCI_H

#include "sysman.h"
#include <string>
#include <osvf.h>

#define BDF_STR_LEN 64

typedef struct pci_addr_mei_device
{
	zes_pci_properties_t pciProps;
	std::string meiDevicePath;
	std::string fwStatus;
} pci_addr_mei_device;

struct PciDowngradeState
{
	bool downgradeSupported;
	bool downgradeEnabled;
	zes_device_action_t pendingAction;
};

class LIBXPUM_API pci : public sysman
{
private:
	pci_addr_mei_device deviceProperties;
	PciDowngradeState downgradeState;
	uint32_t domain;
	uint32_t bus;
	uint32_t dev;
	uint32_t func;
	char bdfStr[BDF_STR_LEN];
	zes_driver_handle_t zesDriver = nullptr;
	typedef ze_result_t (*pfnZesIntelDevicePciLinkSpeedUpdateExp_t)(zes_device_handle_t hDevice, ze_bool_t bDowngrade,
																	zes_device_action_t *pPendingAction);

public:
	pci() : deviceProperties{}, downgradeState{} { deviceProperties.fwStatus = "unknown"; }
	~pci() {}
	ze_result_t init(zes_device_handle_t device);
	ze_result_t getProperties(zes_device_handle_t device, zes_pci_properties_t *pciProperties);
	ze_result_t getBars(zes_device_handle_t device);
	ze_result_t getState(zes_device_handle_t device, zes_pci_link_status_t &pciLinkStatus);
	ze_result_t getCurrentLinkSpeed(zes_device_handle_t device, zes_pci_speed_t &speed);
	ze_result_t getStats(zes_device_handle_t device, zes_pci_stats_t *pciStats);
	ze_result_t getPciDowngradeState(const zes_device_handle_t &device, PciDowngradeState &state);
	ze_result_t setPciDowngradeState(const zes_device_handle_t &device, bool enabled, PciDowngradeState &state);
	ze_result_t zesRun(zes_device_handle_t device);
	bool isBDF(const char *bdf);
	std::string getMeiDevicePath() { return deviceProperties.meiDevicePath; }
	std::string getFWStatus() { return deviceProperties.fwStatus; }
	void getBDF(bdfID &bdf) const
	{
		bdf.domain = this->domain;
		bdf.bus = this->bus;
		bdf.device = this->dev;
		bdf.function = this->func;
	}
	void setZesDriver(zes_driver_handle_t zesD) { zesDriver = zesD; }
	std::string getBDFStr() const { return std::string(bdfStr); }
	devFuncType getFuncType();
};

#endif
