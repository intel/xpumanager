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

#include "amcupd.h"
#include <debug.h>
#include <os.h>

ze_result_t amcupd::preUpdateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();
	std::string amc = "amc";
	DBG("Pre-update AMC firmware...\n");

	deviceHandle = OPENI2C(amc);
	if (deviceHandle < 0) {
		ERR("Failed to open I2C device for AMC\n");
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	// Open the I2C device
	return ZE_RESULT_SUCCESS;
}

ze_result_t amcupd::updateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();
	DBG("Updating AMC firmware...\n");
	return ZE_RESULT_SUCCESS;
}

ze_result_t amcupd::postUpdateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();
	DBG("Post-update AMC firmware...\n");
	// Close the I2C device
	CLOSEI2C(deviceHandle);
	return ZE_RESULT_SUCCESS;
}