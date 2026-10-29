/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef HAL_CORE_BOUNDED_CALL_H
#define HAL_CORE_BOUNDED_CALL_H

/**
 * Level Zero calls with a time limit, for API groups that can block forever.
 *
 * To bound a group: give it one xpum::CircuitBreaker for the process, e.g.
 *
 *     xpum::CircuitBreaker &powerCalls() {
 *         static xpum::CircuitBreaker breaker{"power", POWER_CALL_LIMIT, logBoundedCallEvent};
 *         return breaker;
 *     }
 *
 * then make each of its calls through boundedGet() (the usual fn(handle, ..., T *out) shape) or
 * boundedCall() (anything else). A call that misses the limit reads CALL_ABANDONED.
 */

#include "sysman.h"
#include "utility/circuit_breaker/circuit_breaker.h"

#include <chrono>
#include <concepts>
#include <new>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

/**
 * The result of a call abandoned because it missed its group's limit, or not made because an
 * earlier call of the group did. Callers report it as "the driver did not respond".
 *
 * Not a Level Zero result, so it cannot be mistaken for one a getter returns itself, as
 * ZE_RESULT_NOT_READY is: Level Zero assigns 0x70xxxxxx and 0x78xxxxxx to errors and 0x7ffxxxxx to
 * experimental results; 0x7c000001 is in the gap between them.
 */
inline constexpr auto CALL_ABANDONED = static_cast<ze_result_t>(0x7c000001);

/** A bounded call's result: a ze_result_t, or a struct whose ze_result_t @c status says how it went. */
template <typename R>
concept BoundedResult = std::same_as<R, ze_result_t> || (std::default_initializable<R> && requires(R r) {
							{ r.status } -> std::same_as<ze_result_t &>;
						});

/**
 * @brief Logs a bound group's breaker trips and resets at debug level; a CircuitBreaker observer.
 *
 * @param[in] group The group, e.g. "RAS".
 * @param[in] limit The group's limit.
 * @param[in] event What happened.
 */
LIBXPUM_API void logBoundedCallEvent(std::string_view group, std::chrono::milliseconds limit,
									 xpum::BreakerEvent event) noexcept;

/**
 * @brief Whether a call of any bound group in the library is abandoned and still running.
 *
 * Such a call may still be inside Level Zero, so the process should end without tearing the
 * runtime down under it.
 *
 * @retval true  A call is abandoned and has not returned.
 * @retval false None is.
 */
[[nodiscard]] LIBXPUM_API bool boundedCallsAbandoned() noexcept;

namespace bounded_call_detail {

template <BoundedResult R> [[nodiscard]] R withStatus(ze_result_t status)
{
	if constexpr (std::same_as<R, ze_result_t>) {
		return status;
	} else {
		R result{};
		result.status = status;
		return result;
	}
}

} // namespace bounded_call_detail

/**
 * @brief Makes @p call through @p breaker, with the breaker's limit.
 *
 * @param[in] breaker The group's breaker.
 * @param[in] call  The call; see xpum::AbandonableCall for what it may capture.
 *
 * @retval result                            What @p call returned.
 * @retval CALL_ABANDONED                    @p call missed the limit, or an earlier call of the
 *                                           group did; as the result or as its status.
 * @retval ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY The call could not be queued or the worker started.
 */
template <xpum::AbandonableCall F>
	requires BoundedResult<std::invoke_result_t<F &>>
[[nodiscard]] std::invoke_result_t<F &> boundedCall(xpum::CircuitBreaker &breaker, F &&call)
{
	using Result = std::invoke_result_t<F &>;
	try {
		if (auto result = breaker.run(std::forward<F>(call))) {
			return std::move(*result);
		}
	} catch (const std::bad_alloc &) {
		return bounded_call_detail::withStatus<Result>(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);
	} catch (const std::system_error &) {
		return bounded_call_detail::withStatus<Result>(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY);
	}
	return bounded_call_detail::withStatus<Result>(CALL_ABANDONED);
}

/**
 * @brief Makes a Level Zero getter of the form fn(args..., T *out) through @p breaker.
 *
 * The getter fills a copy of @p out owned by the call, so an abandoned call never writes to the
 * caller's memory; the copy is written back only on success. A @p out with an extension chain
 * is refused, since the call would write through the caller's pNext.
 *
 * @tparam Fn The getter, e.g. zesRasGetProperties.
 * @param[in]     breaker The group's breaker.
 * @param[in,out] out   What the getter fills; its stype is passed in.
 * @param[in]     args  The getter's arguments before @p out, by value.
 *
 * @retval ZE_RESULT_SUCCESS                   @p out holds the answer.
 * @retval ZE_RESULT_ERROR_INVALID_NULL_POINTER @p out is null.
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT    @p out has a pNext chain.
 * @retval CALL_ABANDONED                      See boundedCall().
 * @retval Other                               What the getter returned; @p out is unchanged.
 */
template <auto Fn, typename T, typename... Args>
	requires std::is_trivially_copyable_v<T> && std::default_initializable<T> &&
			 std::same_as<std::invoke_result_t<decltype(Fn), Args..., T *>, ze_result_t>
[[nodiscard]] ze_result_t boundedGet(xpum::CircuitBreaker &breaker, T *out, Args... args)
{
	if (out == nullptr) {
		return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
	}
	if constexpr (requires { out->pNext; }) {
		if (out->pNext != nullptr) {
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}
	struct Answer
	{
		ze_result_t status{};
		T value{};
	};
	auto answer = boundedCall(breaker, [in = *out, args...] {
		Answer filled{.status = ZE_RESULT_SUCCESS, .value = in};
		filled.status = Fn(args..., &filled.value);
		return filled;
	});
	if (answer.status == ZE_RESULT_SUCCESS) {
		*out = answer.value;
	}
	return answer.status;
}

#endif /* HAL_CORE_BOUNDED_CALL_H */
