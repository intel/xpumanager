/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _FREQUENCY_H
#define _FREQUENCY_H

#include "sysman.h"
#include <map>
#include <optional>
#include <span>
#include <vector>

class device; // Forward declaration
class LIBXPUM_API frequency : public sysman
{
private:
	uint32_t frequencyCount;
	zes_freq_handle_t *frequencyHandles;
	zes_device_handle_t deviceHandle;
	device *parentDevice;
	ze_result_t getSchedulerForSubdevice(uint32_t subdeviceId, zes_sched_handle_t &schedulerHandle);
	/// Properties of every handle that reports them; a handle that fails is left out.
	std::vector<zes_freq_properties_t> readAllProperties();
	/// State of the first handle of @p domain.
	ze_result_t getDomainState(zes_freq_domain_t domain, zes_freq_state_t &state);
	/// One member of the state of @p domain, or UNSUPPORTED_FEATURE when the driver does not know it.
	ze_result_t getDomainStateFrequency(double zes_freq_state_t::*member, zes_freq_domain_t domain, double *out);

public:
	frequency() : frequencyCount(0), frequencyHandles(nullptr), deviceHandle(nullptr), parentDevice(nullptr) {}
	~frequency();
	ze_result_t enumFrequencies(zes_device_handle_t device);
	ze_result_t getProperties(zes_freq_handle_t frequencyHandle, zes_freq_properties_t *properties);
	ze_result_t getAvailableClocks(zes_freq_handle_t frequencyHandle);
	ze_result_t getRange(zes_freq_handle_t frequencyHandle);
	ze_result_t getCurFreq(double *currentFreq, zes_freq_domain_t domain);
	ze_result_t getCurVoltage(double *currentVoltage, zes_freq_domain_t domain);
	ze_result_t getCurFreqPerTile(zes_freq_domain_t domain, std::map<uint32_t, double> &tileFrequencies);
	ze_result_t getCurVoltagePerTile(zes_freq_domain_t domain, std::map<uint32_t, double> &tileVoltages);
	ze_result_t setRange(double minFreq, double maxFreq);
	ze_result_t getState(zes_freq_handle_t frequencyHandle, zes_freq_state_t *state);
	ze_result_t getThrottleTime(zes_freq_handle_t frequencyHandle);
	ze_result_t getThrottleReason(zes_freq_throttle_reason_flags_t *throttleReasons);
	ze_result_t setFrequencyRange(double minFreq, double maxFreq, int32_t subdeviceId = -1);
	ze_result_t setFrequencyRangeForAll(double minFreq, double maxFreq);
	ze_result_t resetFrequencyRange(int32_t tileId = -1);
	ze_result_t getFreqAvailableClocks(uint32_t subdeviceId, std::vector<double> &clocks);
	ze_result_t getFreqRangeForTile(uint32_t tileId, double &minFreq, double &maxFreq);
	ze_result_t getFreqStateForTile(uint32_t tileId, zes_freq_state_t *state, zes_freq_properties_t *props);
	uint32_t getFrequencyCount() const { return frequencyCount; }
	std::span<const zes_freq_handle_t> getFrequencyHandles() const { return {frequencyHandles, frequencyCount}; }
	ze_result_t getMaxFreqForDomain(zes_freq_domain_t domain, double &maxMHz);
	[[nodiscard]] ze_result_t getMinFreqForDomain(zes_freq_domain_t domain, double &minMHz);
	[[nodiscard]] ze_result_t getRequestedFreq(double *requestedFreq, zes_freq_domain_t domain);
	[[nodiscard]] ze_result_t getEfficientFreq(double *efficientFreq, zes_freq_domain_t domain);
	[[nodiscard]] ze_result_t getTdpFreq(double *tdpFreq, zes_freq_domain_t domain);

	/// The lowest positive minimum among the @p props of @p domain, if there is one.
	[[nodiscard]] static std::optional<double> minFrequencyOf(std::span<const zes_freq_properties_t> props,
															  zes_freq_domain_t domain) noexcept;
	/// The highest positive maximum among the @p props of @p domain, if there is one.
	[[nodiscard]] static std::optional<double> maxFrequencyOf(std::span<const zes_freq_properties_t> props,
															  zes_freq_domain_t domain) noexcept;
	/// @p reported unless it is negative, which is how the driver says it does not know.
	[[nodiscard]] static std::optional<double> knownFrequencyOf(double reported) noexcept;
	ze_result_t setSchedulerTimeoutMode(uint32_t subdeviceId, uint64_t watchdogTimeout);
	ze_result_t setSchedulerTimesliceMode(uint32_t subdeviceId, uint64_t interval, uint64_t yieldTimeout);
	ze_result_t setSchedulerExclusiveMode(uint32_t subdeviceId);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t init(zes_device_handle_t device, class device *parent);
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
