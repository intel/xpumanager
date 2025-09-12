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

#include "amclib.h"
#include "os.h"

/**
 * @brief Constructor for the amclib class
 *
 * Initializes the AMC (Advanced Management Controller) library instance.
 * Sets up the device list vector and initializes the card count to zero.
 *
 * @note This constructor does not perform device discovery. Call amcEnumFirmwares()
 *       to discover and initialize AMC devices.
 */
amclib::amclib()
{
	TRACING();

	// Initialize the number of cards to 0
	numCards = 0;

	amcDeviceList = new std::vector<amcCardInfo>();
	if (amcDeviceList == nullptr) {
		ERR("Failed to allocate memory for amcDeviceList vector\n");
	}

	pldmobj = NULL;
}

/**
 * @brief Destructor for the amclib class
 *
 * Performs cleanup of allocated resources including:
 * - pldm objects for each discovered card
 * - AMC device list vector
 * Ensures proper resource deallocation and prevents memory leaks.
 */
amclib::~amclib()
{
	TRACING();

	if (pldmobj != nullptr) {
		for (int i = 0; i < numCards; ++i) {
			if (pldmobj[i] != nullptr) {
				delete pldmobj[i];
				pldmobj[i] = nullptr;
			}
		}
		delete[] pldmobj;
		pldmobj = nullptr;
	}

	if (amcDeviceList != nullptr) {
		delete amcDeviceList;
		amcDeviceList = nullptr;
	}
}

/**
 * @brief Enumerates and initializes AMC firmware devices
 *
 * Discovers all available AMC devices in the system and creates pldm objects
 * for each device. This function must be called before any firmware operations.
 *
 * @return Number of AMC cards discovered and initialized, or -1 on error
 * @retval >0 Number of successfully initialized AMC devices
 * @retval -1 No devices found or initialization failed
 *
 * @note This function allocates memory for pldm objects. The destructor
 *       will handle cleanup if this function fails after partial allocation.
 */
int amclib::amcEnumFirmwares()
{
	TRACING();

	numCards = AMC_CARD_DISCOVERY(amcDeviceList);
	if (numCards <= 0) {
		ERR("No AMC devices found\n");
		return -1;
	}

	return numCards;
}

/**
 * @brief Initialize all discovered AMC devices
 *
 * Performs initialization of pldm objects for all discovered AMC cards.
 * This is an internal function called by amcEnumFirmwares().
 *
 * @return Status of initialization
 * @retval 0 All devices initialized successfully
 * @retval -1 One or more devices failed to initialize
 *
 * @note This is a private member function
 */
int amclib::amcInitialize()
{
	TRACING();

	pldmobj = new pldm *[numCards];
	if (pldmobj == nullptr) {
		ERR("Failed to allocate memory for pldm object array\n");
		return -1;
	}

	for (int i = 0; i < numCards; ++i) {
		DBG("############ CARD : %02d ############\n", i);
		pldmobj[i] = new pldm(amcDeviceList->at(i).amcDevicePath, i);
		if (pldmobj[i] == nullptr) {
			ERR("Failed to allocate memory for pldm object for card : %d\n", i);
			for (int j = 0; j < i; ++j) {
				delete pldmobj[j];
			}
			delete[] pldmobj;
			pldmobj = nullptr;
			return -1;
		}
		DBG("#####################################\n\n");
	}

	for (int i = 0; i < numCards; i++) {
		DBG("############ CARD : %02d ############\n", i);
		if (pldmobj[i]->initialize() == -1) {
			ERR("Failed to initialize pldm Object for card: %d\n", i);
			return -1;
		}
		DBG("#####################################\n");
	}
	return 0;
}

/**
 * @brief Flash firmware to a specific AMC card
 *
 * Initiates firmware update process for the specified AMC card using
 * the provided firmware package file.
 *
 * @param cardNum Zero-based index of the AMC card to update
 * @param pkgFilePath Path to the firmware package file
 *
 * @return Status of firmware flash operation
 * @retval 0 Firmware flash completed successfully
 * @retval <0 Firmware flash failed
 *
 * @note The card must be previously enumerated and initialized
 * @note Progress can be monitored using amcFirmwareProgress()
 */
int amclib::amcFirmwareFlash(uint32_t cardNum, const char *pkgFilePath)
{
	TRACING();
	if (!pldmobj) {
		ERR("PLDM objects not initialized\n");
		return -1;
	}
	return pldmobj[cardNum]->fwupd(pkgFilePath);
}

/**
 * @brief Get firmware update progress for all AMC cards
 *
 * Retrieves the firmware update progress percentage for all cards
 * that are currently undergoing firmware update operations.
 *
 * @return Status of progress query
 * @retval 0 Progress retrieved successfully for all cards
 * @retval -1 Failed to get progress for one or more cards
 *
 * @note This function checks progress for all cards, not just active updates
 */
int amclib::amcFirmwareProgress(uint32_t cardNum)
{
	TRACING();
	if (cardNum >= (uint32_t)numCards) {
		ERR("Invalid cardNum specified: %d. Max cards: %d\n", cardNum, numCards);
		return -1;
	}
	if (!pldmobj) {
		ERR("PLDM objects not initialized\n");
		return -1;
	}
	return pldmobj[cardNum]->fwupdProgress();
}

