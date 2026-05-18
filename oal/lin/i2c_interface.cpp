/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "os.h"
#include <algorithm>
#include <cctype>
#include <debug.h>
#include <fcntl.h>
#include <sys/file.h>
#include <filesystem>
#include <fstream>
#include <i2c_interface.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>

/**
 * @brief Constructor for I2CInterface class
 * @param devpath Wide character std::string containing the device path to the I2C device
 *
 * Initializes the I2C interface by opening the AMC device and setting up peripheral access.
 * Sets the init flag to true if successful, false otherwise.
 */
I2CInterface::I2CInterface(const std::string &devpath)
{
	TRACING();
	amchandle = -1;
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
 * Cleans up resources by closing the AMC device handle if it was successfully initialized.
 */
I2CInterface::~I2CInterface()
{
	TRACING();
	if (amchandle >= 0) {
		closeAmc();
		amchandle = -1;
	}
}

/**
 * @brief Finds all I2C devices in the system
 * @return std::vector<std::string> List of device paths found in /sys/bus/i2c/devices
 *
 * Scans the /sys/bus/i2c/devices directory to discover all available I2C devices.
 * Returns an empty std::vector if the directory doesn't exist or no devices are found.
 */
std::vector<std::string> findI2CDevices()
{
	TRACING();
	std::vector<std::string> devices;
	std::string devicesPath = "/sys/bus/i2c/devices";

	// Check if the directory exists
	if (!std::filesystem::exists(devicesPath)) {
		ERR("I2C devices directory does not exist: {}\n", devicesPath.c_str());
		return devices;
	}

	// Iterate through all entries in the directory
	for (const auto &entry : std::filesystem::directory_iterator(devicesPath)) {
		if (entry.is_directory()) {
			std::string dirname = entry.path().filename().string();
			devices.push_back(entry.path().string());
		}
	}

	return devices;
}

/**
 * @brief Retrieves the name of an I2C device
 * @param devicePath Path to the I2C device in /sys/bus/i2c/devices
 * @return std::string The device name read from the device's name file
 *
 * Reads the device name from the 'name' file in the specified device path.
 * Returns empty std::string if the name file cannot be opened or read.
 */
std::string getI2CDeviceName(const std::string &devicePath)
{
	TRACING();
	std::string name;
	std::ifstream nameFile(devicePath + "/name");
	if (nameFile.is_open()) {
		getline(nameFile, name);
		nameFile.close();
	}
	return name;
}

/**
 * @brief Opens the AMC I2C device
 * @param devpath Narrow std::string device path (e.g. /dev/i2c-21)
 * @return bool True if device opened successfully, false otherwise
 *
 * Converts the wide character device path to a regular std::string and opens
 * the I2C device with read/write and non-blocking flags.
 */
bool I2CInterface::openAmc(const std::string &devpath)
{
	TRACING();
	if (devpath.empty()) {
		ERR("Device path is empty\n");
		return false;
	}
	amchandle = open(devpath.c_str(), O_RDWR | O_NONBLOCK);
	if (amchandle < 0) {
		ERR("Failed to open I2C device {}: {}\n", devpath.c_str(), strerror(errno));
		return false;
	}
	if (::flock(amchandle, LOCK_EX | LOCK_NB) < 0) {
		ERR("Failed to acquire exclusive lock on I2C device %s with error: %s\n", devpath.c_str(), strerror(errno));
		close(amchandle);
		amchandle = -1;
		return false;
	}
	return true;
}

/**
 * @brief Establishes communication with the AMC peripheral device
 * @return bool True if peripheral access established successfully, false otherwise
 *
 * Uses ioctl with I2C_SLAVE to acquire bus access and communicate with
 * the AMC device at the predefined I2C address.
 */
bool I2CInterface::open_amc_peripheral()
{
	TRACING();
	int rv;
	if ((rv = ioctl(amchandle, I2C_SLAVE, AMC_I2C_ADDR)) < 0) {
		ERR("Failed to acquire bus access and/or talk to slave. Error code: {}\n", rv);
		return false;
	}
	return true;
}

/**
 * @brief Writes data to the AMC device
 * @param writeBuffer Pointer to the data buffer to write
 * @param writeSize Number of bytes to write
 * @return bool True if write operation successful, false otherwise
 *
 * Performs a write operation to the I2C device using the established handle.
 */
bool I2CInterface::writeAmc(void *writeBuffer, size_t writeSize)
{
	TRACING();
	if (amchandle < 0) {
		ERR("Invalid I2C device handle\n");
		return false;
	}

	ssize_t bytesWritten = write(amchandle, writeBuffer, writeSize);
	if (bytesWritten < 0) {
		ERR("Failed to write to I2C device: {}\n", strerror(errno));
		return false;
	}
	return true;
}

/**
 * @brief Reads data from the AMC device
 * @param readBuffer Pointer to the buffer where read data will be stored
 * @param readSize Number of bytes to read
 * @return bool True if read operation successful, false otherwise
 *
 * Performs a read operation from the I2C device using the established handle.
 */
bool I2CInterface::readAmc(void *readBuffer, size_t readSize)
{
	TRACING();
	if (amchandle < 0) {
		ERR("Invalid I2C device handle\n");
		return false;
	}

	ssize_t bytesRead = read(amchandle, readBuffer, readSize);
	DBG("Number of bytes read from AMC: {}\n", bytesRead);
	if (bytesRead < 0) {
		ERR("Failed to read from I2C device: {}\n", strerror(errno));
		return false;
	}
	return true;
}

/**
 * @brief Closes the AMC device handle
 * @return bool True if device closed successfully, false otherwise
 *
 * Closes the file descriptor for the I2C device.
 */
bool I2CInterface::closeAmc()
{
	TRACING();
	return close(amchandle) == 0;
}

/**
 * @brief Gets the GPU device name that owns the specified I2C device
 * @param i2cDevicePath Path to the I2C device (e.g., "/sys/bus/i2c/devices/i2c-19")
 * @return std::string GPU device name (e.g., "0000:01:00.0") or empty string on error
 *
 * Equivalent to the shell command: basename $(realpath ../..)
 * Resolves the parent directory path and extracts the GPU device identifier.
 */
std::string getGpuDeviceFromI2C(const std::string &i2cDevicePath)
{
	TRACING();
	try {
		// Resolve the real path of "../.." relative to the I2C device path
		std::filesystem::path devicePath(i2cDevicePath);
		std::filesystem::path parentParentPath = devicePath / ".." / "..";
		std::filesystem::path realPath = std::filesystem::canonical(parentParentPath);

		// Return the basename (filename) of the resolved path
		return realPath.filename().string();
	} catch (const std::filesystem::filesystem_error &ex) {
		ERR("Failed to resolve GPU device path for {}: {}\n", i2cDevicePath.c_str(), ex.what());
		return "";
	}
}

/**
 * @brief Discovers AMC cards in the system
 * @param amcDeviceList Pointer to std::vector that will be populated with found AMC device paths
 * @return int Status code (0 for success)
 *
 * Scans all I2C devices in the system looking for devices named "amc".
 * Extracts the I2C bus number from the device path and constructs the
 * corresponding /dev/i2c-X device path for each AMC device found.
 */
int amcCardDiscovery(std::vector<amcCardInfo> *amcDeviceList)
{
	TRACING();
	int cardsCount = 0;
	std::vector<std::string> devices = findI2CDevices();
	for (const auto &device : devices) {
		std::string name = getI2CDeviceName(device);
		if (name == "amc") {
			// Found the device with the specified name, the first few characters of the path
			// until a hyphen are the I2C bus number
			// Example: "/sys/bus/i2c/devices/21-0040" -> "21"
			// Another example: "/sys/bus/i2c/devices/7-0041" -> "7"
			// This needs to be added to i2c- so that we can open the device
			// Example: "/dev/i2c-21"
			// Example: "/dev/i2c-7"
			// Extract the filename portion after the last slash
			std::string filename = device.substr(device.find_last_of('/') + 1);
			// Find the hyphen in the filename
			const size_t hyphenPos = filename.find('-');
			std::string busNumber = (hyphenPos != std::string::npos) ? filename.substr(0, hyphenPos) : "";
			if (busNumber.empty() || !std::all_of(busNumber.begin(), busNumber.end(),
												  [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
				ERR("Invalid I2C bus number '{}' from device path: {}\n", busNumber.c_str(), device.c_str());
				continue;
			}
			const std::string i2cDevice = "/dev/i2c-" + busNumber;
			amcCardInfo info;
			info.amcDevicePath = i2cDevice;
			info.gpuParentPath = getGpuDeviceFromI2C("/sys/bus/i2c/devices/i2c-" + busNumber);
			amcDeviceList->push_back(info);
			DBG("Found AMC device: {}\n", i2cDevice.c_str());
			cardsCount++;
		}
	}
	return cardsCount;
}