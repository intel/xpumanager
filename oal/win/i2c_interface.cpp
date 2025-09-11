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

#include "i2c_interface.h"
#include "os.h"
#include <debug.h>

#define MAX_BUFFER_SIZE (64 * 1024)

/**
 * @brief RAII wrapper for Windows event handles
 *
 * Ensures automatic cleanup of event handles to prevent resource leaks.
 * Used for overlapped I/O operations in I2C communication.
 */
class EventHandle
{
private:
	HANDLE handle;

public:
	EventHandle() : handle(CreateEvent(NULL, TRUE, FALSE, NULL)) {}

	~EventHandle()
	{
		if (handle != NULL) {
			CloseHandle(handle);
		}
	}

	// Non-copyable
	EventHandle(const EventHandle &) = delete;
	EventHandle &operator=(const EventHandle &) = delete;

	// Get the handle
	HANDLE get() const { return handle; }

	// Check if valid
	bool isValid() const { return handle != NULL; }
};

/**
 * @brief Constructor for I2CInterface class on Windows platform
 *
 * Initializes the I2C interface for communication with AMC devices on Windows.
 * Opens the specified device path and establishes communication channel.
 *
 * @param devpath Wide character string containing the device path (e.g., L"\\\\.\\NF_I2C_BUS_0x...")
 *
 * @note Sets init flag to true on successful initialization, false otherwise
 * @note Uses Windows-specific file handles and overlapped I/O for device communication
 */
I2CInterface::I2CInterface(const std::string &devpath)
{
	TRACING();

	amchandle = NULL;
	init = false;

	if (openAmc(devpath) && open_amc_peripheral()) {
		init = true;
	} else {
		ERR("Failed to open AMC device handle\n");
		init = false;
	}
}

/**
 * @brief Destructor for I2CInterface class
 *
 * Cleans up resources by closing the AMC device handle if it was successfully opened.
 * Ensures proper cleanup of Windows file handles to prevent resource leaks.
 *
 * @note Uses Windows CloseHandle() API to close the device handle
 */
I2CInterface::~I2CInterface()
{
	TRACING();
	if (amchandle != NULL) {
		CloseHandle(amchandle);
	}
}

/**
 * @brief Opens AMC device handle using Windows CreateFileW API
 *
 * Creates a file handle for the specified AMC device path with read/write access
 * and overlapped I/O capabilities for asynchronous operations.
 *
 * @param devpath Wide character string containing the device path to open
 * @return true if device opened successfully, false on failure
 *
 * @note Uses FILE_FLAG_OVERLAPPED for asynchronous I/O operations
 * @note Sets amchandle to the created file handle on success
 */
bool I2CInterface::openAmc(const std::string &devpath)
{
	TRACING();
	if (devpath.empty()) {
		ERR("Device path is empty\n");
		return false;
	}

	// Convert UTF-8/narrow std::string to wide string (naive widening)
	std::wstring wdevpath(devpath.begin(), devpath.end());
	DBG("openAmc called with devpath: %ls\n", wdevpath.c_str());
	amchandle = CreateFileW(wdevpath.c_str(), (GENERIC_READ | GENERIC_WRITE), 0, nullptr, OPEN_EXISTING,
							FILE_FLAG_OVERLAPPED, nullptr);

	if (amchandle == INVALID_HANDLE_VALUE) {
		ERR("Error openAmc - %d\n", GetLastError());
		return false;
	}

	DBG("openAmc Success\n");
	return true;
}

/**
 * @brief Opens AMC peripheral for I2C communication
 *
 * Uses Windows DeviceIoControl with IOCTL_SPBTESTTOOL_OPEN to establish
 * communication channel with the AMC peripheral device.
 *
 * @return true if peripheral opened successfully, false on failure
 *
 * @note Uses overlapped I/O structure for asynchronous operation
 * @note Expects ERROR_IO_PENDING as normal behavior for this IOCTL
 */
