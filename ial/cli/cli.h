/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CLI_H
#define _CLI_H

#include <cmds.h>
#include <functional>

// Enum to represent OS types
enum class OSTYPE
{
	WINDOWS,
	LINUX,
	BOTH,
};

/* Structure to hold function and OS type */
struct function_entry
{
	std::function<cmds *()> createFunc;
	DAEMONCAP daemonCap;
	OSTYPE osType;
};

#endif
