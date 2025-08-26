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

#include "common.h"
#include "redfish.h"
#include "http_client.h"
#include <cstring>
#include <cstddef>
#include <cctype>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <algorithm>

/**
 * @brief Constructor for redfish class
 *
 * Initializes the Redfish discovery interface with default configuration.
 * Sets up HTTP client interfaces required for Redfish communication.
 *
 * @note Creates new instances of HttpClient
 * @note Initializes all credential buffers and configuration flags
 */
redfish::redfish()
{
	TRACING();
	httpClient = new HttpClient();
	if (httpClient == nullptr) {
		ERR("Redfish: Failed to allocate memory for HttpClient\n");
	}
	threading = new Threading();
	if (threading == nullptr) {
		ERR("Redfish: Failed to allocate memory for Threading object\n");
	}
	redfishIp = new std::string();
	redfishUsername = new std::string();
	redfishPassword = new std::string();
	init = false;
	redfishPort = REDFISH_DEFAULT_PORT;
}

/**
 * @brief Destructor for redfish class
 *
 * Performs cleanup of allocated resources including HTTP client objects.
 * Ensures proper resource deallocation.
 *
 * @note Sets pointers to nullptr after deletion to prevent double-free
 * @note Safe to call even if objects were not successfully allocated
 */
redfish::~redfish()
{
	TRACING();
	if (httpClient) {
		delete httpClient;
		httpClient = nullptr;
	}

	if (threading) {
		delete threading;
		threading = nullptr;
	}

	// Clean up string pointers
	delete redfishIp;
	delete redfishUsername;
	delete redfishPassword;
	redfishIp = nullptr;
	redfishUsername = nullptr;
	redfishPassword = nullptr;
}

/**
 * @brief Initialize Redfish connection with manual configuration
 *
 * Initializes a Redfish connection using manually provided configuration
 * including IP address, authentication credentials, and port number.
 *
 * @param ipAddress IP address of the Redfish service (required)
 * @param username Username for authentication (required)
 * @param password Password for authentication (required)
 * @param port Port number for the Redfish service (optional, default: 443)
 * @return Status of Redfish initialization
 * @retval REDFISH_SUCCESS Redfish service configured and verified successfully
 * @retval REDFISH_FAILURE Invalid parameters or service verification failed
 *
 * @note Stores credentials in internal buffers for later use
 * @note Verifies service accessibility after configuration
 */
int redfish::initialize(const std::string &ipAddress, const std::string &username, const std::string &password,
						uint16_t port)
{
	TRACING();

	if (ipAddress.empty()) {
		ERR("ERROR: Invalid Redfish initialization parameter: ipAddress is empty\n");
		return REDFISH_FAILURE;
	}

	if (username.empty()) {
		ERR("ERROR: Invalid Redfish initialization parameter: username is empty\n");
		return REDFISH_FAILURE;
	}

	if (password.empty()) {
		ERR("ERROR: Invalid Redfish initialization parameter: password is empty\n");
		return REDFISH_FAILURE;
	}

	*redfishIp = ipAddress;
	*redfishUsername = username;
	*redfishPassword = password;
	redfishPort = port;

	init = true;

	DBG("Redfish client initialized for %s:%d\n", redfishIp->c_str(), redfishPort);

	// Verify the Redfish service is accessible
	int verifyResult = verifyRedfishService();
	if (verifyResult != REDFISH_SUCCESS) {
		ERR("ERROR: Failed to verify Redfish service at %s:%d\n", redfishIp->c_str(), redfishPort);
		return verifyResult;
	}

	return REDFISH_SUCCESS;
}

/**
 * @brief Verify accessibility of the configured Redfish service
 *
 * Attempts to connect to the configured Redfish endpoint using the stored
 * IP address, port, and authentication credentials. Sends an HTTP GET request
 * to the Redfish service root and checks for a valid response. Determines if
 * the service is reachable and responds as expected, handling authentication
 * and common HTTP error codes.
 *
 * @return Status of Redfish service verification
 * @retval REDFISH_SUCCESS      Service is accessible and responds correctly
 * @retval REDFISH_AUTH_REQUIRED Authentication is required (HTTP 401)
 * @retval REDFISH_NOT_FOUND    Service not found (HTTP 404)
 * @retval REDFISH_TIMEOUT      Request timed out
 * @retval REDFISH_FAILURE      Other errors or invalid response
 *
 * @note Uses the internal HTTP client to perform the request
 * @note Checks for Redfish-specific content in the response body
 */
