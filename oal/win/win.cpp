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
#include <psapi.h>
#include <stdlib.h>

LIBXPUM_API char *optarg;	// global argument pointer
LIBXPUM_API int optind = 1; // global argv index

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

void *align_alloc(size_t size) { return malloc(size); }

string getProcessName(uint32_t processId)
{
	string processName = "<unknown>";

	// Open the process with the necessary access rights
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
	if (hProcess) {
		HMODULE hMod;
		DWORD cbNeeded;

		// Get the first module in the process, which is typically the executable
		if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
			char szProcessName[MAX_PATH];
			// Get the process name
			if (GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(char))) {
				processName = szProcessName;
			}
		}
		// Close the process handle
		CloseHandle(hProcess);
	} else {
		ERR("Unable to open process with ID: %d\n", processId);
	}

	return processName;
}

long long openI2C(const string &deviceName)
{
	HANDLE amchandle = nullptr;
	// For now, the device name is not used because we have a hardcoded path
	UNUSED(deviceName);

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
