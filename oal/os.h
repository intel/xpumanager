/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _OS_H
#define _OS_H

#include <cstddef>
#include <cstdint>
#include <string>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define UNUSED_VAR(x) (void)(x)
#define UNUSED [[maybe_unused]]
#define GETPROCESSNAME(processId) getProcessName(processId)
#define AMC_CARD_DISCOVERY(amcDeviceList) amcCardDiscovery(amcDeviceList)
#define TIMESTAMP timestamp
#define SETPROGRESS(devIndex, lineNum, totalThreads, progress) setProgress(devIndex, lineNum, totalThreads, progress)

struct bdfID
{
	uint32_t domain;
	uint32_t bus;
	uint32_t device;
	uint32_t function;
};

struct amcCardInfo
{
	std::string amcDevicePath;
	std::string gpuParentPath;
};

std::string getProcessName(uint32_t processId);
std::string timestamp();
int amcCardDiscovery(void *amcDeviceList);
void setProgress(int devIndex, int lineNum, int totalThreads, uint32_t progress);

#ifdef _WIN32
#include "win/oswin.h"
#else
#include "lin/oslin.h"
#endif

#endif