int redfish::verifyRedfishService() const
{
	TRACING();

	if (!httpClient) {
		ERR("Redfish: HTTP client not initialized\n");
		return REDFISH_FAILURE;
	}

	// Build the URL for the service root
	std::string url;
	const std::string &ip = *redfishIp;
	uint16_t port = redfishPort;

	if (port == 443) {
		url = "https://" + ip + "/redfish/v1/";
	} else {
		url = "https://" + ip + ":" + std::to_string(port) + "/redfish/v1/";
	}

	DBG("Redfish: Verifying Redfish service at: %s\n", url.c_str());
	HttpResponse response;
	int result;

	const std::string &username = *redfishUsername;
	const std::string &password = *redfishPassword;

	// Try with authentication if credentials are available
	if (!username.empty() && !password.empty()) {
		result = httpClient->getWithAuth(url, username, password, response);
	} else {
		result = httpClient->get(url, response);
	}

	// Check the result
	if (result == HTTP_SUCCESS) {
		DBG("Redfish: HTTP request successful, status code: %d\n", response.statusCode);

		// Check if we got a valid HTTP response
		if (response.statusCode >= REDFISH_HTTP_OK && response.statusCode <= REDFISH_HTTP_SUCCESS_MAX) {
			DBG("Redfish: Redfish service verification successful\n");
			return REDFISH_SUCCESS;
		} else if (response.statusCode == REDFISH_HTTP_UNAUTHORIZED) {
			DBG("Redfish: Authentication required (HTTP 401)\n");
			return REDFISH_AUTH_REQUIRED;
		} else if (response.statusCode == REDFISH_HTTP_NOT_FOUND) {
			DBG("Redfish: Redfish service not found (HTTP 404)\n");
			return REDFISH_NOT_FOUND;
		} else {
			DBG("Redfish: HTTP error: %d\n", response.statusCode);
			return REDFISH_FAILURE;
		}
	} else {
		// HTTP client error
		switch (result) {
		case HTTP_TIMEOUT:
			DBG("Redfish: Request timed out\n");
			return REDFISH_TIMEOUT;
		case HTTP_AUTH_REQUIRED:
			DBG("Redfish: Authentication required\n");
			return REDFISH_AUTH_REQUIRED;
		case HTTP_NOT_FOUND:
			DBG("Redfish: Service not found\n");
			return REDFISH_NOT_FOUND;
		default:
			DBG("Redfish: HTTP request failed with error: %d\n", result);
			return REDFISH_FAILURE;
		}
	}
}

/**
 * @brief Make a generic GET request to any Redfish endpoint
 *
 * Performs an authenticated HTTP GET request to the specified Redfish endpoint
 * and returns the response data. Handles SSL verification, authentication,
 * and error conditions automatically.
 *
 * @param endpoint Redfish endpoint path (e.g., "/redfish/v1/Systems")
 * @param responseData Reference to string to store the response data
 * @return Status of the request
 * @retval REDFISH_SUCCESS Request completed successfully
 * @retval REDFISH_FAILURE HTTP client error or invalid parameters
 * @retval REDFISH_AUTH_REQUIRED Authentication required (HTTP 401)
 * @retval REDFISH_NOT_FOUND Resource not found (HTTP 404)
 *
 * @note Automatically constructs full URL using configured IP and port
 * @note Uses 30-second timeout for data requests
 * @note Disables SSL verification for self-signed certificates
 */
