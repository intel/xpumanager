/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __AMC_BUS_H
#define __AMC_BUS_H

#include <cstdint>
#include <memory>
#include <span>
#include <string>

/**
 * @brief The byte link between xpu-smi and one AMC, as the MCTP transport uses it
 *
 * A write is one SMBus block write that starts at the command code; the bus puts the AMC's address
 * on the wire. A read is one I2C read of the AMC's I2C target, which returns whatever it has staged,
 * or padding when it has nothing. The lock gives one process the AMC for the length of an operation.
 *
 * The transport and everything above it only use this interface, so a test can give a card a simulated
 * AMC instead of an I2C device (hal/amc/test/amc_simulator.h).
 */
class AmcBus
{
public:
	AmcBus() = default;
	virtual ~AmcBus() = default;
	AmcBus(const AmcBus &) = delete;
	AmcBus &operator=(const AmcBus &) = delete;
	AmcBus(AmcBus &&) = delete;
	AmcBus &operator=(AmcBus &&) = delete;

	/**
	 * @brief Whether the bus can be used
	 * @return bool True once the AMC's device has been opened
	 */
	[[nodiscard]] virtual bool isOpen() const = 0;

	/**
	 * @brief Write one SMBus frame to the AMC
	 * @param frame Bytes from the command code through the PEC
	 * @return bool True when the whole frame was written
	 */
	virtual bool write(std::span<const uint8_t> frame) = 0;

	/**
	 * @brief Read from the AMC's I2C target
	 * @param buffer Filled with exactly buffer.size() bytes
	 * @return bool True when the read succeeded
	 */
	virtual bool read(std::span<uint8_t> buffer) = 0;

	/**
	 * @brief Take the AMC for this process without waiting
	 * @return bool True when the lock was taken; isBusy() tells why it was not
	 */
	[[nodiscard]] virtual bool tryLock() = 0;

	/**
	 * @brief Give the AMC back after tryLock()
	 */
	virtual void unlock() = 0;

	/**
	 * @brief Whether the last tryLock() failed because another process holds the AMC
	 * @return bool True when the AMC is in use elsewhere
	 */
	[[nodiscard]] virtual bool isBusy() const = 0;

	/**
	 * @brief Close the AMC's device; nothing can be sent or received afterwards and isOpen() is false
	 * @return bool True when the device was closed, or was closed already
	 */
	virtual bool close() = 0;
};

/**
 * @brief Open the AMC behind an I2C device through the OS layer (oal I2CInterface)
 * @param devpath The AMC's I2C device, such as /dev/i2c-3
 * @return std::unique_ptr<AmcBus> The bus, check isOpen(); nullptr when devpath is empty
 */
std::unique_ptr<AmcBus> makeI2cAmcBus(const std::string &devpath);

#endif // __AMC_BUS_H
