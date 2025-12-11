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
#include "sysman.h"

/**
 * @brief Prints a human-readable list of engine types from engine flags
 *
 * This utility function decodes engine type flags and prints the corresponding
 * engine types in a readable format for debugging and informational purposes.
 *
 * @param engines Bitfield containing engine type flags to decode and print
 */
void sysman::printEngines(zes_engine_type_flags_t engines)
{
	if (engines & ZES_ENGINE_TYPE_FLAG_COMPUTE)
		DBG("      - Compute Engine\n");
	if (engines & ZES_ENGINE_TYPE_FLAG_OTHER)
		DBG("      - Other Engine\n");
	if (engines & ZES_ENGINE_TYPE_FLAG_3D)
		DBG("      - 3D Engine\n");
	if (engines & ZES_ENGINE_TYPE_FLAG_MEDIA)
		DBG("      - Media Engine\n");
	if (engines & ZES_ENGINE_TYPE_FLAG_DMA)
		DBG("      - DMA Engine\n");
	if (engines & ZES_ENGINE_TYPE_FLAG_RENDER)
		DBG("      - Render Engine\n");
}

/**
 * @brief Converts a Level Zero error code to a string.
 *
 * @param ret_val The Level Zero error code.
 * @return const char* The string representation of the error code.
 */
LIBXPUM_API const char *l0_error_to_string(ze_result_t retVal)
{
#define CASE(ret_val)                                                                                                  \
	case ret_val:                                                                                                      \
		return #ret_val;
	switch (retVal) {
		CASE(ZE_RESULT_SUCCESS)
		CASE(ZE_RESULT_NOT_READY)
		CASE(ZE_RESULT_ERROR_UNINITIALIZED)
		CASE(ZE_RESULT_ERROR_DEVICE_LOST)
		CASE(ZE_RESULT_ERROR_INVALID_ARGUMENT)
		CASE(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY)
		CASE(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY)
		CASE(ZE_RESULT_ERROR_MODULE_BUILD_FAILURE)
		CASE(ZE_RESULT_ERROR_MODULE_LINK_FAILURE)
		CASE(ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
		CASE(ZE_RESULT_ERROR_NOT_AVAILABLE)
		CASE(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE)
		CASE(ZE_RESULT_ERROR_UNSUPPORTED_VERSION)
		CASE(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE)
		CASE(ZE_RESULT_ERROR_INVALID_NULL_HANDLE)
		CASE(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE)
		CASE(ZE_RESULT_ERROR_INVALID_NULL_POINTER)
		CASE(ZE_RESULT_ERROR_INVALID_SIZE)
		CASE(ZE_RESULT_ERROR_UNSUPPORTED_SIZE)
		CASE(ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT)
		CASE(ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT)
		CASE(ZE_RESULT_ERROR_INVALID_ENUMERATION)
		CASE(ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION)
		CASE(ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT)
		CASE(ZE_RESULT_ERROR_INVALID_NATIVE_BINARY)
		CASE(ZE_RESULT_ERROR_INVALID_GLOBAL_NAME)
		CASE(ZE_RESULT_ERROR_INVALID_KERNEL_NAME)
		CASE(ZE_RESULT_ERROR_INVALID_FUNCTION_NAME)
		CASE(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION)
		CASE(ZE_RESULT_ERROR_INVALID_GLOBAL_WIDTH_DIMENSION)
		CASE(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX)
		CASE(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE)
		CASE(ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE)
		CASE(ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED)
		CASE(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE)
		CASE(ZE_RESULT_ERROR_OVERLAPPING_REGIONS)
		CASE(ZE_RESULT_ERROR_UNKNOWN)
		CASE(ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED)
	default:
		return "Unknown LevelZero error";
	}
}