int redfish::getRedfishResource(const std::string &endpoint, std::string &responseData) const
{
	TRACING();

	if (!httpClient || !init) {
		ERR("Redfish: HTTP client not initialized\n");
		return REDFISH_FAILURE;
	}

	if (endpoint.empty()) {
		ERR("Redfish: Invalid parameters\n");
		return REDFISH_FAILURE;
	}

	// Build the full URL
	std::string url;
	const std::string &ip = *redfishIp;
	uint16_t port = redfishPort;

	if (port == 443) {
		url = "https://" + ip + endpoint;
	} else {
		url = "https://" + ip + ":" + std::to_string(port) + endpoint;
	}

	HttpResponse response;
	int result;

	const std::string &username = *redfishUsername;
	const std::string &password = *redfishPassword;

	// Make authenticated request
	if (!username.empty() && !password.empty()) {
		result = httpClient->getWithAuth(url, username, password, response);
	} else {
		result = httpClient->get(url, response);
	}

	// Handle the response
	if (result == HTTP_SUCCESS && response.statusCode >= REDFISH_HTTP_OK &&
		response.statusCode <= REDFISH_HTTP_SUCCESS_MAX) {
		if (!response.responseData.empty()) {
			responseData = response.responseData;
			return REDFISH_SUCCESS;
		}
	}

	// Handle errors
	DBG("Redfish Endpoint: %s\n", endpoint.c_str());
	if (result != HTTP_SUCCESS) {
		ERR("Redfish: HTTP request failed with error: %d\n", result);
		return REDFISH_FAILURE;
	} else if (response.statusCode == REDFISH_HTTP_UNAUTHORIZED) {
		ERR("Redfish: Authentication required (HTTP 401)\n");
		return REDFISH_AUTH_REQUIRED;
	} else if (response.statusCode == REDFISH_HTTP_NOT_FOUND) {
		ERR("Redfish: Resource not found (HTTP 404)\n");
		return REDFISH_NOT_FOUND;
	} else {
		ERR("Redfish: HTTP error: %d\n", response.statusCode);
		return REDFISH_FAILURE;
	}
}

/**
 * @brief Extract the first system ID from the systems collection
 *
 * Parses the Systems collection JSON response to extract the ID of the first
 * available system. This ID is used for subsequent system-specific requests.
 *
 * @param systemId Pointer to string that will contain the discovered system ID
 * @return Status of the extraction
 * @retval REDFISH_SUCCESS System ID extracted successfully
 * @retval REDFISH_FAILURE Failed to parse systems collection or extract ID
 *
 * @note systemId parameter is required (cannot be nullptr)
 * @note Performs simple JSON parsing to find the first @odata.id in Members array
 * @note Extracts just the system ID from the full path (e.g., "system" from "/redfish/v1/Systems/system")
 */
int redfish::getFirstSystemId(std::string *systemId) const
{
	TRACING();

	if (!systemId) {
		ERR("Redfish: systemId parameter is required\n");
		return REDFISH_FAILURE;
	}

	// Get the systems collection
	std::string systemsResponse;
	std::string systemsEndpoint = "/redfish/v1/Systems";
	if (getRedfishResource(systemsEndpoint, systemsResponse) != REDFISH_SUCCESS) {
		ERR("Redfish: Failed to get systems collection\n");
		return REDFISH_FAILURE;
	}

	// Simple JSON parsing to extract the first system ID
	// Look for pattern: "Members": [ { "@odata.id": "/redfish/v1/Systems/SYSTEM_ID" } ]
	size_t membersPos = systemsResponse.find("\"Members\"");
	if (membersPos == std::string::npos) {
		ERR("Redfish: No Members found in systems collection\n");
		return REDFISH_FAILURE;
	}

	size_t odataIdPos = systemsResponse.find("\"@odata.id\"", membersPos);
	if (odataIdPos == std::string::npos) {
		ERR("Redfish: No @odata.id found in Members\n");
		return REDFISH_FAILURE;
	}

	size_t colonPos = systemsResponse.find(":", odataIdPos);
	if (colonPos == std::string::npos) {
		ERR("Redfish: Invalid @odata.id format\n");
		return REDFISH_FAILURE;
	}

	// Skip past ": and find the value
	size_t valueStart = colonPos + 1;
	while (valueStart < systemsResponse.length() &&
		   (systemsResponse[valueStart] == ' ' || systemsResponse[valueStart] == '"')) {
		valueStart++;
	}

	size_t valueEnd = systemsResponse.find("\"", valueStart);
	if (valueEnd == std::string::npos) {
		ERR("Redfish: Invalid @odata.id value format\n");
		return REDFISH_FAILURE;
	}

	// Extract the full path (e.g., "/redfish/v1/Systems/system")
	std::string fullPath = systemsResponse.substr(valueStart, valueEnd - valueStart);

	// Find the last '/' and extract the system ID
	size_t lastSlashPos = fullPath.rfind('/');
	if (lastSlashPos != std::string::npos && lastSlashPos + 1 < fullPath.size()) {
		*systemId = fullPath.substr(lastSlashPos + 1);
		DBG("Redfish: Found system ID: %s\n", systemId->c_str());
		return REDFISH_SUCCESS;
	} else {
		ERR("Redfish: Could not extract Redfish System ID from path: %s\n", fullPath.c_str());
		return REDFISH_FAILURE;
	}
}

