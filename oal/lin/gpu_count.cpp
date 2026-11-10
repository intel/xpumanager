/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "gpu_count.h"
#include "pci_sysfs.h"

/**
 * @brief Counts the GPUs Level Zero will report, from sysfs; see countLevelZeroGpus().
 *
 * @retval count  The number of GPUs; 0 when sysfs cannot be read.
 */
std::size_t hostGpuCount() noexcept
{
	try {
		return countLevelZeroGpus();
	} catch (...) {
		// An allocation failure only costs the per-device share of the default timeout.
		return 0;
	}
}