bool I2CInterface::open_amc_peripheral()
{
	TRACING();
	ULONG bytesReturned;
	ULONG peripheralindex = 0;
	OVERLAPPED overlapped = {};

	if (amchandle == nullptr) {
		ERR("File is null\n");
		return false;
	}

	// amchandle was opened with the FILE_FLAG_OVERLAPPED flag, the operation is performed
	// as an overlapped (asynchronous) operation. DeviceIoControl returns immediately,
	// Expects ERROR_IO_PENDING as normal behavior for this IOCTL
	DeviceIoControl(amchandle, IOCTL_SPBTESTTOOL_OPEN, &peripheralindex, sizeof(ULONG), nullptr, 0, &bytesReturned,
					&overlapped);
	if (GetLastError() != ERROR_IO_PENDING) {
		ERR("Error open_amc_peripheral - %d\n", GetLastError());
		return false;
	}
	DBG("open_amc_peripheral Success\n");
	return true;
}

/**
 * @brief Writes data to AMC device using Windows overlapped I/O
 *
 * Performs asynchronous write operation to the AMC device handle.
 * Handles both immediate completion and pending I/O scenarios with proper timeout.
 *
 * @param writebuffer Pointer to buffer containing data to write
 * @param writesize Number of bytes to write from the buffer
 * @return true if write operation completed successfully, false on failure
 *
 * @note Uses 5-second timeout for pending operations
 * @note Creates event handle for synchronization with overlapped I/O
 * @note Validates input parameters before attempting write
 */
bool I2CInterface::writeAmc(void *writebuffer, size_t writesize)
{
	TRACING();
	if (!writebuffer || writesize == 0) {
		ERR("Invalid write buffer\n");
		return false;
	}

	DWORD byteswritten = 0;
	OVERLAPPED overlapped = {};
	EventHandle eventHandle;

	if (!eventHandle.isValid()) {
		ERR("Failed to create event for overlapped I/O\n");
		return false;
	}

	overlapped.hEvent = eventHandle.get();

	BOOL success = WriteFile(amchandle, writebuffer, (DWORD)writesize, &byteswritten, &overlapped);

	if (!success) {
		if (GetLastError() == ERROR_IO_PENDING) {
			// Wait for the write to complete
			DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 5000); // 5 sec timeout
			if (waitResult == WAIT_OBJECT_0) {
				if (!GetOverlappedResult(amchandle, &overlapped, &byteswritten, FALSE)) {
					ERR("GetOverlappedResult failed: %lu\n", GetLastError());
					return false;
				}
			} else {
				ERR("WaitForSingleObject timeout or error: %lu\n", GetLastError());
				CancelIo(amchandle);
				return false;
			}
		} else {
			ERR("WriteFile failed: %lu\n", GetLastError());
			return false;
		}
	} else {
		// Operation completed immediately
		DBG("WriteFile success, %lu bytes written\n", byteswritten);
	}

	return true;
}

/**
 * @brief Reads data from AMC device using Windows overlapped I/O
 *
 * Performs asynchronous read operation from the AMC device handle.
 * Zeroes the read buffer before operation and handles both immediate completion
 * and pending I/O scenarios with proper timeout.
 *
 * @param readbuffer Pointer to buffer where read data will be stored
 * @param readsize Number of bytes to read (must be <= MAX_I2C_BUFFER_SIZE)
 * @return true if read operation completed successfully, false on failure
 *
 * @note Uses 5-second timeout for pending operations
 * @note Creates event handle for synchronization with overlapped I/O
 * @note Validates read size against MAX_I2C_BUFFER_SIZE before attempting read
 * @note Zeroes read buffer before operation to ensure clean data
 */
