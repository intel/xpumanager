/*
 * Copyright © 2025 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <cmd_agentset.h>
#include <cmd_amcsensor.h>
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

string progName = "xpumd";

int main(int argc, char *argv[])
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