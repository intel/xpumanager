/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Windows stub for proc_fdinfo — /proc does not exist on Windows.
 * All functions return empty / zero / all-nullopt results so callers
 * degrade gracefully without any platform guards.
 */

#include "../proc_fdinfo.h"

namespace fdinfo {

std::vector<ProcessSnapshot> capture(const std::string & /*pciAddr*/, const std::string & /*procRoot*/) { return {}; }

PidUtilMap delta(const std::vector<ProcessSnapshot> & /*before*/, const std::vector<ProcessSnapshot> & /*after*/)
{
	return {};
}

ProcUtil toProcUtil(const EngineUtilMap & /*engineUtils*/) { return {}; }

uint64_t enginesFromSnapshot(const ProcessSnapshot & /*snap*/) { return 0; }

ProcUtil aggregateDeviceUtil(const std::vector<ProcessSnapshot> & /*before*/,
							 const std::vector<ProcessSnapshot> & /*after*/)
{
	return {};
}

} // namespace fdinfo
