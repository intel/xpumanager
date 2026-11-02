/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _POWER_H
#define _POWER_H

#include "sysman.h"
#include <array>
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>

#define POWER_MONITOR_INTERNAL_PERIOD 80
#define DEFAULT_THROTTLE_POWER 300
/**
 * @brief Extended power limit descriptor
 *
 * This structure represents an extended power limit configuration for a device
 * or subdevice power domain. It provides information about specific power level
 * settings, current limit values, and whether the limit is locked.
 */
struct PowerLimitExt
{
	zes_power_level_t level; ///< Power level type (e.g., sustained, burst, peak)
	uint32_t limitMw;		 ///< Power limit value
	bool locked;			 ///< Indicates if the power limit is locked and cannot be modified
};

class device; // Forward declaration

// Device-specific power thresholds to avoid DLL interface issues with STL containers
class PowerThresholds
{
private:
	std::map<uint32_t, uint64_t> healthDeviceToTdps;
	/** Guards the one-time config file load. std::once_flag and std::mutex can be neither copied
	 *  nor moved, so they live here behind power's unique_ptr rather than in power itself. */
	std::once_flag configFlag;
	/** Serialises AMC TDP reads and guards amcTdp and amcSettled. */
	std::mutex amcMutex;
	/** TDP in watts read from the AMC FRU, 0 until a read returns one. */
	uint64_t amcTdp = 0;
	/** Set once the AMC read gave a final answer, so it is not asked again. */
	bool amcSettled = false;
	/** The background AMC read started by power::preloadThrottlePower(), if any. */
	std::future<void> preload;

public:
	PowerThresholds() = default;
	~PowerThresholds() = default;

	// Getter - return default if not found
	uint64_t getThrottlePower(uint32_t deviceId, uint64_t defaultValue) const;

	// Setter for loading configuration
	void setThrottlePower(uint32_t deviceId, uint64_t value);

	// Runs load exactly once; concurrent callers wait until it has finished
	void loadConfigOnce(const std::function<void()> &load) { std::call_once(configFlag, load); }

	/**
	 * Returns the AMC TDP, calling read first unless an earlier call already settled it. read
	 * stores the TDP it got, if any, and returns true when its answer is final; false (AMC busy or
	 * not responding) leaves the TDP to be read again on the next call. Concurrent callers wait.
	 */
	/// Runs task on its own thread unless one is already running. Returns false if no thread could
	/// be started.
	bool startPreload(const std::function<void()> &task);

	/// Waits for the thread started by startPreload(), if any.
	void waitPreload()
	{
		if (preload.valid()) {
			preload.wait();
		}
	}

	uint64_t amcThrottlePower(const std::function<bool(uint64_t &)> &read)
	{
		std::lock_guard<std::mutex> lock(amcMutex);
		if (!amcSettled) {
			amcSettled = read(amcTdp);
		}
		return amcTdp;
	}
};

class LIBXPUM_API power : public sysman
{
private:
	uint32_t powerCount;
	zes_pwr_handle_t *powerHandles;
	zes_device_handle_t zeDeviceHandle;
	device *deviceHandle;
	std::unique_ptr<PowerThresholds> thresholds;
	uint64_t defaultThrottlePower;
	/** One flag per getEnergy() domain — index 0 card, index 1 GPU — set once its
	 *  "domain not exposed" notice has been logged, so a dump loop reports it once. */
	std::array<bool, 2> energyDomainMissingLogged;
	/** Same, for the getEnergyPerTile() notice, which a stats run repeats per sample. */
	bool perTileEnergyDomainMissingLogged;

	// Threshold loading. Private because they write the threshold state without the locks:
	// only getThrottlePower() and preloadThrottlePower() call them, under PowerThresholds'
	// mutex and once_flag, so a direct call could race the preload thread.
	void ensureThrottlePowerLoaded();
	bool readAmcThrottlePower(uint64_t &watts);
	void loadConfigThresholds();
	void loadThresholdSection(const nlohmann::json &thresholdsJson, const std::string &key,
							  std::function<void(uint32_t, uint64_t)> setter);
	static uint32_t parseDeviceId(const std::string &hexKey);

public:
	power();
	~power();
	// power owns powerHandles and thresholds; a member-wise copy or move would leave two objects
	// releasing the same memory.
	power(const power &) = delete;
	power &operator=(const power &) = delete;
	power(power &&) = delete;
	power &operator=(power &&) = delete;
	ze_result_t enumPowerDomains(zes_device_handle_t device);
	ze_result_t getProperties(zes_pwr_handle_t powerHandle, zes_power_properties_t *properties,
							  zes_power_ext_properties_t *extProps);
	ze_result_t getEnergyCounter(zes_pwr_handle_t powerHandle, zes_power_energy_counter_t *energyCounter);
	ze_result_t getEnergyThreshold(zes_pwr_handle_t powerHandle);
	ze_result_t getPowerLimits(zes_pwr_handle_t powerHandle);
	ze_result_t getEnergy(uint64_t *power, uint64_t *timeStamp, bool forGPU);
	[[nodiscard]] ze_result_t getInstantPower(uint32_t *milliwatts, bool forGPU);
	[[nodiscard]] ze_result_t getMemoryInstantPower(uint32_t *milliwatts);

	/// Spelled-out name of a Level Zero power domain, e.g. "Card". Never null; "Unknown" for
	/// domains this build does not name.
	static const char *domainName(zes_power_domain_t domain);

	/**
	 * @param [out] tileEnergy  Map of tile_id -> (energy_microjoules, timestamp_microseconds).
	 * @param [out] domainUsed  Optional. Set to the domain the readings came from, or to
	 *                          ZES_POWER_DOMAIN_UNKNOWN when no reading was taken or when the
	 *                          per-tile readings do not all share one domain. Callers that label
	 *                          the figure must not assume a domain without consulting this.
	 */
	ze_result_t getEnergyPerTile(std::map<uint32_t, std::pair<uint64_t, uint64_t>> &tileEnergy,
								 zes_power_domain_t *domainUsed = nullptr);
	ze_result_t setSustainedLimit(uint32_t limit_mw, int32_t tile_id = -1);
	ze_result_t setBurstLimit(uint32_t limit_mw);
	ze_result_t setPeakLimit(uint32_t limit_ac_mw, uint32_t limit_dc_mw);
	ze_result_t getLimitsExt(std::vector<PowerLimitExt> &limits);
	ze_result_t setLimitsExt(int32_t tile_id, zes_power_level_t level, uint32_t limit_mw);
	ze_result_t getDomainLimits(std::map<zes_power_domain_t, std::map<zes_power_level_t, int32_t>> &domainLimits);
	ze_result_t getMaxPowerLimit(std::string &range);

	uint32_t getPowerCount() { return powerCount; }
	uint64_t getThrottlePower(uint32_t pciDeviceId);
	void preloadThrottlePower();
	zes_pwr_handle_t *getPowerHandles() { return powerHandles; }
	ze_result_t setPowerLimit(double powerLimit);

	ze_result_t init(zes_device_handle_t zeDevice) override;
	ze_result_t init(zes_device_handle_t zeDevice, class device *device);
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif