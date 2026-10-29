/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "bounded_call.h"

/**
 * @brief Logs a bound group's breaker trips and resets at debug level; a CircuitBreaker observer.
 *
 * Debug level only: the values the group could not read say why where they are reported.
 *
 * @param[in] group The group, e.g. "RAS".
 * @param[in] limit The group's limit.
 * @param[in] event What happened.
 */
void logBoundedCallEvent(std::string_view group, std::chrono::milliseconds limit, xpum::BreakerEvent event) noexcept
{
	if (event == xpum::BreakerEvent::Tripped) {
		DBG("A {} call did not return within {} ms; {} calls are not made until it does\n", group, limit.count(),
			group);
	} else {
		DBG("The abandoned {} call returned; {} calls are made again\n", group, group);
	}
}

/**
 * @brief Whether a call of any bound group in the library is abandoned and still running.
 *
 * Defined here rather than in the header so that it reads the library's breakers, whichever
 * module asks.
 *
 * @retval true  A call is abandoned and has not returned.
 * @retval false None is.
 */
bool boundedCallsAbandoned() noexcept { return xpum::CircuitBreaker::anyAbandoned(); }