/**
 * @brief Thread-safe processing of a PCIe device to identify Intel GPUs
 *
 * Retrieves and analyzes Redfish resource data for the specified PCIe device
 * path. Checks if the device is an Intel PCIe device and if function 0 is a GPU.
 * If a GPU is found, safely updates the gpu_devices array and found_count using
 * the provided mutex to ensure thread safety in concurrent environments.
 *
 * @param devicePath    Redfish path to the PCIe device (e.g., "/redfish/v1/Chassis/1/PCIeDevices/1")
 * @param gpuDevices    Array to store discovered GPU device information
 * @param maxDevices    Maximum number of devices that can be stored in gpuDevices
 * @param foundCount    Pointer to the count of GPUs found (incremented if a GPU is found)
 * @param mutex         Mutex handle for thread safety (can be nullptr if not needed)
 * @return true if the device is an Intel GPU and was processed successfully, false otherwise
 *
 * @note Only function 0 is checked for GPU identification
 * @note The gpuDevices array and foundCount are updated in a thread-safe manner
 * @note Intended for use in parallel or multithreaded Redfish discovery routines
 */
bool redfish::processPcieDeviceThreadsafe(const std::string &devicePath, RedfishGPUDevice *gpuDevices, int maxDevices,
										  int *foundCount, MutexHandle *mutex) const
{
	if (devicePath.empty()) {
		ERR("Redfish: PCIe device path is required\n");
		return false;
	}

	// Get detailed information for this PCIe device
	std::string deviceInfo;
	if (getRedfishResource(devicePath, deviceInfo) != REDFISH_SUCCESS) {
		ERR("Redfish: Failed to get device details for %s\n", devicePath.c_str());
		return false;
	}

	// Check if this is an Intel device
	if (!isIntelDevice(deviceInfo)) {
		return false;
	}

	// Directly check function 0 for this Intel PCIe device (GPUs always have function 0)
	std::string functionEndpoint = devicePath + "/PCIeFunctions/0";

	// Get detailed function information for function 0
	std::string functionDetails;
	if (getRedfishResource(functionEndpoint, functionDetails) != REDFISH_SUCCESS) {
		ERR("Redfish: Failed to get function 0 details for %s\n", devicePath.c_str());
		return false;
	}

	// Check if this is a GPU
	if (!isGpuFunction(functionDetails)) {
		return false;
	}

	// We found a GPU! Thread-safe update of the array
	threading->lockMutex(mutex);

	if (*foundCount >= maxDevices) {
		threading->unlockMutex(mutex);
		return false;
	}

	RedfishGPUDevice *gpu = &gpuDevices[*foundCount];
	populateGpuDevice(*foundCount, gpu, devicePath, functionEndpoint, functionDetails);

	(*foundCount)++;

	threading->unlockMutex(mutex);

	return true;
}

/**
 * @brief Discover Intel GPUs using Redfish PCIe device enumeration (HTTP scan)
 *
 * Scans the Redfish PCIeDevices collection for the specified system, launching
 * parallel worker threads to process each PCIe device and identify Intel GPUs.
 * Populates the provided gpu_devices array with discovered GPU information and
 * updates the found_count accordingly. Handles thread synchronization and error
 * conditions, and supports batch processing for large device sets.
 *
 * @param systemId     Redfish system ID to scan for PCIe devices
 * @param gpuDevices   Array to store discovered GPU device information
 * @param maxDevices   Maximum number of devices that can be stored in gpuDevices
 * @param foundCount   Pointer to the count of GPUs found (incremented for each GPU)
 * @return REDFISH_SUCCESS if at least one GPU is found, REDFISH_FAILURE otherwise
 *
 * @note Uses a thread pool to process devices in parallel for efficiency
 * @note Ensures thread-safe updates to the gpuDevices array and foundCount
 * @note Cleans up all resources and threads before returning
 */
