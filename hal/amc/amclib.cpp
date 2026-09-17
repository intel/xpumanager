/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "amclib.h"
#include "os.h"

#include <chrono>
#include <memory>
#include <thread>

namespace {

// Covers the longest AMC operation of a discovery, first-use MCTP/PLDM init plus the FRU read
// (about 2.4 s on CRI); a firmware update holds the bus far longer and is not waited out.
constexpr auto AMC_BUS_WAIT = std::chrono::seconds{5};
constexpr auto AMC_BUS_POLL = std::chrono::milliseconds{50};

} // namespace

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
	cardState = nullptr;
	cardMutex = nullptr;
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

	delete[] cardState;
	cardState = nullptr;
	delete[] cardMutex;
	cardMutex = nullptr;

	if (amcDeviceList != nullptr) {
		delete amcDeviceList;
		amcDeviceList = nullptr;
	}
}

/**
 * @brief Enumerates and initializes AMC firmware devices
 *
 * Discovers all available AMC devices in the system. No device is opened here;
 * call amcInitialize() next, and each card is opened on first use.
 *
 * @return Number of AMC cards discovered, or -1 on error
 * @retval >0 Number of discovered AMC devices
 * @retval -1 No devices found
 */
int amclib::amcEnumFirmwares()
{
	TRACING();

	numCards = AMC_CARD_DISCOVERY(amcDeviceList);
	if (numCards <= 0) {
		DBG("No AMC devices found\n");
		return -1;
	}

	return numCards;
}

/**
 * @brief Prepare per-card state for all discovered AMC devices
 *
 * Allocates a slot for each discovered card. No I2C device is opened here; each
 * card's device is opened and initialized by acquireCard() the first time that
 * card is used. This keeps one process from locking every AMC in the system when
 * it only needs one, so separate processes can work on separate cards concurrently.
 *
 * @return Status of initialization
 * @retval AMC_SUCCESS Per-card state allocated
 * @retval AMC_ERROR No cards enumerated
 *
 * @note amcEnumFirmwares() must be called first
 */
int amclib::amcInitialize()
{
	TRACING();

	if (numCards <= 0) {
		ERR("No AMC cards enumerated\n");
		return AMC_ERROR;
	}

	if (pldmobj != nullptr) {
		return AMC_SUCCESS;
	}

	pldmobj = new pldm *[numCards]();
	cardState = new std::atomic<int>[numCards];
	cardMutex = new std::mutex[numCards];
	for (int i = 0; i < numCards; ++i) {
		cardState[i].store(CARD_UNINIT);
	}
	return AMC_SUCCESS;
}

/**
 * @brief Take a card for one AMC operation, initializing it on first use
 *
 * Locks the card's in-process mutex, then its I2C bus, and runs the MCTP/PLDM init
 * sequence the first time the card is used. Both locks are held until the returned
 * lease is destroyed, so another process can use the card between operations.
 *
 * When another process holds the bus, waits up to AMC_BUS_WAIT for it. If that wait
 * times out, the card stays uninitialized so a later call can retry, but later calls
 * do not wait again until the bus has been acquired. Any other init failure is remembered, so a broken card is not
 * re-probed on every call.
 *
 * @param cardNum Zero-based index of the AMC card
 *
 * @return A lease that is true when the card is ready for the operation
 * @retval true  The card is initialized and its bus is locked by this process
 * @retval false The card index is invalid, the card failed to initialize, or the bus stayed busy
 */
