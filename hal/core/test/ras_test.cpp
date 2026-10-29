/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for the RAS module's bounded calls. The RAS entry points it uses are defined here,
 * so the test needs no GPU and can make enumeration block as the CRI runtime does when a card's
 * RAS node never answers. The test cases share the process-wide RAS breaker and run in order.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include "bounded_call.h"
#include "ras.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <algorithm>
#include <map>
#include <thread>
#include <type_traits>
#include <vector>

using namespace std::chrono_literals;

namespace {

// Level Zero handles are pointers to opaque driver types; these only need distinct addresses.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::array<int, 3> handleStorage{};
std::vector<zes_ras_error_category_exp_t> clearedCategories;
std::atomic<bool> blockEnumeration{false};
std::atomic<ze_result_t> enumerationResult{ZE_RESULT_SUCCESS};
std::atomic<int> enumerations{0};
std::promise<void> releaseEnumeration;
std::shared_future<void> enumerationReleased = releaseEnumeration.get_future().share();
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

template <typename Handle> [[nodiscard]] Handle fakeHandle(std::size_t index)
{
	// reinterpret_cast: the C API's handles are opaque pointers that the fakes below never follow.
	return reinterpret_cast<Handle>(&handleStorage.at(index));
}

constexpr std::uint64_t CORRECTABLE_RESETS = 2;
constexpr std::uint64_t CATEGORY_ERRORS = 5;
// The experimental calls report more categories when asked for the count than they then return,
// as a driver may when its list changes between the two calls.
constexpr uint32_t CATEGORIES_COUNTED = 3;
constexpr std::array RETURNED_CATEGORIES{ZES_RAS_ERROR_CATEGORY_EXP_MEMORY_ERRORS,
										 ZES_RAS_ERROR_CATEGORY_EXP_PCIE_ERRORS};
constexpr std::uint64_t UNCORRECTABLE_RESETS = 3;

[[nodiscard]] zes_device_handle_t newDevice() { return fakeHandle<zes_device_handle_t>(0); }

[[nodiscard]] bool eventually(ras &device, std::chrono::milliseconds within)
{
	const auto until = std::chrono::steady_clock::now() + within;
	uint64_t count = 0;
	while (device.getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count) != ZE_RESULT_SUCCESS) {
		if (std::chrono::steady_clock::now() > until) {
			return false;
		}
		std::this_thread::sleep_for(20ms);
	}
	return true;
}

} // namespace

