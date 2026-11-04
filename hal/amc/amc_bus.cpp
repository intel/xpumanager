/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "amc_bus.h"
#include "common.h"
#include "i2c_interface.h"

namespace {

/**
 * @brief AmcBus over the AMC's I2C device (oal I2CInterface on Linux and Windows)
 */
class I2cAmcBus final : public AmcBus
{
public:
	explicit I2cAmcBus(const std::string &devpath) : mDev(devpath) {}

	[[nodiscard]] bool isOpen() const override { return mDev.isInit(); }

	bool write(std::span<const uint8_t> frame) override
	{
		// writeAmc() takes a non-const pointer but only reads from it
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
		return mDev.writeAmc(const_cast<uint8_t *>(frame.data()), frame.size());
	}

	bool read(std::span<uint8_t> buffer) override { return mDev.readAmc(buffer.data(), buffer.size()); }

	[[nodiscard]] bool tryLock() override { return mDev.tryLock(); }

	void unlock() override { mDev.unlock(); }

	[[nodiscard]] bool isBusy() const override { return mDev.isBusy(); }

	bool close() override { return mDev.closeAmc(); }

private:
	I2CInterface mDev;
};

} // namespace

std::unique_ptr<AmcBus> makeI2cAmcBus(const std::string &devpath)
{
	if (devpath.empty()) {
		ERR("Device path is empty\n");
		return nullptr;
	}
	return std::make_unique<I2cAmcBus>(devpath);
}