amclib::CardLease amclib::acquireCard(uint32_t cardNum)
{
	if (pldmobj == nullptr) {
		ERR("PLDM objects not initialized\n");
		return {};
	}
	if (cardNum >= static_cast<uint32_t>(numCards)) {
		ERR("Invalid card number {} (valid range: 0-{})\n", cardNum, numCards - 1);
		return {};
	}

	std::unique_lock<std::mutex> lock(cardMutex[cardNum]);
	if (cardState[cardNum].load(std::memory_order_relaxed) == CARD_FAILED) {
		DBG("AMC card {} failed to initialize earlier\n", cardNum);
		return {};
	}

	if (pldmobj[cardNum] == nullptr) {
		DBG("############ CARD : {:02} ############\n", cardNum);
		auto card = std::make_unique<pldm>(amcDeviceList->at(cardNum).amcDevicePath, cardNum);
		if (!card->isI2cOpen()) {
			ERR("Failed to open the I2C device for AMC card {}\n", cardNum);
			cardState[cardNum].store(CARD_FAILED, std::memory_order_relaxed);
			return {};
		}
		pldmobj[cardNum] = card.release();
	}
	pldm *card = pldmobj[cardNum];

	if (!lockBus(cardNum, card)) {
		return {};
	}
	CardLease lease{lock.release(), card};

	if (cardState[cardNum].load(std::memory_order_relaxed) == CARD_UNINIT) {
		if (card->initialize() != PLDM_SUCCESS) {
			ERR("Failed to initialize pldm Object for card: {}\n", cardNum);
			cardState[cardNum].store(CARD_FAILED, std::memory_order_relaxed);
			return {};
		}
		cardState[cardNum].store(CARD_READY, std::memory_order_release);
		DBG("#####################################\n");
	}
	return lease;
}

/**
 * @brief Lock a card's I2C bus, waiting a bounded time while another process holds it
 *
 * Reports a busy card once per busy period. Until the bus is free again, later calls for
 * that card try once without waiting.
 *
 * @param cardNum Zero-based index of the AMC card, used for the message
 * @param card    The card's pldm object
 *
 * @retval true  The bus is locked by this process
 * @retval false The bus is still held by another process, or locking failed
 */
bool amclib::lockBus(uint32_t cardNum, pldm *card)
{
	const auto wait = card->mBusWaitExpired ? std::chrono::milliseconds{0} : AMC_BUS_WAIT;
	const auto deadline = std::chrono::steady_clock::now() + wait;
	while (!card->tryLockBus()) {
		if (!card->isI2cBusy()) {
			return false;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			if (!card->mBusWaitExpired) {
				// Expected while another process uses the card, so not an error: its data is just not
				// available to this command.
				DBG("AMC card {} (GPU {}) is in use by another process; its serial number and other AMC data are "
					"unavailable\n",
					cardNum, amcDeviceList->at(cardNum).gpuParentPath);
				card->mBusWaitExpired = true;
			}
			return false;
		}
		std::this_thread::sleep_for(AMC_BUS_POLL);
	}
	card->mBusWaitExpired = false;
	return true;
}

/**
 * @brief Flash firmware to a specific AMC card
 *
 * Initiates firmware update process for the specified AMC card using
 * the provided firmware package file.
 *
 * @param cardNum Zero-based index of the AMC card to update
 * @param pkgFilePath Path to the firmware package file
 * @param compIdFilter When non-zero, only the component with this ComponentIdentifier is flashed.
 *                     Zero flashes every component the package carries.
 * @param forceUpdate  Allow a forced (downgrade) update of components that support it. Passed with
 *                     the flash, not set beforehand, so the card's bus is locked only once.
 *
 * @return Status of firmware flash operation
 * @retval AMC_SUCCESS Firmware flash completed successfully
 * @retval AMC_ERROR Firmware flash failed
 *
 * @note The card must be previously enumerated and initialized
 * @note Progress can be monitored using amcFirmwareProgress()
 */
int amclib::amcFirmwareFlash(uint32_t cardNum, const char *pkgFilePath, uint16_t compIdFilter, bool forceUpdate)
{
	TRACING();
	const CardLease card = acquireCard(cardNum);
	if (!card) {
		return AMC_ERROR;
	}
	int ret = card->fwupd(pkgFilePath, compIdFilter, forceUpdate);
	return (ret == PLDM_SUCCESS) ? AMC_SUCCESS : AMC_ERROR;
}

