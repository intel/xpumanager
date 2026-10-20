/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef FS_LOCK_H
#define FS_LOCK_H

#include "pci_address.h"
#include <cstdint>
#include <list>
#include <optional>
#include <set>
#include <span>

// RAII cross-process lock to ensure only one firmware update runs at a time on a device.
// Updates of different devices do not exclude each other. The lock object (a file on
// Linux, a named semaphore on Windows) and its name are OS-specific and owned by each
// acquire() implementation, so they are deliberately not part of this interface.
class FSLock
{
public:
	/**
	 * @brief Takes the firmware update lock of one device, without waiting
	 *
	 * @param[in] device PCI address of the device; its canonical text names the lock
	 */
	explicit FSLock(const PciAddress &device) { acquire(device); }
	~FSLock() { release(); }

	/**
	 * @brief Reports whether this object holds the lock
	 *
	 * @retval true  The lock is held until this object is destroyed
	 * @retval false The lock could not be taken; see busy()
	 */
	[[nodiscard]] bool locked() const { return acquired; }

	/**
	 * @brief Reports whether the lock could not be taken because another update holds it
	 *
	 * @retval true  Another process (or another FSLock) holds this device's lock
	 * @retval false The lock is held by this object, or taking it failed for another reason
	 *               (not running as root/administrator, an unusable lock directory), which
	 *               acquire() has already reported
	 */
	[[nodiscard]] bool busy() const { return heldByAnotherUpdate; }

	// Owns an OS handle/fd released in the destructor: copying would duplicate
	// ownership and double-close, moving would need handle invalidation.
	FSLock(const FSLock &) = delete;
	FSLock &operator=(const FSLock &) = delete;
	FSLock(FSLock &&) = delete;
	FSLock &operator=(FSLock &&) = delete;

private:
	uintptr_t handle = 0;	// Cast HANDLE/fd to uintptr_t
	uintptr_t nsHandle = 0; // Windows only: private namespace scoping the semaphore
	bool acquired = false;
	bool heldByAnotherUpdate = false;

	void acquire(const PciAddress &device);
	void release();
};

// The firmware update locks of all devices in one request, taken together. Each device is
// locked once however often it is listed, so a request never collides with itself. If any
// lock cannot be taken, none are kept.
class FSLockSet
{
public:
	/**
	 * @brief Takes the firmware update lock of every device, without waiting
	 *
	 * @param[in] devices PCI addresses of the devices; duplicates are allowed
	 */
	explicit FSLockSet(std::span<const PciAddress> devices)
	{
		for (const PciAddress &device : std::set<PciAddress>(devices.begin(), devices.end())) {
			const FSLock &lock = locks.emplace_back(device);
			if (!lock.locked()) {
				failed = device;
				failedBusy = lock.busy();
				locks.clear();
				return;
			}
		}
	}

	FSLockSet(const FSLockSet &) = delete;
	FSLockSet &operator=(const FSLockSet &) = delete;
	FSLockSet(FSLockSet &&) = delete;
	FSLockSet &operator=(FSLockSet &&) = delete;
	~FSLockSet() = default;

	/**
	 * @brief Reports whether every device's lock is held
	 *
	 * @retval true  All locks are held until this object is destroyed
	 * @retval false At least one lock could not be taken, and none are held
	 */
	[[nodiscard]] bool locked() const noexcept { return !failed.has_value(); }

	/**
	 * @brief Returns the device whose lock could not be taken
	 *
	 * @retval address That device
	 * @retval nullopt Every lock is held
	 */
	[[nodiscard]] const std::optional<PciAddress> &failedDevice() const noexcept { return failed; }

	/**
	 * @brief Reports whether that device's lock is held by another update
	 *
	 * @retval true  Another update of failedDevice() is in progress
	 * @retval false Every lock is held, or taking failedDevice()'s lock failed for another reason
	 */
	[[nodiscard]] bool failedBecauseBusy() const noexcept { return failedBusy; }

private:
	std::list<FSLock> locks;
	std::optional<PciAddress> failed;
	bool failedBusy = false;
};

#endif // FS_LOCK_H
