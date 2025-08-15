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

	// Initilize the number of cards to 0
	numCards = 0;

	amcDeviceList = new std::vector<std::basic_string<TCHAR>>();
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
amclib ::~amclib()
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

	if (amcDeviceList != NULL) {
		delete amcDeviceList;
		amcDeviceList = NULL;
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

	pldmobj = new pldm *[numCards];
	if (pldmobj == nullptr) {
		ERR("Failed to allocate memory for pldm object array\n");
		delete amcDeviceList;
		amcDeviceList = nullptr;
		return -1;
	}

	for (int i = 0; i < numCards; ++i) {
		DBG("############ CARD : %02d ############\n", i);
		pldmobj[i] = new pldm(amcDeviceList->at(i).c_str(), i);
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

	if (initialize() != 0) {
		ERR("Failed to initialize amclib\n");
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
int amclib::initialize()
{
	TRACING();
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
	return pldmobj[cardNum]->fwupdProgress();
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