/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
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
 * @retval AMC_SUCCESS All devices initialized successfully
 * @retval AMC_ERROR One or more devices failed to initialize
 *
 * @note This is a private member function
 */
int amclib::amcInitialize()
{
	TRACING();

	pldmobj = new pldm *[numCards];
	if (pldmobj == nullptr) {
		ERR("Failed to allocate memory for pldm object array\n");
		return AMC_ERROR;
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
			return AMC_ERROR;
		}
		DBG("#####################################\n\n");
	}

	for (int i = 0; i < numCards; i++) {
		DBG("############ CARD : %02d ############\n", i);
		if (pldmobj[i]->initialize() != PLDM_SUCCESS) {
			ERR("Failed to initialize pldm Object for card: %d\n", i);
			return AMC_ERROR;
		}
		DBG("#####################################\n");
	}
	return AMC_SUCCESS;
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
 * @retval AMC_SUCCESS Firmware flash completed successfully
 * @retval AMC_ERROR Firmware flash failed
 *
 * @note The card must be previously enumerated and initialized
 * @note Progress can be monitored using amcFirmwareProgress()
 */
int amclib::amcFirmwareFlash(uint32_t cardNum, const char *pkgFilePath)
{
	TRACING();
	if (!pldmobj) {
		ERR("PLDM objects not initialized\n");
		return AMC_ERROR;
	}
	int ret = pldmobj[cardNum]->fwupd(pkgFilePath);
	return (ret == PLDM_SUCCESS) ? AMC_SUCCESS : AMC_ERROR;
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
 * @brief Get the index of an AMC card associated with the specified GPU BDF
 *
 * Searches the discovered AMC devices for a card associated with the given
 * GPU BDF (Bus:Device.Function) string and returns its index.
 *
 * @param gpuBDF GPU BDF string to identify the AMC card
 *
 * @return Card index if found, or -1 if not found
 * @retval >=0 Index of the found AMC card
 * @retval -1 No matching card found
 *
 * @note The function searches through the discovered AMC devices
 *       and retrieves information for the first matching GPU BDF.
 * @note The function returns -1 if no devices have been enumerated.
 */
int amclib::amcGetIndex(const std::string &gpuBDF)
{
	TRACING();
	for (size_t i = 0; i < amcDeviceList->size(); ++i) {
		if ((*amcDeviceList)[i].gpuParentPath == gpuBDF) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

/**
 * @brief Get AMC card information by GPU BDF
 *
 * Retrieves the serial number and AMC version for the AMC card associated
 * with the specified GPU BDF (Bus:Device.Function) string.
 *
 * @param gpuBDF GPU BDF string to identify the AMC card
 * @param serialNumStr Reference to string to receive the serial number
 * @param amcVersionStr Reference to string to receive the AMC version
 *
 * @return Card index if found, or -1 if not found or on error
 * @retval >=0 Index of the found AMC card
 * @retval -1 No matching card found or error occurred
 *
 * @note The function searches through the discovered AMC devices
 *       and retrieves information for the first matching GPU BDF.
 * @note The function returns -1 if no devices have been enumerated.
 */
int amclib::amcGetCardInfo(std::string gpuBDF, std::string &serialNumStr, std::string &versionStr)
{
	char serialNum[MAX_PATH] = {}, amcVersion[MAX_PATH] = {};

	for (size_t i = 0; i < amcDeviceList->size(); ++i) {
		if ((*amcDeviceList)[i].gpuParentPath == gpuBDF) {
			// Get amcGetSerialNumber and amcGetVersion
			uint8_t cardNum = static_cast<uint8_t>(i);
			size_t bufferSize = sizeof(serialNum);
			if (amcGetSerialNumber(cardNum, serialNum, &bufferSize) != AMC_SUCCESS) {
				ERR("Failed to get serial number for card %zu\n", i);
				return -1;
			}
			serialNumStr = std::string(serialNum);

			if (amcGetVersion(cardNum, amcVersion, &bufferSize) != AMC_SUCCESS) {
				ERR("Failed to get version for card %zu\n", i);
				return -1;
			}
			versionStr = std::string(amcVersion);

			return static_cast<int>(i);
		}
	}
	return -1;
}

/**
 * @brief Get sensor information by sensor ID for a specific AMC card
 *
 * Retrieves sensor information, including sensor ID and reading,
 * for the specified sensor ID on the given AMC card.
 *
 * @param[in] cardIndex Index of the AMC card to query
 * @param[in] sensorId Sensor ID to retrieve information for
 * @param[out] sensorInfo Reference to vector to receive sensor information
 *
 * @return Status of the sensor info retrieval operation
 * @retval AMC_SUCCESS Sensor information retrieved successfully
 * @retval AMC_ERROR Operation failed due to invalid card index or sensor ID
 *
 * @note The function populates the provided vector with matching sensor info
 *       If multiple sensors share the same ID, all will be included.
 */
int amclib::amcGetSensorInfoBySensorId(int cardIndex, uint16_t sensorId, std::vector<amcSensorInfo> &sensorInfo)
{
	TRACING();

	if (!pldmobj || !pldmobj[cardIndex]) {
		ERR("PLDM object not initialized for card index: %d\n", cardIndex);
		return AMC_ERROR;
	}

	uint8_t ret = pldmobj[cardIndex]->getSensorInfo(sensorId);
	if (ret != PLDM_SUCCESS) {
		ERR("Failed to get sensor info for sensor ID %d on card index %d\n", sensorId, cardIndex);
		return AMC_ERROR;
	}
	std::vector<pldmSensorInfo> &sensorInfoList = pldmobj[cardIndex]->getSensorInfoList();
	for (const auto &sensor : sensorInfoList) {
		if (sensor.sensorId == sensorId) {
			amcSensorInfo info;
			info.sensorId = sensor.sensorId;
			info.sensorReading = sensor.reading;
			sensorInfo.push_back(info);
		}
	}
	return AMC_SUCCESS;
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
 * @retval AMC_SUCCESS VRSync command executed successfully on all cards
 * @retval AMC_ERROR VRSync command failed on one or more cards
 */
int amclib::oemVrsync(uint8_t cmd)
{
	TRACING();
	for (int i = 0; i < numCards; i++) {
		if (pldmobj[i]->oemVrsyncCmd(cmd) != PLDM_SUCCESS) {
			ERR("Failed to perform VRSync for card : %d\n", i);
			return AMC_ERROR;
		}
	}
	return AMC_SUCCESS;
}

/**
 * @brief Get the serial number for a specified AMC card
 *
 * This function supports two usage patterns:
 * 1. Length query: Pass nullptr for serialNum to get required buffer length
 * 2. Data copy: Pass valid buffer and bufferSize to copy the serial number string
 *
 * @param cardNum Card number to query (0-based index, must be less than numCards)
 * @param serialNum Pointer to char buffer to receive serial number, or nullptr to query length
 * @param bufferSize Pointer to size_t: INPUT = buffer size, OUTPUT = required length (including null terminator)
 *
 * @return Status of the serial number retrieval operation
 * @retval AMC_SUCCESS Serial number retrieved successfully or length returned successfully
 * @retval AMC_ERROR Operation failed due to invalid card number, uninitialized PLDM object, or buffer too small
 *
 * @note The card must be properly initialized via amcEnumFirmwares() before calling this function
 * @note When querying length, pass nullptr for serialNum and check returned bufferSize
 * @note When copying data, ensure bufferSize >= returned length from previous query
 */
int amclib::amcGetSerialNumber(uint8_t cardNum, char *serialNum, size_t *bufferSize)
{
	TRACING();

	// Validate card number is within valid range
	if (cardNum >= numCards) {
		ERR("Invalid card number %d (valid range: 0-%d)\n", cardNum, numCards - 1);
		return AMC_ERROR;
	}

	// Ensure PLDM object exists for the specified card
	if (pldmobj[cardNum] == nullptr) {
		ERR("PLDM object not initialized for card %d\n", cardNum);
		return AMC_ERROR;
	}

	// Call the underlying PLDM function directly
	if (pldmobj[cardNum]->getFruSerialNum(serialNum, bufferSize) != PLDM_SUCCESS) {
		ERR("Failed to get serial number for card %d\n", cardNum);
		return AMC_ERROR;
	}

	// Log the serial number if fetched successfully
	if (serialNum != nullptr && bufferSize != nullptr) {
		DBG("Serial Number of card %d: %s\n", cardNum, serialNum);
	}

	return AMC_SUCCESS;
}

/**
 * @brief Get the AMC version for a specified AMC card
 *
 * This function supports two usage patterns:
 * 1. Length query: Pass nullptr for version to get required buffer length
 * 2. Data copy: Pass valid buffer and bufferSize to copy the version string
 *
 * @param cardNum Card number to query (0-based index, must be less than numCards)
 * @param version Pointer to char buffer to receive AMC version, or nullptr to query length
 * @param bufferSize Pointer to size_t: INPUT = buffer size, OUTPUT = required length (including null terminator)
 *
 * @return Status of the AMC version retrieval operation
 * @retval AMC_SUCCESS AMC version retrieved successfully or length returned successfully
 * @retval AMC_ERROR Operation failed due to invalid card number, uninitialized PLDM object, or buffer too small
 *
 * @note The card must be properly initialized via amcEnumFirmwares() before calling this function
 * @note When querying length, pass nullptr for amcVersion and check returned bufferSize
 * @note When copying data, ensure bufferSize >= returned length from previous query
 */
int amclib::amcGetVersion(uint8_t cardNum, char *amcVersion, size_t *bufferSize)
{
	TRACING();

	// Validate card number is within valid range
	if (cardNum >= numCards) {
		ERR("Invalid card number %d (valid range: 0-%d)\n", cardNum, numCards - 1);
		return AMC_ERROR;
	}

	// Ensure PLDM object exists for the specified card
	if (pldmobj[cardNum] == nullptr) {
		ERR("PLDM object not initialized for card %d\n", cardNum);
		return AMC_ERROR;
	}

	// Call the underlying PLDM function directly
	if (pldmobj[cardNum]->getAmcVersion(amcVersion, bufferSize) != PLDM_SUCCESS) {
		ERR("Failed to get version for card %d\n", cardNum);
		return AMC_ERROR;
	}

	// Log the amc version if fetched successfully
	if (amcVersion != nullptr && bufferSize != nullptr) {
		DBG("AMC Version of card %d: %s\n", cardNum, amcVersion);
	}

	return AMC_SUCCESS;
}

/**
 * @brief Perform AMC reset via PLDM Platform Monitoring & Control
 *
 * Resets the Add-in Card Management Controller (AMC) for the specified card.
 *
 * @param [in] cardNum Zero-based card number to reset (0 to numCards-1)
 *
 * @return Status of reset operation
 * @retval AMC_SUCCESS Reset command sent successfully
 * @retval AMC_ERROR Invalid card number or reset command failed
 *
 */
int amclib::amcGpuReset(uint32_t cardNum)
{
	TRACING();

	if (cardNum >= static_cast<uint32_t>(numCards)) {
		ERR("Invalid card number %d (valid range: 0-%d)\n", cardNum, numCards - 1);
		return AMC_ERROR;
	}

	if (pldmobj[cardNum] == nullptr) {
		ERR("PLDM object not initialized for card %d\n", cardNum);
		return AMC_ERROR;
	}

	DBG("Initiating AMC reset for card %d\n", cardNum);

	// Execute the AMC reset command
	if (pldmobj[cardNum]->amcGpuReset() != PLDM_SUCCESS) {
		ERR("Failed to reset AMC for card %d\n", cardNum);
		return AMC_ERROR;
	}

	return AMC_SUCCESS;
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
 * @retval AMC_SUCCESS Redfish connection initialized successfully
 * @retval AMC_ERROR Failed to initialize Redfish connection
 *
 * @note Ip, username and password are required parameters
 */
int amclib::redfishInitialize(const std::string &ip, const std::string &username, const std::string &password,
							  uint16_t port)
{
	TRACING();

	if (ip.empty()) {
		ERR("Ip address is required\n");
		return AMC_ERROR;
	}

	if (username.empty()) {
		ERR("Username is required\n");
		return AMC_ERROR;
	}

	if (password.empty()) {
		ERR("Password is required\n");
		return AMC_ERROR;
	}

	// Manual configuration with provided IP
	int ret = redfishObj.initialize(ip, username, password, port);
	return (ret == REDFISH_SUCCESS) ? AMC_SUCCESS : AMC_ERROR;
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
 * @retval AMC_SUCCESS Discovery completed successfully, devices found
 * @retval AMC_ERROR Discovery failed or no devices found
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
		return AMC_ERROR;
	}

	*gpuDevices = nullptr;
	*foundCount = 0;

	if (!redfishObj.isInit()) {
		ERR("Redfish not initialized\n");
		return AMC_ERROR;
	}

	// Get the first system ID automatically
	std::string systemId;
	if (redfishObj.getFirstSystemId(&systemId) != REDFISH_SUCCESS) {
		ERR("Redfish: Failed to get system ID\n");
		return AMC_ERROR;
	}

	// Use a reasonably large temporary buffer for discovery
	RedfishGPUDevice tempDevices[MAX_TEMP_GPUS];
	int tempCount = 0;

	int result = redfishObj.discoverIntelGpus(systemId, tempDevices, MAX_TEMP_GPUS, &tempCount);

	if (result != REDFISH_SUCCESS || tempCount <= 0) {
		INFO("Redfish: No Intel GPUs found or discovery failed\n");
		return AMC_ERROR;
	}

	// Dynamically allocate exact amount needed
	*gpuDevices = new (std::nothrow) RedfishGPUDevice[tempCount];
	if (!*gpuDevices) {
		ERR("Redfish: Failed to allocate memory for %d GPU devices\n", tempCount);
		return AMC_ERROR;
	}

	// Copy discovered devices to dynamically allocated array
	for (int i = 0; i < tempCount; i++) {
		(*gpuDevices)[i] = tempDevices[i];
	}
	*foundCount = tempCount;

	return AMC_SUCCESS;
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
