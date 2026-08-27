/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef UTILITY_SCOPE_EXIT_H
#define UTILITY_SCOPE_EXIT_H

#include <concepts>
#include <type_traits>
#include <utility>

/**
 * @brief Executes a callable unconditionally when the enclosing scope exits.
 *
 * Use for RAII cleanup of resources that do not have a dedicated wrapper type (e.g. opaque C-API handles).
 *
 * Construction is via CTAD:
 * @code
 *     ScopeExit guard{[&] { cleanup(); }};
 * @endcode
 *
 *
 * @tparam F Any callable satisfying std::invocable<F>.
 */
template <typename F>
    requires std::invocable<F &>
class ScopeExit
{
public:
	explicit ScopeExit(F f) noexcept(std::is_nothrow_move_constructible_v<F>) : fn(std::move(f)) {}

	ScopeExit(const ScopeExit &) = delete;
	ScopeExit(ScopeExit &&) = delete;
	ScopeExit &operator=(const ScopeExit &) = delete;
	ScopeExit &operator=(ScopeExit &&) = delete;

	~ScopeExit() noexcept
	{
		try {
			fn();
		} catch (...) {
		}
	}

private:
	F fn;
};

#endif // UTILITY_SCOPE_EXIT_H
