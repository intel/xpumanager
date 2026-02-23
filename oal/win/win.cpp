/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>
#include <debug.h>
#include <malloc.h>
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <iostream>
#include <syncstream>

// Define the variables without LIBXPUM_API to avoid DLL linkage issues
char *optarg = nullptr; // global argument pointer
int optind = 1;			// global argv index

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

	if (optind == 0) {
		optind = 1;
		next = NULL;
	}

	optarg = NULL;

	if (next == NULL || *next == '\0') {
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
	// Initialize optind if it hasn't been initialized yet
	if (optind == 0) {
		optind = 1;
	}

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
		ERR("Couldn't open AMC device handle - %lu\n", GetLastError());
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
		ERR("Failed to close I2C handle - %lu\n", GetLastError());
		return -1;
	}
	DBG("Successfully closed I2C handle\n");
	return 0;
}

/**
 * @brief Get NUMA node information for a PCI device on Windows (Simplified)
 * @param bdf PCI Bus:Device.Function string (e.g., "0000:00:02.0")
 * @return NUMA node number, or -1 if not found
 */
static int getDeviceNumaNode(std::string bdf)
{
	TRACING();

	// Simplified approach: Use GetNumaHighestNodeNumber to get available NUMA nodes
	// Then use heuristics or default to node 0 for most devices
	ULONG highestNodeNumber = 0;
	if (!GetNumaHighestNodeNumber(&highestNodeNumber)) {
		DBG("GetNumaHighestNodeNumber failed: %lu\n", GetLastError());
		return 0; // Default to NUMA node 0
	}

	// For now, use a simple heuristic based on the PCI bus number
	// In a real implementation, you would:
	// 1. Parse the BDF to extract bus, device, function
	// 2. Query registry for device location information
	// 3. Map to appropriate NUMA node

	// Extract bus number from BDF (format: "DDDD:BB:DD.F")
	size_t colonPos = bdf.find(':', 5); // Skip first domain part
	if (colonPos != std::string::npos && colonPos + 3 < bdf.length()) {
		std::string busStr = bdf.substr(colonPos + 1, 2);
		try {
			int busNumber = std::stoi(busStr, nullptr, 16);
			// Simple heuristic: distribute across available NUMA nodes
			return busNumber % (highestNodeNumber + 1);
		} catch (const std::exception &) {
			// Fall through to default
		}
	}

	return 0; // Default to NUMA node 0
}

/**
 * @brief Convert NUMA node to CPU affinity mask on Windows
 * @param numaNode NUMA node number
 * @return CPU affinity mask as hexadecimal string
 */
static std::string numaToCpuMask(int numaNode)
{
	TRACING();

	if (numaNode < 0) {
		return "";
	}

	// Get processor mask for the NUMA node
	ULONGLONG processorMask = 0;
	BOOL result = GetNumaNodeProcessorMask((UCHAR)numaNode, &processorMask);

	if (!result) {
		ERR("GetNumaNodeProcessorMask failed: %lu\n", GetLastError());
		return "";
	}

	// Convert mask to hexadecimal string (similar to Linux format)
	std::stringstream ss;
	ss << std::hex << (processorMask & 0xFFFFF);
	return ss.str();
}

/**
 * @brief Convert NUMA node to CPU list on Windows
 * @param numaNode NUMA node number
 * @return Comma-separated list of CPU numbers
 */
static std::string numaToCpuList(int numaNode)
{
	TRACING();

	if (numaNode < 0) {
		return "";
	}

	// Get processor mask for the NUMA node
	ULONGLONG processorMask = 0;
	BOOL result = GetNumaNodeProcessorMask((UCHAR)numaNode, &processorMask);

	if (!result) {
		ERR("GetNumaNodeProcessorMask failed: %lu\n", GetLastError());
		return "";
	}

	// Convert mask to CPU list
	std::vector<int> cpuList;
	for (int i = 0; i < 64; i++) { // 64-bit mask
		if (processorMask & (1ULL << i)) {
			cpuList.push_back(i);
		}
	}

	// Format as first-last CPU (e.g., "0-7")
	std::stringstream ss;
	if (!cpuList.empty()) {
		ss << cpuList.front();
		if (cpuList.size() > 1) {
			ss << "-" << cpuList.back();
		}
	}

	return ss.str();
}

/**
 * @brief Windows equivalent of Linux getLocalCpus()
 * @param bdf PCI Bus:Device.Function string
 * @return CPU affinity mask as hexadecimal string
 */
std::string getLocalCpus(const std::string &bdf)
{
	TRACING();

	// Get NUMA node for the device
	int numaNode = getDeviceNumaNode(bdf);

	// Convert NUMA node to CPU mask
	return numaToCpuMask(numaNode);
}

/**
 * @brief Windows equivalent of Linux getCpuList()
 * @param bdf PCI Bus:Device.Function string
 * @return Comma-separated list of local CPU numbers
 */
std::string getCpuList(const std::string &bdf)
{
	TRACING();

	// Get NUMA node for the device
	int numaNode = getDeviceNumaNode(bdf);

	// Convert NUMA node to CPU list
	return numaToCpuList(numaNode);
}

/**
 * @brief Sets the progress of a firmware update operation.
 *
 * This function updates the progress of a firmware update operation by printing
 * the progress to the console. It uses ANSI escape codes to control the cursor
 * position in the terminal.
 *
 * @param devIndex The device index.
 * @param lineNum The line number to update (1-based).
 * @param totalThreads The total number of threads (or steps) in the update process.
 * @param progress The current progress percentage (0-100).
 */
void setProgress(int devIndex, int lineNum, int totalThreads, uint32_t progress)
{
	TRACING();
	std::lock_guard<std::mutex> lock(progressPrintMutex);

	// Use std::osyncstream for thread-safe output (C++20)
	std::osyncstream sync_out(std::cout);

	// Windows console handling
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hConsole != INVALID_HANDLE_VALUE) {
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
			// Calculate target row (current row - lines_up)
			int lines_up = totalThreads - lineNum;
			SHORT targetRow = csbi.dwCursorPosition.Y - (SHORT)lines_up;
			if (targetRow < 0)
				targetRow = 0;

			// Move cursor to target position
			COORD pos = {0, targetRow};
			SetConsoleCursorPosition(hConsole, pos);

			// Clear the line and print progress
			sync_out << "\rFirmware progress for device " << devIndex << ": ";
			for (int j = 0; j < (int)progress; ++j) {
				sync_out << "#";
			}
			sync_out << " " << progress << "%";

			// Restore cursor to original position
			pos = {csbi.dwCursorPosition.X, csbi.dwCursorPosition.Y};
			SetConsoleCursorPosition(hConsole, pos);
		}
	}
	sync_out.flush();
}
