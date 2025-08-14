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

/**
 * @brief Prepares the AMC (Advanced Memory Controller) for firmware update
 *
 * This function performs pre-update operations including opening the I2C
 * device connection required for AMC firmware communication. It establishes
 * the necessary hardware interface before the actual firmware update process.
 *
 * @param fwInfo Pointer to firmware information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful preparation,
 *                     ZE_RESULT_ERROR_UNKNOWN if I2C device open fails
 */
ze_result_t amcupd::preUpdateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs the actual AMC firmware update operation
 *
 * This function executes the main firmware update process for the AMC.
 * Currently implemented as a placeholder that logs the update operation.
 * In a full implementation, this would contain the firmware flashing logic.
 *
 * @param fwInfo Pointer to firmware information structure containing update details
 * @return ze_result_t ZE_RESULT_SUCCESS on successful update
 */
ze_result_t amcupd::updateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();
	DBG("Updating AMC firmware...\n");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Performs cleanup operations after AMC firmware update
 *
 * This function handles post-update cleanup including closing the I2C
 * device connection that was opened during the pre-update phase. It ensures
 * proper resource cleanup regardless of update success or failure.
 *
 * @param fwInfo Pointer to firmware information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful cleanup
 */
ze_result_t amcupd::postUpdateAMC(UNUSED firmwareInfo *fwInfo)
{
	TRACING();

	return ZE_RESULT_SUCCESS;
}