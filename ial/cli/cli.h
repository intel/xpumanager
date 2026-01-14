/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _CLI_H
#define _CLI_H

#include <cmds.h>
#include <functional>

#define STRINGIFY(x) #x
#define CONCATENATE_WITH_DOT(a, b) "v" STRINGIFY(a) "." STRINGIFY(b)
#define CONCATENATE_WITH_DOTS(a, b, c, d) STRINGIFY(a) "." STRINGIFY(b) "." STRINGIFY(c) "." STRINGIFY(d)
#define GET_SHORT_VERSION() CONCATENATE_WITH_DOT(MAJOR, MINOR)
#define GET_FULL_VERSION() CONCATENATE_WITH_DOTS(MAJOR, MINOR, PATCH, BUILD_NUMBER)

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