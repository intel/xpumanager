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
#ifndef _VF_H
#define _VF_H

#include "sysman.h"
#include <osvf.h>

class LIBXPUM_API vf : public sysman
{
private:
	uint32_t vfActiveCount, vfEnabledCount;
	zes_vf_handle_t *vfActiveHandles, *vfEnabledHandles;

public:
	vf() : vfActiveCount(0), vfEnabledCount(0), vfActiveHandles(nullptr), vfEnabledHandles(nullptr) {}
	~vf();
	ze_result_t enumActiveVF(zes_device_handle_t device);
	ze_result_t enumEnabledVF(zes_device_handle_t device);
	ze_result_t getVFCapabilities(zes_vf_handle_t vfHandle);
	ze_result_t getVFMemoryUtilization(zes_vf_handle_t vfHandle);
	ze_result_t getVFEngineUtilization(zes_vf_handle_t vfHandle);
	ze_result_t createVFs(DeviceSriovInfo *deviceInfo);

	ze_result_t init(zes_device_handle_t device) override;
	ze_result_t zesRun(zes_device_handle_t device) override;
};

#endif