int redfish::discoverIntelGpusHttpScan(const std::string &systemId, RedfishGPUDevice *gpuDevices, int maxDevices,
									   int *foundCount) const
{
	TRACING();

	if (systemId.empty() || !gpuDevices || !foundCount || maxDevices <= 0) {
		ERR("Redfish: Invalid parameters for parallel GPU discovery\n");
		return REDFISH_FAILURE;
	}

	*foundCount = 0;
	// Initialize GPU devices (std::string members are automatically initialized)
	for (int i = 0; i < maxDevices; i++) {
		gpuDevices[i].id = 0;
		gpuDevices[i].deviceName.clear();
		gpuDevices[i].vendorName.clear();
		gpuDevices[i].deviceId.clear();
		gpuDevices[i].vendorId.clear();
		gpuDevices[i].classCode.clear();
		gpuDevices[i].deviceClass.clear();
		gpuDevices[i].pciBdfAddress.clear();
		gpuDevices[i].functionType.clear();
		gpuDevices[i].devicePath.clear();
		gpuDevices[i].functionEndpoint.clear();
	}

	// Get the PCIeDevices collection which contains the Members array
	std::string devicesEndpoint = "/redfish/v1/Systems/" + systemId + "/PCIeDevices";

	std::string pcieDevicesCollection;
	if (getRedfishResource(devicesEndpoint, pcieDevicesCollection) != REDFISH_SUCCESS) {
		ERR("Redfish: Failed to get PCIeDevices collection\n");
		return REDFISH_FAILURE;
	}

	// Initialize mutex for thread-safe GPU array access
	MutexHandle *gpuArrayMutex;
	if (threading->createMutex(&gpuArrayMutex) != 0) {
		ERR("Redfish: Failed to create mutex for GPU array access\n");
		return REDFISH_FAILURE;
	}

	// Parse the Members array to collect all PCIe device paths
	size_t members_pos = pcieDevicesCollection.find("\"Members\"");
	if (members_pos == std::string::npos) {
		ERR("Redfish: No Members found in PCIeDevices collection\n");
		threading->destroyMutex(gpuArrayMutex);
		return REDFISH_FAILURE;
	}

	// Find the array start
	size_t arrayStartPos = pcieDevicesCollection.find("[", members_pos);
	if (arrayStartPos == std::string::npos) {
		ERR("Redfish: Invalid Members array format\n");
		threading->destroyMutex(gpuArrayMutex);
		return REDFISH_FAILURE;
	}

	// Use dynamic array to collect all device paths
	std::vector<std::string> devicePaths;
	size_t currentPos = arrayStartPos + 1;
	int parseIndex = 0;

	// Parse each PCIe device reference in the Members array
	while (currentPos < pcieDevicesCollection.length() && pcieDevicesCollection[currentPos] != ']') {
		// Skip whitespace and commas
		while (currentPos < pcieDevicesCollection.length() &&
			   (pcieDevicesCollection[currentPos] == ' ' || pcieDevicesCollection[currentPos] == '\n' ||
				pcieDevicesCollection[currentPos] == '\t' || pcieDevicesCollection[currentPos] == ',')) {
			currentPos++;
		}

		// Check if we've reached the end
		if (currentPos >= pcieDevicesCollection.length() || pcieDevicesCollection[currentPos] == ']') {
			break;
		}

		// Look for @odata.id
		size_t odataIdPos = pcieDevicesCollection.find("\"@odata.id\"", currentPos);
		if (odataIdPos == std::string::npos) {
			break;
		}

		// Find the device path
		// Look for the colon after @odata.id
		size_t colonPos = pcieDevicesCollection.find(":", odataIdPos);
		if (colonPos == std::string::npos) {
			// Move to next object
			currentPos = odataIdPos + 12;
			while (currentPos < pcieDevicesCollection.length() && pcieDevicesCollection[currentPos] != ',' &&
				   pcieDevicesCollection[currentPos] != '}' && pcieDevicesCollection[currentPos] != ']') {
				currentPos++;
			}
			parseIndex++;
			continue;
		}

		// Find the start of the value (skip spaces and quotes)
		size_t valueStart = colonPos + 1;
		while (valueStart < pcieDevicesCollection.length() &&
			   (pcieDevicesCollection[valueStart] == ' ' || pcieDevicesCollection[valueStart] == '"')) {
			valueStart++;
		}

		// Find the end of the value (find the closing quote)
		size_t valueEnd = pcieDevicesCollection.find("\"", valueStart);
		if (valueEnd == std::string::npos) {
			currentPos = odataIdPos + 12;
			continue;
		}

		// Store the device path in the vector
		if (valueEnd <= valueStart) {
			ERR("Redfish: Invalid @odata.id value format\n");
			continue;
		}

		std::string devicePath = pcieDevicesCollection.substr(valueStart, valueEnd - valueStart);
		devicePaths.push_back(devicePath);

		// Move to next device - find the closing brace of this object
		currentPos = valueEnd + 1;
		while (currentPos < pcieDevicesCollection.length() && pcieDevicesCollection[currentPos] != '}') {
			currentPos++;
		}
		if (currentPos < pcieDevicesCollection.length() && pcieDevicesCollection[currentPos] == '}') {
			currentPos++;
		}

		parseIndex++;
	}

	int deviceCount = static_cast<int>(devicePaths.size());

	if (deviceCount == 0) {
		DBG("No PCIe devices found to process\n");
		threading->destroyMutex(gpuArrayMutex);
		return REDFISH_FAILURE;
	}

	// Process devices in batches using thread pool
	int devicesProcessed = 0;
	while (devicesProcessed < deviceCount) {
		// Calculate how many threads to spawn for this batch
		int devicesRemaining = deviceCount - devicesProcessed;
		int threadsToSpawn = std::min(devicesRemaining, DEFAULT_MAX_THREADS);

		// Create thread data for this batch
		std::vector<ThreadDeviceData> threadData(threadsToSpawn);
		std::vector<ThreadHandle *> threadHandles(threadsToSpawn);

		DBG("Redfish: Processing batch: devices %d-%d of %d (using %d threads)\n", devicesProcessed,
			devicesProcessed + threadsToSpawn - 1, deviceCount, threadsToSpawn);

		// Spawn threads for current batch
		for (int i = 0; i < threadsToSpawn; i++) {
			int deviceIndex = devicesProcessed + i;

			threadData[i].redfishInstance = this;
			threadData[i].devicePath = devicePaths[deviceIndex];
			threadData[i].gpuDevices = gpuDevices;
			threadData[i].maxDevices = maxDevices;
			threadData[i].foundCount = foundCount;
			threadData[i].gpuArrayMutex = gpuArrayMutex;
			threadData[i].threadId = deviceIndex;

			// Use device worker thread creation method
			if (threading->createThread(&threadHandles[i], deviceWorkerThread, &threadData[i]) != 0) {
				ERR("Redfish: Failed to create thread for device %d\n", deviceIndex);
				// Clean up any threads that were created successfully
				for (int j = 0; j < i; j++) {
					threading->joinThread(threadHandles[j], nullptr);
					threading->cleanupThread(threadHandles[j]);
				}
				threading->destroyMutex(gpuArrayMutex);
				return REDFISH_FAILURE;
			}
		}

		// Wait for all threads in the batch to complete
		for (int i = 0; i < threadsToSpawn; i++) {
			if (threadHandles[i] != nullptr) {
				threading->joinThread(threadHandles[i], nullptr);
				threading->cleanupThread(threadHandles[i]);
			}
		}

		devicesProcessed += threadsToSpawn;
	}

	threading->destroyMutex(gpuArrayMutex);

	if (*foundCount > 0) {
		return REDFISH_SUCCESS;
	} else {
		return REDFISH_FAILURE;
	}
}

