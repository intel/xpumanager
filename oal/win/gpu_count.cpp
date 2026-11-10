/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gpu_count.h"

/**
 * @brief Counts the Intel GPUs; not implemented on Windows yet.
 *
 * The default watchdog timeout then has no per-device share.
 *
 * @retval 0  Always.
 */
std::size_t hostGpuCount() noexcept { return 0; }
