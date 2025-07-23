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

thread_id *create_thread(funcptr thread, void *args)
{
	HANDLE thread_hdl;

	thread_hdl = CreateThread(NULL, NULL, *thread, args, NULL, NULL);

	thread_id *new_thread_id = new thread_id(thread_hdl);

	return new_thread_id;
}

void wait_for_thread(thread_id *tid)
{
	WaitForSingleObject(tid->ret_thread_uid(), INFINITE);
	delete tid; // Clean up the thread_id object after waiting
}

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