/**
 * @brief Discover Intel GPUs using Redfish service
 *
 * Main entry point for Intel GPU discovery via Redfish. Currently performs
 * HTTP-based remote discovery by scanning the Redfish PCIeDevices collection
 * for the specified system and identifying Intel GPUs. Populates the provided
 * gpu_devices array with discovered GPU information and updates found_count.
 *
 * @param system_id     Redfish system identifier (e.g., "system")
 * @param gpu_devices   Array to populate with discovered GPU device information
 * @param max_devices   Maximum number of devices the array can hold
 * @param found_count   Pointer to variable that will receive the number of GPUs found
 * @return REDFISH_SUCCESS if one or more Intel GPUs are discovered, REDFISH_FAILURE otherwise
 *
 * @note Currently only supports HTTP-based discovery. Local discovery optimizations
 *       may be added in the future.
 */
int redfish::discoverIntelGpus(const std::string &systemId, RedfishGPUDevice *gpuDevices, int maxDevices,
							   int *foundCount) const
{
	TRACING();

	if (systemId.empty() || !gpuDevices || !foundCount || maxDevices <= 0) {
		ERR("Redfish: Invalid parameters for GPU discovery\n");
		return REDFISH_FAILURE;
	}

	// HTTP-based discovery for remote systems
	return discoverIntelGpusHttpScan(systemId, gpuDevices, maxDevices, foundCount);
}

/**
 * @brief Worker thread function for device processing (application layer)
 *
 * This function is passed to the OAL's create_thread API and performs
 * device processing using the provided ThreadDeviceData.
 *
 * @param data Pointer to ThreadDeviceData containing thread parameters
 * @return Always returns nullptr (pthread convention)
 */