/**
 * @brief Find the index of an AMC card associated with the specified GPU BDF
 *
 * Searches the discovered AMC device list to find the index of the card
 * associated with the given GPU Bus-Device-Function (BDF) identifier.
 *
 * @param gpuBDF The BDF string of the GPU to check (e.g., "0000:00:02.0")
 * @return Index of the AMC card in amcDeviceList if found, -1 if not found
 */
int amclib::findBDF(const std::string &gpuBDF)
{
	for (size_t i = 0; i < amcDeviceList->size(); ++i) {
		if ((*amcDeviceList)[i].gpuParentPath == gpuBDF) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

/**
 * @brief Execute OEM VRSync command on all AMC cards
 *
 * Sends OEM-specific VRSync (Voltage Regulator Synchronization) commands
 * to all discovered AMC cards. Used for power management operations.
 *
 * @param cmd VRSync command to execute
 *            - OEM_VR_SYNC_PAUSE (0x01): Pause VR sync
 *            - OEM_VR_SYNC_RESUME (0x02): Resume VR sync
 *
 * @return Status of VRSync operation
 * @retval 0 VRSync command executed successfully on all cards
 * @retval -1 VRSync command failed on one or more cards
 */
int amclib::oemVrsync(uint8_t cmd)
{
	TRACING();
	for (int i = 0; i < numCards; i++) {
		if (pldmobj[i]->oemVrsyncCmd(cmd) != PLDM_SUCCESS) {
			ERR("Failed to perform VRSync for card : %d\n", i);
			return -1;
		}
	}
	return 0;
}

/**
 * @brief Initialize Redfish connection with manual configuration
 *
 * Initializes a Redfish connection using provided credentials and IP address.
 *
 * @param ip BMC IP address
 * @param username BMC username for authentication
 * @param password BMC password for authentication
 * @param port BMC port number (default: 443)
 *
 * @return Status of Redfish initialization
 * @retval 0 Redfish connection initialized successfully
 * @retval -1 Failed to initialize Redfish connection
 *
 * @note Ip, username and password are required parameters
 */
int amclib::redfishInitialize(const std::string &ip, const std::string &username, const std::string &password,
							  uint16_t port)
{
	TRACING();

	if (ip.empty()) {
		ERR("Ip address is required\n");
		return -1;
	}

	if (username.empty()) {
		ERR("Username is required\n");
		return -1;
	}

	if (password.empty()) {
		ERR("Password is required\n");
		return -1;
	}

	// Manual configuration with provided IP
	return redfishObj.initialize(ip, username, password, port);
}

/**
 * @brief Discover Intel GPU devices via Redfish protocol
 *
 * Performs discovery of Intel GPU devices accessible through the Redfish BMC interface.
 * Dynamically allocates memory for discovered devices and returns device information.
 * Uses parallel processing for improved discovery performance.
 *
 * @param gpu_devices Pointer to array pointer that will be allocated and populated with discovered devices
 * @param found_count Pointer to integer that will contain the number of discovered devices
 *
 * @return Status of GPU discovery operation
 * @retval 0 Discovery completed successfully, devices found
 * @retval -1 Discovery failed or no devices found
 *
 * @note Redfish must be initialized before calling this function
 * @note Caller must call freeGpuDevices() to release allocated memory
 * @note Memory is allocated using new[] and must be freed with delete[]
 */
int amclib::redfishDiscovery(RedfishGPUDevice **gpuDevices, int *foundCount)
{
	TRACING();

	if (!gpuDevices || !foundCount) {
		ERR("Invalid parameters for Redfish discovery\n");
		return -1;
	}

	*gpuDevices = nullptr;
	*foundCount = 0;

	if (!redfishObj.isInit()) {
		ERR("Redfish not initialized\n");
		return -1;
	}

	// Get the first system ID automatically
	std::string systemId;
	if (redfishObj.getFirstSystemId(&systemId) != REDFISH_SUCCESS) {
		ERR("Redfish: Failed to get system ID\n");
		return -1;
	}

	// Use a reasonably large temporary buffer for discovery
	RedfishGPUDevice tempDevices[MAX_TEMP_GPUS];
	int tempCount = 0;

	int result = redfishObj.discoverIntelGpus(systemId, tempDevices, MAX_TEMP_GPUS, &tempCount);

	if (result != REDFISH_SUCCESS || tempCount <= 0) {
		INFO("Redfish: No Intel GPUs found or discovery failed\n");
		return -1;
	}

	// Dynamically allocate exact amount needed
	*gpuDevices = new (std::nothrow) RedfishGPUDevice[tempCount];
	if (!*gpuDevices) {
		ERR("Redfish: Failed to allocate memory for %d GPU devices\n", tempCount);
		return -1;
	}

	// Copy discovered devices to dynamically allocated array
	for (int i = 0; i < tempCount; i++) {
		(*gpuDevices)[i] = tempDevices[i];
	}
	*foundCount = tempCount;

	return 0;
}

/**
 * @brief Free memory allocated for GPU device array
 *
 * Releases memory previously allocated by the redfishDiscovery() function for
 * the GPU device array. This function should be called after processing
 * the discovered GPU devices to prevent memory leaks.
 *
 * @param gpu_devices Pointer to GPU device array allocated by redfishDiscovery()
 *
 * @note This function can safely handle NULL pointers
 * @note Only call this function on arrays allocated by redfishDiscovery()
 */
void amclib::freeGpuDevices(RedfishGPUDevice *gpuDevices)
{
	TRACING();

	if (gpuDevices) {
		delete[] gpuDevices;
	}
}
