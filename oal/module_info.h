/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef OAL_MODULE_INFO_H
#define OAL_MODULE_INFO_H

#include <optional>
#include <string>
#include <vector>

/**
 * @brief Returns the path of the shared library that provides @p symbol.
 *
 * Answers "which copy of the library am I actually running against", which a
 * version number alone cannot: a Conan-cached loader shadowing the system one
 * reports the same version string.
 *
 * @param[in] symbol Address of any symbol (function or data) in the target module.
 *                   Not const-qualified because dladdr() predates const
 *                   correctness; neither implementation writes through it.
 *
 * @retval path          Absolute path of the module containing @p symbol.
 * @retval std::nullopt  @p symbol is null, or the OS could not attribute the
 *                       address to a module with a filename.
 */
[[nodiscard]] std::optional<std::string> modulePathForSymbol(void *symbol) noexcept;

/**
 * @brief Returns libraries force-loaded ahead of the normal search order.
 *
 * Linux: the entries of LD_PRELOAD, whether or not they were resolvable. These
 * silently replace symbols in the graphics stack, so a version report that
 * omits them can describe a build that is not the one running.
 *
 * @retval entries  The preloaded libraries, in load order, as given (not resolved).
 * @retval {}       Nothing preloaded, or the platform has no equivalent mechanism
 *                  (Windows).
 */
[[nodiscard]] std::vector<std::string> interposedModules();

#endif // OAL_MODULE_INFO_H