void *deviceWorkerThread(void *data)
{
	ThreadDeviceData *threadData = static_cast<ThreadDeviceData *>(data);
	// Call the device processing function
	static_cast<const redfish *>(threadData->redfishInstance)
		->processPcieDeviceThreadsafe(threadData->devicePath, threadData->gpuDevices, threadData->maxDevices,
									  threadData->foundCount, threadData->gpuArrayMutex);
	return nullptr;
}

/**
 * @brief Extract a field value from JSON data
 *
 * Performs simple JSON parsing to extract the value of a specified field.
 * This is a lightweight alternative to a full JSON parser for extracting
 * specific Redfish property values.
 *
 * @param jsonData JSON string to parse
 * @param fieldName Name of the field to extract (including quotes)
 * @param outputBuffer Buffer to store the extracted field value
 * @param bufferSize Size of the output buffer
 * @return true if field was found and extracted, false otherwise
 *
 * @note Expects fieldName to include quotes (e.g., "\"DeviceId\"")
 * @note Performs string-based parsing, not full JSON validation
 * @note Extracted value excludes surrounding quotes
 */
bool redfish::extractJsonField(const std::string &jsonData, const std::string &fieldName,
							   std::string &outputValue) const
{
	size_t fieldPos = jsonData.find(fieldName);
	if (fieldPos == std::string::npos) {
		return false;
	}

	size_t colonPos = jsonData.find(":", fieldPos);
	if (colonPos == std::string::npos) {
		return false;
	}

	size_t valueStart = colonPos + 1;
	while (valueStart < jsonData.length() && (jsonData[valueStart] == ' ' || jsonData[valueStart] == '"')) {
		valueStart++;
	}

	if (valueStart >= jsonData.length()) {
		return false;
	}

	size_t valueEnd = jsonData.find("\"", valueStart);
	if (valueEnd == std::string::npos) {
		return false;
	}

	outputValue = jsonData.substr(valueStart, valueEnd - valueStart);
	return true;
}

/**
 * @brief Check if a PCIe device is an Intel device
 *
 * Scans the provided device information string for Intel-specific identifiers,
 * such as the PCI vendor ID ("0x8086" or "8086") or the string "Intel".
 * Used to determine if a discovered PCIe device belongs to Intel.
 *
 * @param deviceInfo JSON or text string containing PCIe device details
 * @return true if the device is identified as an Intel device, false otherwise
 *
 * @note Performs a simple substring search for known Intel identifiers
 */
bool redfish::isIntelDevice(const std::string &deviceInfo) const
{
	return (deviceInfo.find("\"0x8086\"") != std::string::npos || deviceInfo.find("\"8086\"") != std::string::npos ||
			deviceInfo.find("Intel") != std::string::npos);
}

/**
 * @brief Check if a PCIe function represents a GPU device
 *
 * Parses the provided function details to determine if the PCIe function
 * corresponds to a GPU. This is typically identified by checking if the
 * ClassCode field is set to 0x03, which indicates a display controller.
 *
 * @param functionDetails JSON or text string containing PCIe function details
 * @return true if the function is identified as a GPU, false otherwise
 *
 * @note Uses simple JSON field extraction to check the ClassCode value
 */
bool redfish::isGpuFunction(const std::string &functionDetails) const
{
	std::string classCode;
	if (!extractJsonField(functionDetails, "\"ClassCode\"", classCode)) {
		return false;
	}

	return (classCode.compare(0, 4, "0x03") == 0);
}

/**
 * @brief Populate a RedfishGPUDevice structure with discovered GPU details
 *
 * Fills in the fields of a RedfishGPUDevice structure using information
 * extracted from the Redfish PCIe device and function endpoints. Copies
 * device and function paths, extracts relevant JSON fields (such as DeviceId,
 * VendorId, ClassCode, DeviceClass, FunctionType, and PCIeType), generates
 * the PCI BDF address, and sets vendor and device names.
 *
 * @param id                Index or identifier for the GPU device
 * @param gpu               Pointer to the RedfishGPUDevice structure to populate
 * @param devicePath        Redfish path to the PCIe device
 * @param functionEndpoint  Redfish endpoint for the PCIe function (e.g., function 0)
 * @param functionDetails   JSON or text string with PCIe function details
 *
 * @note Assumes all input buffers are valid and sufficiently sized
 * @note Uses safe string copy and JSON field extraction helpers
 * @note Sets vendor name to "Intel(R) Corporation" and formats device name
 */
