/*
 * Copyright (C) 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include <os.h>
#include <debug.h>
#include <malloc.h>
#include <psapi.h>
#include <stdlib.h>

/**
 * @brief Creates a new thread on Windows platform
 *
 * This function creates a new thread using the Windows CreateThread API,
 * wrapping the thread handle in a thread_id object for later management.
 *
 * @param thread Function pointer to the thread function
 * @param args Arguments to pass to the thread function
 * @return thread_id* Pointer to the created thread_id object
 */
thread_id *create_thread(funcptr thread, void *args)
{
	HANDLE thread_hdl;
	thread_hdl = CreateThread(NULL, NULL, *thread, args, NULL, NULL);
	thread_id *new_thread_id = new thread_id(thread_hdl);
	return new_thread_id;
}

/**
 * @brief Waits for the specified thread to complete on Windows platform
 *
 * This function blocks until the specified thread has finished execution,
 * then cleans up the thread_id object.
 *
 * @param tid Pointer to the thread_id object representing the thread to wait for
 */
void wait_for_thread(thread_id *tid)
{
	WaitForSingleObject(tid->ret_thread_uid(), INFINITE);
	delete tid; // Clean up the thread_id object after waiting
}

/**
 * @brief Generates a timestamp string for logging and diagnostic purposes (Windows)
 *
 * This function creates a formatted timestamp string using the current system
 * time on Windows. It provides consistent time formatting for logging, debugging,
 * and diagnostic operations throughout the XPUM system.
 *
 * @return std::string Formatted timestamp string representing current system time
 */
std::string timestamp()
{
	std::string timestamp_str = "";
	SYSTEMTIME st;
	GetLocalTime(&st);
	char buffer[64];
	sprintf_s(buffer, sizeof(buffer), "%02d:%02d:%02d.%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	timestamp_str = buffer;
	return timestamp_str;
}

/**
 * @brief Retrieves the process name for a given process ID (Windows)
 *
 * This function reads the executable name of the process with the specified
 * process ID using Windows APIs. It is used for process identification in
 * system monitoring and management operations.
 *
 * @param processId 32-bit unsigned integer representing the target process ID
 * @return std::string Process name if found, "<unknown>" if not found or error occurs
 */
std::string getProcessName(uint32_t processId)
{
	std::string processName = "<unknown>";
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