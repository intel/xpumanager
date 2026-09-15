/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_DYNLOAD_H
#define OAL_DYNLOAD_H

/**
 * @brief Resolves a symbol from the Level Zero runtime library.
 *
 * Searches the already-loaded Level Zero loader for @p name and returns its
 * address as a @c void*, or @c nullptr if the symbol is not present (e.g. the
 * installed runtime is older than the SDK version that introduced it).
 * Callers must use @c std::bit_cast to convert the result to the target
 * function-pointer type; @c reinterpret_cast between data and function
 * pointers is undefined behaviour.
 *
 * @param name  Null-terminated symbol name.
 * @return Address of the symbol, or @c nullptr if not found.
 */
[[nodiscard]] void *resolveL0Sym(const char *name) noexcept;

#endif // OAL_DYNLOAD_H
