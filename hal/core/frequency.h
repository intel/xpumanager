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
#ifndef _FREQUENCY_H
#define _FREQUENCY_H

#include "sysman.h"

class device; // Forward declaration
class LIBXPUM_API frequency : public sysman
{
private:
	uint32_t frequencyCount;
	zes_freq_handle_t *frequencyHandles;
	zes_device_handle_t deviceHandle;
	device *parentDevice;
	ze_result_t getSchedulerForSubdevice(uint32_t subdeviceId, zes_sched_handle_t &schedulerHandle);

public:
	frequency() : frequencyCount(0), frequencyHandles(nullptr), deviceHandle(nullptr), parentDevice(nullptr) {}
	~frequency();
	ze_result_t enumFrequencies(zes_device_handle_t device);
	ze_result_t getProperties(zes_freq_handle_t frequencyHandle, zes_freq_properties_t *properties);
	ze_result_t getAvailableClocks(zes_freq_handle_t frequencyHandle);
	ze_result_t getRange(zes_freq_handle_t frequencyHandle);
	ze_result_t getCurFreq(double *currentFreq, zes_freq_domain_t domain);
	ze_result_t setRange(double minFreq, double maxFreq);
	ze_result_t getState(zes_freq_handle_t frequencyHandle, zes_freq_state_t *state);
	ze_result_t getThrottleTime(zes_freq_handle_t frequencyHandle);
	ze_result_t getThrottleReason(zes_freq_throttle_reason_flags_t *throttleReasons);
	ze_result_t setFrequencyRange(double minFreq, double maxFreq, int32_t subdeviceId = -1);
	ze_result_t setFrequencyRangeForAll(double minFreq, double maxFreq);
	ze_result_t getFreqAvailableClocks(uint32_t subdeviceId, std::vector<double> &clocks);
	ze_result_t setSchedulerTimeoutMode(uint32_t subdeviceId, uint64_t watchdogTimeout);
	ze_result_t setSchedulerTimesliceMode(uint32_t subdeviceId, uint64_t interval, uint64_t yieldTimeout);
	ze_result_t setSchedulerExclusiveMode(uint32_t subdeviceId);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t init(zes_device_handle_t device, class device *parent);
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