// The fakes replace the loader's entry points for the RAS calls ras.cpp makes in these tests.
extern "C" {

ze_result_t zesDeviceEnumRasErrorSets(zes_device_handle_t, uint32_t *pCount, zes_ras_handle_t *phRas)
{
	if (phRas == nullptr) {
		++enumerations;
	}
	if (blockEnumeration.load()) {
		enumerationReleased.wait();
	}
	if (const ze_result_t forced = enumerationResult.load(); forced != ZE_RESULT_SUCCESS) {
		return forced;
	}
	if (phRas != nullptr) {
		phRas[0] = fakeHandle<zes_ras_handle_t>(1);
		phRas[1] = fakeHandle<zes_ras_handle_t>(2);
	}
	*pCount = 2;
	return ZE_RESULT_SUCCESS;
}

ze_result_t zesRasGetProperties(zes_ras_handle_t hRas, zes_ras_properties_t *pProperties)
{
	pProperties->type =
		hRas == fakeHandle<zes_ras_handle_t>(1) ? ZES_RAS_ERROR_TYPE_CORRECTABLE : ZES_RAS_ERROR_TYPE_UNCORRECTABLE;
	pProperties->onSubdevice = false;
	pProperties->subdeviceId = 0;
	return ZE_RESULT_SUCCESS;
}

ze_result_t zesRasGetState(zes_ras_handle_t hRas, ze_bool_t, zes_ras_state_t *pState)
{
	pState->category[ZES_RAS_ERROR_CAT_RESET] =
		hRas == fakeHandle<zes_ras_handle_t>(1) ? CORRECTABLE_RESETS : UNCORRECTABLE_RESETS;
	return ZE_RESULT_SUCCESS;
}

ze_result_t zesRasGetSupportedCategoriesExp(zes_ras_handle_t, uint32_t *pCount,
											zes_ras_error_category_exp_t *pCategories)
{
	if (pCategories == nullptr) {
		*pCount = CATEGORIES_COUNTED;
		return ZE_RESULT_SUCCESS;
	}
	*pCount = static_cast<uint32_t>(RETURNED_CATEGORIES.size());
	std::ranges::copy(RETURNED_CATEGORIES, pCategories);
	return ZE_RESULT_SUCCESS;
}

ze_result_t zesRasGetStateExp2(zes_ras_handle_t, const uint32_t count, const zes_ras_error_category_exp_t *,
							   zes_ras_state_exp2_t *pState)
{
	for (uint32_t i = 0; i < count; ++i) {
		pState[i].errorCounter = CATEGORY_ERRORS;
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t zesRasGetStateExp(zes_ras_handle_t, uint32_t *pCount, zes_ras_state_exp_t *pState)
{
	if (pState == nullptr) {
		*pCount = CATEGORIES_COUNTED;
		return ZE_RESULT_SUCCESS;
	}
	*pCount = static_cast<uint32_t>(RETURNED_CATEGORIES.size());
	for (std::size_t i = 0; i < RETURNED_CATEGORIES.size(); ++i) {
		pState[i] = {.category = RETURNED_CATEGORIES.at(i), .errorCounter = CATEGORY_ERRORS};
	}
	return ZE_RESULT_SUCCESS;
}

ze_result_t zesRasClearStateExp(zes_ras_handle_t, zes_ras_error_category_exp_t category)
{
	clearedCategories.push_back(category);
	return ZE_RESULT_SUCCESS;
}

} // extern "C"

TEST_CASE("a device's error sets are enumerated on first use and their counters summed")
{
	ras device;
	REQUIRE(device.init(newDevice()) == ZE_RESULT_SUCCESS);
	uint64_t count = 0;
	CHECK(device.getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count) == ZE_RESULT_SUCCESS);
	CHECK(count == CORRECTABLE_RESETS + UNCORRECTABLE_RESETS);
}

// A device, and the ras inside it, is copied while the driver builds its device list.
static_assert(std::is_copy_constructible_v<ras> && std::is_copy_assignable_v<ras>);

TEST_CASE("a device read from many threads at once is enumerated once")
{
	constexpr int threadCount = 8;
	ras device;
	REQUIRE(device.init(newDevice()) == ZE_RESULT_SUCCESS);
	enumerations = 0;
	std::vector<std::future<ze_result_t>> reads;
	for (int i = 0; i < threadCount; ++i) {
		reads.push_back(std::async(std::launch::async, [&device] {
			uint64_t count = 0;
			return device.getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count);
		}));
	}
	for (auto &read : reads) {
		CHECK(read.get() == ZE_RESULT_SUCCESS);
	}
	CHECK(enumerations == 1);
}

TEST_CASE("a copy of a device keeps its error sets and does not enumerate them again")
{
	ras device;
	REQUIRE(device.init(newDevice()) == ZE_RESULT_SUCCESS);
	uint64_t count = 0;
	REQUIRE(device.getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count) == ZE_RESULT_SUCCESS);
	enumerations = 0;
	ras copy{device};
	ras assigned;
	assigned = device;
	for (ras *read : {&copy, &assigned, &device}) {
		count = 0;
		CHECK(read->getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count) == ZE_RESULT_SUCCESS);
		CHECK(count == CORRECTABLE_RESETS + UNCORRECTABLE_RESETS);
	}
	CHECK(enumerations == 0);
}