void redfish::populateGpuDevice(int id, RedfishGPUDevice *gpu, const std::string &devicePath,
								const std::string &functionEndpoint, const std::string &functionDetails) const
{
	gpu->id = id;

	// Store paths
	gpu->devicePath = devicePath;
	gpu->functionEndpoint = functionEndpoint;

	// Extract all the device information fields
	extractJsonField(functionDetails, "\"DeviceId\"", gpu->deviceId);
	extractJsonField(functionDetails, "\"VendorId\"", gpu->vendorId);
	extractJsonField(functionDetails, "\"ClassCode\"", gpu->classCode);
	extractJsonField(functionDetails, "\"DeviceClass\"", gpu->deviceClass);
	extractJsonField(functionDetails, "\"FunctionType\"", gpu->functionType);

	// Generate PCI BDF address
	generatePciInfo(gpu, devicePath);

	// Set vendor name and device name
	gpu->vendorName = "Intel(R) Corporation";
	gpu->deviceName = "Intel(R) Graphics [" + gpu->deviceId + "]";
}

/**
 * @brief Generate PCI BDF address for a GPU device from its Redfish device path
 *
 * Parses the Redfish PCIe device path to extract the bus and device numbers,
 * then formats and stores the PCI BDF (Bus:Device.Function) address in the
 * provided RedfishGPUDevice structure. Assumes function 0 for all discovered
 * GPU devices.
 *
 * @param gpu         Pointer to the RedfishGPUDevice structure to update
 * @param devicePath  Redfish path to the PCIe device (e.g., "/redfish/v1/Chassis/1/PCIeDevices/S0B113D0")
 *
 * @note Expects devicePath to follow the S0B<bus>D<device> format
 * @note Sets gpu->pci_bdf_address to the formatted PCI address (e.g., "0000:71:19.0")
 * @note Does nothing if parsing fails or the path format is invalid
 */
void redfish::generatePciInfo(RedfishGPUDevice *gpu, const std::string &devicePath) const
{
	size_t lastSlash = devicePath.find_last_of('/');
	if (lastSlash == std::string::npos) {
		return;
	}

	std::string pathId = devicePath.substr(lastSlash + 1);

	// Parse S0B113D0 format: S(socket)B(bus)D(device).F(function implied 0)
	if (pathId.empty() || pathId[0] != 'S') {
		return;
	}

	// Extract bus number (after 'B', before 'D')
	size_t busPos = pathId.find('B');
	size_t devicePos = pathId.find('D');
	if (busPos == std::string::npos || devicePos == std::string::npos || busPos >= devicePos) {
		return;
	}

	size_t busStart = busPos + 1; // Skip 'B'
	size_t busLength = devicePos - busStart;

	if (busLength == 0 || busLength > MAX_BUS_LENGTH) {
		return;
	}

	// Extract bus number
	std::string busStr = pathId.substr(busStart, busLength);
	int busNum = 0;
	try {
		busNum = std::stoi(busStr, nullptr, 16);
	} catch (const std::exception &) {
		DBG("Redfish: Failed to parse bus number\n");
		return;
	}

	size_t deviceStart = devicePos + 1; // Skip 'D'

	// Find the end of the device number (end of string or next non-hex char)
	size_t deviceEnd = deviceStart;
	while (deviceEnd < pathId.length() && std::isxdigit(pathId[deviceEnd])) {
		deviceEnd++;
	}

	size_t deviceLength = deviceEnd - deviceStart;
	if (deviceLength == 0 || deviceLength > MAX_DEVICE_LENGTH) {
		DBG("Redfish: Invalid device number length\n");
		return;
	}

	std::string deviceStr = pathId.substr(deviceStart, deviceLength);
	int deviceNum = 0;
	try {
		deviceNum = std::stoi(deviceStr, nullptr, 16);
	} catch (const std::exception &) {
		DBG("Redfish: Failed to parse device number\n");
		return;
	}

	// Format the PCI BDF address using std::ostringstream for proper hex formatting
	std::ostringstream bdfStream;
	bdfStream << "0000:" << std::hex << std::setfill('0') << std::setw(2) << busNum << ":" << std::setw(2) << deviceNum
			  << ".0";
	gpu->pciBdfAddress = bdfStream.str();
}
