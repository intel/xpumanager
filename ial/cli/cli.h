/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CLI_H
#define _CLI_H

#include <cmds.h>
#include <memory>

enum class OSTYPE
{
	WINDOWS,
	LINUX,
	BOTH
};

struct function_entry
{
	std::unique_ptr<cmds> (*createFunc)(); // factory: returns a new command instance
	OSTYPE osType;						   // LINUX, WINDOWS, or BOTH
};

#endif