TEST_CASE("only the categories the second call returns are read, though the first counted more")
{
	ras device;
	REQUIRE(device.init(newDevice()) == ZE_RESULT_SUCCESS);
	std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> states;
	REQUIRE(device.getErrorsPerTileRasExp(states) == ZE_RESULT_SUCCESS);
	for (const auto type : {ZES_RAS_ERROR_TYPE_CORRECTABLE, ZES_RAS_ERROR_TYPE_UNCORRECTABLE}) {
		CAPTURE(type);
		REQUIRE(states[type].size() == RETURNED_CATEGORIES.size());
		for (std::size_t i = 0; i < RETURNED_CATEGORIES.size(); ++i) {
			CHECK(states[type][i].category == RETURNED_CATEGORIES.at(i));
			CHECK(states[type][i].errorCount == CATEGORY_ERRORS);
		}
	}
}

TEST_CASE("only the categories the second call returns are cleared, though the first counted more")
{
	ras device;
	REQUIRE(device.init(newDevice()) == ZE_RESULT_SUCCESS);
	clearedCategories.clear();
	REQUIRE(device.clearErrors() == ZE_RESULT_SUCCESS);
	// Two error sets, each clearing exactly the returned categories; no zeroed RESET from the tail.
	const std::vector<zes_ras_error_category_exp_t> expected{RETURNED_CATEGORIES[0], RETURNED_CATEGORIES[1],
															 RETURNED_CATEGORIES[0], RETURNED_CATEGORIES[1]};
	CHECK(clearedCategories == expected);
}

TEST_CASE("a failed enumeration is reported as its error, not as zero errors or no RAS support")
{
	enumerationResult = ZE_RESULT_ERROR_DEVICE_LOST;
	ras device;
	REQUIRE(device.init(newDevice()) == ZE_RESULT_SUCCESS);
	uint64_t count = 99;
	CHECK(device.getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count) ==
		  ZE_RESULT_ERROR_DEVICE_LOST);
	std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> states;
	CHECK(device.getErrorsPerTileRasExp(states) == ZE_RESULT_ERROR_DEVICE_LOST);
	CHECK(states.empty());
	enumerationResult = ZE_RESULT_SUCCESS;
	// Not retried: only an enumeration cut short by the time limit is.
	CHECK(device.getErrorsPerTileRasExp(states) == ZE_RESULT_ERROR_DEVICE_LOST);
}

TEST_CASE("an enumeration that does not return reads CALL_ABANDONED, not as a device without RAS")
{
	blockEnumeration = true;
	ras stuck;
	REQUIRE(stuck.init(newDevice()) == ZE_RESULT_SUCCESS);
	uint64_t count = 99;
	CHECK(stuck.getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count) == CALL_ABANDONED);
	CHECK(count == 0);
	std::map<zes_ras_error_type_t, std::vector<ras_state_exp_t>> states;
	CHECK(stuck.getErrorsPerTileRasExp(states) == CALL_ABANDONED);
	CHECK(stuck.clearErrors() == CALL_ABANDONED);
	CHECK(boundedCallsAbandoned());

	// Another device's RAS calls would wait on the same runtime state, so they are not made.
	ras other;
	REQUIRE(other.init(newDevice()) == ZE_RESULT_SUCCESS);
	const auto start = std::chrono::steady_clock::now();
	CHECK(other.getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count) == CALL_ABANDONED);
	CHECK(std::chrono::steady_clock::now() - start < 500ms);

	// Once the abandoned enumeration returns, the devices enumerate again.
	{
		blockEnumeration = false;
		releaseEnumeration.set_value();
		CHECK(eventually(stuck, 5000ms));
		CHECK(eventually(other, 5000ms));
		CHECK_FALSE(boundedCallsAbandoned());
		CHECK(stuck.getErrors(ZES_RAS_ERROR_CAT_RESET, ZES_RAS_ERROR_TYPE_FORCE_UINT32, &count) == ZE_RESULT_SUCCESS);
		CHECK(count == CORRECTABLE_RESETS + UNCORRECTABLE_RESETS);
	}
}
