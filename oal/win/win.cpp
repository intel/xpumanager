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

#include "os.h"
#include <debug.h>
#include <malloc.h>
#include <stdlib.h>

LIBXPUM_API char *optarg;	// global argument pointer
LIBXPUM_API int optind = 1; // global argv index

/**
 * @brief Windows implementation of getopt for command line argument parsing
 *
 * This function provides Windows compatibility for the POSIX getopt function,
 * enabling standard command line argument parsing on Windows platforms.
 * It processes short options and handles option arguments.
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @param optstring String specifying the legitimate option characters
 * @return int The next option character, EOF when done, or '?' for unknown option
 */
int getopt(int argc, char *argv[], char *optstring)
{
	static char *next = NULL;
	if (optind == 0)
		next = NULL;

	optarg = NULL;

	if (next == NULL || *next == '\0') {
		if (optind == 0)
			optind++;

		if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0') {
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
			return EOF;
		}

		if (strcmp(argv[optind], "--") == 0) {
			optind++;
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
			return EOF;
		}

		next = argv[optind];
		next++; // skip past -
		optind++;
	}

	char c = *next++;
	char *cp = strchr(optstring, c);

	if (cp == NULL || c == ':')
		return '?';

	cp++;
	if (*cp == ':') {
		if (*next != '\0') {
			optarg = next;
			next = NULL;
		} else if (optind < argc) {
			optarg = argv[optind];
			optind++;
		} else {
			return '?';
		}
	}

	return c;
}

/**
 * @brief Windows implementation of getopt_long for extended command line parsing
 *
 * This function provides Windows compatibility for the GNU getopt_long function,
 * enabling parsing of both short and long command line options. It handles
 * required and optional arguments for long options.
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @param optstring String specifying the legitimate short option characters
 * @param longopts Array of option structures for long options
 * @param longindex Pointer to store the index of the matched long option
 * @return int The option character, -1 when done, or '?' for unknown option
 */
int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex)
{
	if (optind >= argc) {
		return -1;
	}

	char *current_arg = argv[optind];
	if (current_arg[0] != '-') {
		return -1;
	}

	// Check for long options
	if (current_arg[1] == '-') {
		current_arg += 2; // Skip the '--'
		for (int i = 0; longopts[i].name != nullptr; ++i) {
			if (strcmp(current_arg, longopts[i].name) == 0) {
				if (longopts[i].has_arg && optind + 1 < argc) {
					optarg = argv[++optind];
				}
				if (longindex) {
					*longindex = i;
				}
				++optind;
				return longopts[i].val;
			}
		}
	} else {
		// Check for short options
		current_arg += 1; // Skip the '-'
		char *opt_pos = strchr((char *)optstring, *current_arg);
		if (opt_pos) {
			if (*(opt_pos + 1) == ':') {
				if (optind + 1 < argc) {
					optarg = argv[++optind];
				} else {
					return '?'; // Missing argument
				}
			}
			++optind;
			return *current_arg;
		}
	}

	++optind;
	return '?'; // Unknown option
}

/**
 * @brief Windows implementation of aligned memory allocation
 *
 * This function provides a Windows-compatible wrapper for aligned memory allocation.
 * Currently implemented as a simple malloc wrapper, but could be extended to
 * provide actual aligned allocation if needed.
 *
 * @param size Size of memory block to allocate in bytes
 * @return void* Pointer to allocated memory block, or NULL on failure
 */
void *align_alloc(size_t size) { return malloc(size); }

/**
 * @brief Opens an I2C device for communication on Windows platform
 *
 * This function establishes communication with the AMC (Advanced Management Controller)
 * device on Windows systems by opening the device handle. It provides Windows-specific
 * I2C access capabilities for hardware sensor monitoring and device communication.
 * The function uses Windows CreateFileW API to open the AMC device path.
 *
 * @param deviceName String reference containing the device name (currently unused on Windows)
 * @return long long File handle if successful, -1 on error
 */
long long openI2C(UNUSED const std::string &deviceName)
{
	HANDLE amchandle = nullptr;

	// Open the device handle
	amchandle =
		CreateFileW(AMC_PATH, (GENERIC_READ | GENERIC_WRITE), 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);

	if (amchandle == INVALID_HANDLE_VALUE) {
		ERR("Couldn't open AMC device handle - %d\n", GetLastError());
		return -1;
	}

	DBG("Successfully opened AMC device handle %ls\n", AMC_PATH);
	return (long long)amchandle;
}

/**
 * @brief Closes an I2C device handle on Windows platform
 *
 * This function safely closes the specified I2C device handle on Windows systems,
 * releasing system resources and terminating the communication channel with the
 * AMC device. It uses the Windows CloseHandle API to properly clean up the handle
 * and includes error checking to ensure successful closure.
 *
 * @param fd File handle of the I2C device to close
 * @return int 0 on success, -1 on error (invalid handle or closure failure)
 */
int closeI2C(long long fd)
{
	if (fd == -1) {
		ERR("Invalid file descriptor\n");
		return -1;
	}
	if (CloseHandle((HANDLE)fd) == 0) {
		ERR("Failed to close I2C handle - %d\n", GetLastError());
		return -1;
	}
	DBG("Successfully closed I2C handle\n");
	return 0;
}