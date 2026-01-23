/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <cmd_agentset.h>
#include <cmd_config.h>
#include <cmd_diag.h>
#include <cmd_discovery.h>
#include <cmd_dump.h>
#include <cmd_group.h>
#include <cmd_health.h>
#include <cmd_log.h>
#include <cmd_policy.h>
#include <cmd_ps.h>
#include <cmd_stats.h>
#include <cmd_topdown.h>
#include <cmd_topology.h>
#include <cmd_updatefw.h>
#include <cmd_vgpu.h>
#include <debug.h>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <os.h>
#include <vector>

std::string progName = "xpumd";

#ifdef _DEBUG
int dbgLvl = DBG;
#else
int dbgLvl = INFO;
#endif

/**
 * @brief Main entry point for the XPU Manager daemon
 *
 * This function initializes the XPU Manager daemon (xpumd) by setting up
 * the Level Zero Sysman driver interface and preparing the system for
 * GPU monitoring and management operations. It handles driver initialization
 * and provides appropriate feedback on startup status.
 *
 * @param argc Number of command line arguments (currently unused)
 * @param argv Array of command line argument strings (currently unused)
 * @return int Exit status: 0 on success, non-zero on failure
 */
int main(UNUSED int argc, UNUSED char *argv[])
{
	arg_struct arg;

	// Create sysman driver instance
	ze_result_t result = arg.sm.init();
	switch (result) {
	case ZE_RESULT_SUCCESS:
		PRINT("Sysman driver initialized successfully.\n");
		break;
	default:
		ERR("Sysman driver initialization failed.\n");
		return -1;
		break;
	}

	if (arg.sm.run() != ZE_RESULT_SUCCESS) {
		ERR("Sysman driver run failed.\n");
		return -1;
	}
	return 0;
}