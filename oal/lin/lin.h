/*
 *
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _LIN_H
#define _LIN_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <getopt.h>
#include <pthread.h>
#include <string>
#include <unistd.h>
#include <sys/time.h>
#include <vector>

/**
 * @brief A class to encapsulate the result of executing a system command
 *
 * This class stores both the output and exit status of a system command
 * execution, providing a clean interface to access command results.
 * It's used by the execCommand function to return structured results.
 */
class SystemCommandResult
{
	std::string _output;
	int _exitStatus;

public:
	SystemCommandResult(std::string &cmdOutput, int cmdExitStatus)
	{
		_output = cmdOutput;
		_exitStatus = cmdExitStatus;
	}

	SystemCommandResult(const std::string &cmdOutput, int cmdExitStatus)
	{
		_output = cmdOutput;
		_exitStatus = cmdExitStatus;
	}

	const std::string &output() { return _output; }

	int exitStatus() { return _exitStatus; }
};

SystemCommandResult execCommand(const std::string &command);
std::string findResourceFile(const std::string &relativePath);

#endif
