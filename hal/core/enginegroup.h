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
#ifndef _ENGINEGROUP_H
#define _ENGINEGROUP_H

#include "sysman.h"

class LIBXPUM_API enginegroup : public sysman
{
private:
	uint32_t engineGroupCount;
	zes_engine_handle_t *engineGroups;

public:
	enginegroup() : engineGroupCount(0), engineGroups(nullptr) {}
	~enginegroup();
	ze_result_t enumGroups(zes_device_handle_t device);
	ze_result_t getProperties(zes_engine_handle_t engineGroup, zes_engine_properties_t *engineProperties);
	ze_result_t getActivity(zes_engine_handle_t engineGroup, zes_engine_stats_t *engineStats);
	ze_result_t getActivityExt(zes_engine_handle_t engineGroup);
	ze_result_t getUtilization(zes_engine_group_t type, uint64_t *utilization, uint64_t *timestamp);
	ze_result_t getMediaEngines(uint32_t *mediaEngines, zes_engine_group_t type);
	ze_result_t getUtilization(uint32_t *utilization, zes_engine_group_t type);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