/**
 * @brief Get the firmware update progress of an AMC card
 *
 * Called while amcFirmwareFlash() runs on another thread. That thread locks the bus and
 * initializes the card first, which can take several seconds while another process holds the
 * bus; until then there is no progress to report.
 *
 * @param cardNum Zero-based index of the AMC card
 *
 * @return Progress of the update
 * @retval 0-100 Percentage complete; 0 while the card is not yet initialized
 * @retval -1    Invalid card index, or PLDM objects not initialized
 */
int amclib::amcFirmwareProgress(uint32_t cardNum)
{
	TRACING();
	if (pldmobj == nullptr || cardNum >= static_cast<uint32_t>(numCards)) {
		return -1;
	}
	// The flash thread holds the card's lease for the whole update, so progress is read without
	// one. A ready card's pldm object is never replaced, so it can be read once the state says so.
	if (cardState[cardNum].load(std::memory_order_acquire) != CARD_READY) {
		return 0;
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
				// The callee (or acquireCard) has already reported why
				DBG("Failed to get serial number for card {}\n", i);
				return -1;
			}
			serialNumStr = std::string(serialNum);

			if (amcGetVersion(cardNum, amcVersion, &bufferSize) != AMC_SUCCESS) {
				DBG("Failed to get version for card {}\n", i);
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

	if (cardIndex < 0) {
		ERR("Invalid card index: {}\n", cardIndex);
		return AMC_ERROR;
	}
	const CardLease card = acquireCard(static_cast<uint32_t>(cardIndex));
	if (!card) {
		return AMC_ERROR;
	}

	uint8_t ret = card->getSensorInfoById(sensorId);
	if (ret != PLDM_SUCCESS) {
		ERR("Failed to get sensor info for sensor ID {} on card index {}\n", sensorId, cardIndex);
		return AMC_ERROR;
	}
	std::vector<pldmSensorInfo> &sensorInfoList = card->getSensorInfoList();
	for (const auto &sensor : sensorInfoList) {
		if (sensor.sensorId == sensorId) {
			amcSensorInfo info;
			info.sensorId = sensor.sensorId;
			info.sensorReading = sensor.reading;
			info.sensorUnit = sensor.unit;
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
		const CardLease card = acquireCard(static_cast<uint32_t>(i));
		if (!card || card->oemVrsyncCmd(cmd) != PLDM_SUCCESS) {
			ERR("Failed to perform VRSync for card : {}\n", i);
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

	if (bufferSize == nullptr) {
		ERR("Buffer size parameter cannot be null\n");
		return AMC_ERROR;
	}

	const CardLease card = acquireCard(cardNum);
	if (!card) {
		return AMC_ERROR;
	}

	// Call the underlying PLDM function directly
	if (card->getFruSerialNum(serialNum, *bufferSize) != PLDM_SUCCESS) {
		ERR("Failed to get serial number for card {}\n", cardNum);
		return AMC_ERROR;
	}

	// Log the serial number if fetched successfully
	if (serialNum != nullptr) {
		DBG("Serial Number of card {}: {}\n", cardNum, serialNum);
	}

	return AMC_SUCCESS;
}

/**
 * @brief Retrieves the FRU part number for a specified AMC card.
 *
 * Supports two usage patterns:
 * 1. Length query: pass nullptr for partNum to receive the required buffer length.
 * 2. Data copy: pass a valid buffer and bufferSize to copy the part number string.
 *
 * @param[in]     cardNum     Card index (0-based, must be less than numCards).
 * @param[out]    partNum     Buffer to receive the null-terminated part number, or nullptr to query length.
 * @param[in,out] bufferSize  INPUT = buffer capacity; OUTPUT = required length including null terminator.
 *
 * @retval AMC_SUCCESS  Part number retrieved or required length returned.
 * @retval AMC_ERROR    Invalid card index, uninitialized PLDM object, or buffer too small.
 */
int amclib::amcGetPartNumber(uint8_t cardNum, char *partNum, size_t *bufferSize)
{
	TRACING();

	const CardLease card = acquireCard(cardNum);
	if (!card) {
		return AMC_ERROR;
	}

	if (bufferSize == nullptr) {
		ERR("Buffer size parameter cannot be null\n");
		return AMC_ERROR;
	}

	if (card->getFruPartNum(partNum, *bufferSize) != PLDM_SUCCESS) {
		ERR("Failed to get part number for card {}\n", cardNum);
		return AMC_ERROR;
	}

	if (partNum != nullptr) {
		DBG("Part Number of card {}: {}\n", cardNum, partNum);
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

	const CardLease card = acquireCard(cardNum);
	if (!card) {
		return AMC_ERROR;
	}

	// Call the underlying PLDM function directly
	if (card->getAmcVersion(amcVersion, bufferSize) != PLDM_SUCCESS) {
		ERR("Failed to get version for card {}\n", cardNum);
		return AMC_ERROR;
	}

	// Log the amc version if fetched successfully
	if (amcVersion != nullptr && bufferSize != nullptr) {
		DBG("AMC Version of card {}: {}\n", cardNum, amcVersion);
	}

	return AMC_SUCCESS;
}

/**
 * @brief Get the Thermal Design Power (TDP) value for a specified AMC card
 *
 * @param [in] cardNum Card number to query (0-based index, must be less than numCards)
 * @param [out] tdp Pointer to char buffer to receive TDP string, or nullptr to query length
 * @param [in,out] bufferSize Pointer to size_t: INPUT = buffer size, OUTPUT = required length (including null
 * terminator)
 *
 * @return Status of the TDP retrieval operation
 * @retval AMC_SUCCESS TDP retrieved successfully or length returned successfully
 * @retval AMC_ERROR Operation failed due to invalid card number, uninitialized PLDM object, or TDP not available
 */
int amclib::amcGetTdp(uint8_t cardNum, char *tdp, size_t *bufferSize)
{
	TRACING();

	const CardLease card = acquireCard(cardNum);
	if (!card) {
		return AMC_ERROR;
	}

	if (bufferSize == nullptr) {
		ERR("Buffer size parameter cannot be null\n");
		return AMC_ERROR;
	}

	if (card->getFruTdp(tdp, *bufferSize) != PLDM_SUCCESS) {
		DBG("TDP not available for card {}\n", cardNum);
		return AMC_ERROR;
	}

	if (tdp != nullptr) {
		DBG("TDP of card {}: {}\n", cardNum, tdp);
	}

	return AMC_SUCCESS;
}

// Time the card needs to complete an AMC-driven reset before the bus is worth rescanning.
static constexpr uint32_t GPU_RESET_SETTLE_MS = 6000;

/**
 * @brief Perform AMC reset via PLDM Platform Monitoring & Control
 *
 * Resets the Add-in Card Management Controller (AMC) for the specified card and brings
 * the GPU back onto the PCI bus around it. The AMC powers the GPU down, so its link goes
 * away and it stops answering config reads for the duration of the reset. The sequence is:
 *
 * 1. Mask Surprise Link Down on the root port above the GPU, so the deliberate
 *    power-down is not reported as a fatal AER error.
 * 2. Issue the PLDM reset command.
 * 3. Remove the GPU from the bus while it is down.
 * 4. Wait for the reset to complete.
 * 5. Rescan the bus and confirm the GPU came back with a trained link.
 * 6. Clear the Surprise Link Down status and unmask its reporting again.
 *
 * @param [in] cardNum Zero-based card number to reset (0 to numCards-1)
 *
 * @return Status of reset operation
 * @retval AMC_SUCCESS GPU was reset and is back on the bus
 * @retval AMC_ERROR Invalid card number, or any step of the sequence failed
 *
 */
int amclib::amcGpuReset(uint32_t cardNum)
{
	TRACING();

	const CardLease card = acquireCard(cardNum);
	if (!card) {
		return AMC_ERROR;
	}

	const std::string gpuBdf = amcDeviceList->at(cardNum).gpuParentPath;
	if (gpuBdf.empty()) {
		ERR("No GPU BDF known for card {}; cannot sequence the reset\n", cardNum);
		return AMC_ERROR;
	}

	// The root port above the GPU's switch upstream port, which is where Surprise Down has
	// to be masked for the AMC to cut the GPU's power without raising an AER exception.
	// Resolved up front because the lookup walks the GPU's own sysfs node, which is gone
	// once the device has been removed.
	const std::string portBdf = getParentOfUpstreamPortBdf(gpuBdf);
	if (portBdf.empty()) {
		ERR("Could not find the root port above GPU {}\n", gpuBdf);
		return AMC_ERROR;
	}

	DBG("Initiating AMC reset for card {} (GPU {}, root port {})\n", cardNum, gpuBdf, portBdf);

	bool sldWasAlreadyMasked = false;
	if (maskSLDInPCIeConfig(portBdf, &sldWasAlreadyMasked) != 0) {
		ERR("Failed to mask Surprise Link Down on {} before resetting GPU {}\n", portBdf, gpuBdf);
		return AMC_ERROR;
	}

	int ret = AMC_SUCCESS;

	// Execute the AMC reset command
	if (card->amcGpuReset() != PLDM_SUCCESS) {
		ERR("Failed to reset AMC for card {}\n", cardNum);
		ret = AMC_ERROR;
	}

	// Hand back the AMC I2C handle before the GPU is detached. The I2C adapter is provided by
	// the GPU, so it disappears along with it, and the reset command above is the last thing
	// this card's PLDM object can do until the card is enumerated again.
	if (ret == AMC_SUCCESS && !pldmobj[cardNum]->closeAmcInterface()) {
		ERR("Failed to close the AMC I2C handle for card {} before removing GPU {}\n", cardNum, gpuBdf);
		ret = AMC_ERROR;
	}

	if (ret == AMC_SUCCESS && removePCIeDevice(gpuBdf) != 0) {
		ERR("Failed to remove GPU {} from the PCI bus\n", gpuBdf);
		ret = AMC_ERROR;
	}

	if (ret == AMC_SUCCESS) {
		MSLEEP(GPU_RESET_SETTLE_MS);

		if (rescanPCIeDevice(gpuBdf, "") != 0) {
			ERR("GPU {} did not come back after the AMC reset of card {}\n", gpuBdf, cardNum);
			ret = AMC_ERROR;
		}
	}

	// Restore the port's error reporting even when a step above failed, so a failed reset
	// does not leave Surprise Link Down masked for good. Ports that already had it masked
	// keep it that way, since that setting is not ours to drop.
	if (unmaskSLDInPCIeConfig(portBdf, sldWasAlreadyMasked) != 0) {
		ERR("Failed to unmask Surprise Link Down on {} after resetting GPU {}\n", portBdf, gpuBdf);
		ret = AMC_ERROR;
	}

	return ret;
}

/**
 * @brief Read a file from the AMC card using PLDM file transfer
 *
 * This function retrieves the contents of a file identified by its PDR ID from the specified AMC card.
 * It uses the PLDM file transfer protocol to read the file in chunks and assembles the complete file data.
 *
 * @param[in] deviceIndex Index of the AMC card to read from (0-based)
 * @param[in] filePdrId PDR ID of the file to read
 * @param[out] fileData Reference to vector that will receive the file data
 *
 * @return Status of the file read operation
 * @retval AMC_SUCCESS File read successfully and data populated in fileData
 * @retval AMC_ERROR Failed to read file due to invalid parameters, PLDM errors, or other issues
 *
 * @note The function handles multipart transfers for large files and ensures proper assembly of data
 * @note The caller is responsible for providing a valid device index and file PDR ID
 */
int amclib::amcReadFile(int deviceIndex, uint16_t filePdrId, std::vector<uint8_t> &fileData)
{
	TRACING();

	if (deviceIndex < 0) {
		ERR("Invalid device index: {}\n", deviceIndex);
		return AMC_ERROR;
	}
	const CardLease card = acquireCard(static_cast<uint32_t>(deviceIndex));
	if (!card) {
		return AMC_ERROR;
	}

	int ret = card->getFile(filePdrId, fileData);
	return (ret == PLDM_SUCCESS) ? AMC_SUCCESS : AMC_ERROR;
}
