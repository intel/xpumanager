/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _RAS_H
#define _RAS_H

#include "sysman.h"
#include <map>
#include <mutex>
#include <vector>

/**
 * @brief Experimental per-category RAS error state entry.
 *
 * Holds the error counter reported by the experimental RAS API for a single
 * error category.
 */
struct ras_state_exp_t
{
	zes_ras_error_category_exp_t category;
	uint64_t errorCount;
	uint32_t tileId;
};

class LIBXPUM_API ras : public sysman
{
private:
	uint32_t rasCount;
	zes_ras_handle_t *rasHandles;
	// Enumerated on first use: on some runtimes the first enumeration in a process takes more
	// than a second, and most commands never read RAS.
	zes_device_handle_t zesDevice = nullptr;
	bool enumerated = false;
	/// Guards the enumeration state above. A copy gets a mutex of its own, so ras stays copyable
	/// as its device is; devices are only copied while the driver builds its list, before any
	/// thread reads RAS.
	struct EnumLock
	{
		std::mutex mutex;
		EnumLock() = default;
		EnumLock(const EnumLock &) noexcept {}
		EnumLock &operator=(const EnumLock &) noexcept { return *this; }
	};
	EnumLock enumLock;

	void ensureEnumerated();

public:
	ras() : rasCount(0), rasHandles(nullptr) {}
	~ras();
	ze_result_t enumRasErrorSets(zes_device_handle_t device);
	ze_result_t getProperties(zes_ras_handle_t rasHandle, zes_ras_properties_t *properties);
	ze_result_t getConfig(zes_ras_handle_t rasHandle, zes_ras_config_t *config);
	ze_result_t getState(zes_ras_handle_t rasHandle, zes_ras_state_t *state);
	ze_result_t getErrors(zes_ras_error_cat_t type, zes_ras_error_type_t errorType, uint64_t *rasCounter);
	ze_result_t getErrorsPerTile(zes_ras_error_cat_t type, zes_ras_error_type_t errorType,
								 std::map<uint32_t, uint64_t> &countersPerTile, uint64_t *totalCounter);
	ze_result_t getErrorsPerTileRasExp(std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> &rasErrStates);
	ze_result_t clearErrors();
	ze_result_t init(zes_device_handle_t device);
	ze_result_t zesRun(zes_device_handle_t device);
};

#endif