bool I2CInterface::readAmc(void *readbuffer, size_t readsize)
{
	TRACING();
	if (readsize == 0 || readsize > MAX_I2C_BUFFER_SIZE) {
		ERR("Invalid read size\n");
		return false;
	}

	ZERO_MEM(readbuffer, readsize);

	DWORD bytesread = 0;
	OVERLAPPED overlapped = {};
	EventHandle eventHandle;

	if (!eventHandle.isValid()) {
		ERR("Failed to create event for overlapped I/O\n");
		return false;
	}

	overlapped.hEvent = eventHandle.get();

	BOOL success = ReadFile(amchandle, readbuffer, (DWORD)readsize, &bytesread, &overlapped);

	if (!success) {
		if (GetLastError() == ERROR_IO_PENDING) {
			DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 5000); // 5 sec timeout
			if (waitResult == WAIT_OBJECT_0) {
				if (!GetOverlappedResult(amchandle, &overlapped, &bytesread, FALSE)) {
					ERR("GetOverlappedResult failed: %lu\n", GetLastError());
					return false;
				}
			} else {
				ERR("WaitForSingleObject timeout or error: %lu\n", GetLastError());
				CancelIo(amchandle);
				return false;
			}
		} else {
			ERR("ReadFile failed: %lu\n", GetLastError());
			return false;
		}
	} else {
		// Operation completed immediately
		DBG("ReadFile success: %lu bytes read\n", bytesread);
	}

	return true;
}

/**
 * @brief Closes the AMC device handle
 *
 * Closes the Windows file handle for the AMC device if it's currently open.
 *
 * @return true if handle was closed successfully or was already NULL, false on failure
 *
 * @note Safe to call multiple times as it checks for NULL handle
 */
bool I2CInterface::closeAmc()
{
	TRACING();
	if (amchandle != NULL) {
		return CloseHandle(amchandle);
	}
	return true;
}

/**
 * @brief Discovers AMC devices on Windows platform
 *
 * Scans the system for AMC I2C bus devices by querying DOS device names.
 * Searches for devices with "NF_I2C_BUS" prefix and constructs full device paths.
 * Normalizes hexadecimal notation from "0X" to "0x" format.
 *
 * @param amcDeviceList Pointer to vector that will be populated with discovered device paths
 * @return Number of discovered AMC devices, or -1 on error
 *
 * @note Uses QueryDosDevice Windows API to enumerate device names
 * @note Filters for "NF_I2C_BUS" device name pattern
 * @note Constructs full device paths with "\\\\.\" prefix
 * @note Normalizes hexadecimal notation for consistency
 */
int amcCardDiscovery(void *amcDeviceList)
{
	TRACING();
	std::vector<std::string> *deviceList = static_cast<std::vector<std::string> *>(amcDeviceList);

	TCHAR *devices = new (std::nothrow) TCHAR[MAX_BUFFER_SIZE];
	if (!devices) {
		ERR("Memory allocation for device list failed.\n");
		return -1;
	}

	DWORD result = QueryDosDevice(NULL, devices, MAX_BUFFER_SIZE);
	if (result == 0) {
		ERR("QueryDosDevice failed with error: %d\n", GetLastError());
		delete[] devices;
		return -1;
	}

	// Iterate through all device names
	for (TCHAR *p = devices; *p; p += lstrlen(p) + 1) {
		std::basic_string<TCHAR> deviceName = p;

		// Check if name starts with "NF_I2C_BUS"
		if (deviceName.find(L"NF_I2C_BUS") == 0) {
			// Replace all "0X" with "0x"
			size_t pos = 0;
			while ((pos = deviceName.find(L"0X", pos)) != std::wstring::npos) {
				deviceName.replace(pos, 2, L"0x");
				pos += 2;
			}

			// Construct full device path
			std::basic_string<TCHAR> fullPath = L"\\\\.\\" + deviceName;

			// Convert wide path to narrow ASCII/UTF-8 (device names expected ASCII)
			std::string narrowPath;
			narrowPath.reserve(fullPath.size());
			for (wchar_t wc : fullPath) {
				if (wc < 128)
					narrowPath.push_back(static_cast<char>(wc));
				else
					narrowPath.push_back('?');
			}
			DBG("%s\n", narrowPath.c_str());
			deviceList->push_back(narrowPath);
		}
	}
	delete[] devices;

	if (deviceList->empty()) {
		ERR("No matching AMC devices found.\n");
		return -1;
	} else {
		INFO("Total AMC devices found: %d\n", static_cast<int>(deviceList->size()));
	}

	return static_cast<int>(deviceList->size());
}