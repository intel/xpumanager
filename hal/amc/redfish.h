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

#ifndef _REDFISH_H
#define _REDFISH_H

#include "common.h"
#include "http_client.h"
#include "threading.h"
#include <string>
#include <vector>
#include <cstdint>

// Redfish return codes
enum
{
	REDFISH_SUCCESS,
	REDFISH_FAILURE,
	REDFISH_TIMEOUT,
	REDFISH_AUTH_REQUIRED,
	REDFISH_NOT_FOUND,
};

// Redfish HTTP status codes
#define REDFISH_HTTP_OK 200
#define REDFISH_HTTP_ACCEPTED 202
#define REDFISH_HTTP_SUCCESS_MAX 299
#define REDFISH_HTTP_BAD_REQUEST 400
#define REDFISH_HTTP_UNAUTHORIZED 401
#define REDFISH_HTTP_NOT_FOUND 404
#define REDFISH_HTTP_INTERNAL_ERROR 500

// Redfish constants
#define REDFISH_DEFAULT_PORT 443
#define MAX_TEMP_GPUS 64
#define MAX_BUS_LENGTH 8
#define MAX_DEVICE_LENGTH 16
#define DEFAULT_MAX_THREADS 30

// Structure to hold GPU device information available from Redfish
struct RedfishGPUDevice
{
	int id;						  // Unique ID for the device in the list
	std::string deviceName;		  // e.g., "Intel(R) Graphics [0xe216]"
	std::string vendorName;		  // e.g., "Intel(R) Corporation"
	std::string deviceId;		  // e.g., "0xe216"
	std::string vendorId;		  // e.g., "0x8086"
	std::string classCode;		  // e.g., "0x038000"
	std::string deviceClass;	  // e.g., "DisplayController"
	std::string pciBdfAddress;	  // e.g., "0000:113:00.0" (derived from device path)
	std::string functionType;	  // e.g., "Physical"
	std::string devicePath;		  // Full Redfish device path
	std::string functionEndpoint; // Full Redfish function endpoint
};

// Structure to pass data to worker threads
struct ThreadDeviceData
{
	const void *redfishInstance;  // redfish instance pointer
	std::string devicePath;		  // PCIe device path to process
	RedfishGPUDevice *gpuDevices; // Array to store discovered GPUs
	int maxDevices;				  // Maximum number of devices in array
	int *foundCount;			  // Pointer to count of found devices
	MutexHandle *gpuArrayMutex;	  // Mutex for thread-safe array access
	int threadId;				  // Thread identifier for debugging
	void *processFunction;		  // Function pointer to device processing function
};

// Thread worker function declaration - platform-independent signature
void *deviceWorkerThread(void *data);

class LIBXPUM_API redfish
{
private:
	// HTTP interfaces
	HttpClient *httpClient;

	// Threading support
	mutable Threading *threading;

	// Initialization state and configuration
	bool init;
	std::string *redfishIp;
	std::string *redfishUsername;
	std::string *redfishPassword;
	uint16_t redfishPort;

	int verifyRedfishService() const;
	int getRedfishResource(const std::string &endpoint, std::string &responseData) const;
	int discoverIntelGpusHttpScan(const std::string &systemId, RedfishGPUDevice *gpuDevices, int maxDevices,
								  int *foundCount) const;

	// Helper functions for GPU discovery
	bool extractJsonField(const std::string &jsonData, const std::string &fieldName, std::string &outputValue) const;
	bool isIntelDevice(const std::string &deviceInfo) const;
	bool isGpuFunction(const std::string &functionDetails) const;
	void populateGpuDevice(int id, RedfishGPUDevice *gpu, const std::string &devicePath,
						   const std::string &functionEndpoint, const std::string &functionDetails) const;
	void generatePciInfo(RedfishGPUDevice *gpu, const std::string &devicePath) const;

public:
	redfish();
	~redfish();
	int initialize(const std::string &ipAddress, const std::string &username, const std::string &password,
				   uint16_t port = 443);
	int getFirstSystemId(std::string *systemId) const;
	int discoverIntelGpus(const std::string &systemId, RedfishGPUDevice *gpuDevices, int maxDevices,
						  int *foundCount) const;

	bool processPcieDeviceThreadsafe(const std::string &devicePath, RedfishGPUDevice *gpuDevices, int maxDevices,
									 int *foundCount, MutexHandle *mutex) const;

	// Simple initialization state check - only public accessor needed
	bool isInit() const { return init; }
};

#endif // _REDFISH_H