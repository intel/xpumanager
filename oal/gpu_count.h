/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * How many GPUs the host has, read from the OS without Level Zero, so the watchdog can size its
 * default timeout before the driver is brought up.
 */

#pragma once

#include <cstddef>

/**
 * @brief Counts the GPUs Level Zero will report, without initializing it.
 *
 * Linux reads sysfs (countLevelZeroGpus() in oal/lin/pci_sysfs.h). Windows has no implementation
 * yet and reports none, so its default timeout has no per-GPU share.
 *
 * @retval count  The number of GPUs; 0 when the OS cannot tell.
 */
[[nodiscard]] std::size_t hostGpuCount() noexcept;
