/* 
 *  Copyright (C) 2021-2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_property.cpp
 */

#include "device_property.h"

namespace xpum {

const std::string DeviceProperty::TYPE = "TYPE";
const std::string DeviceProperty::VENDOR_ID = "VENDOR_ID";
const std::string DeviceProperty::DEVICE_ID = "DEVICE_ID";
const std::string DeviceProperty::SUB_DEVICE_ID = "SUB_DEVICE_ID";
const std::string DeviceProperty::CORE_CLOCK_RATE = "CORE_CLOCK_RATE";
const std::string DeviceProperty::MAX_MEM_ALLOC_SIZE = "MAX_MEM_ALLOC_SIZE";
const std::string DeviceProperty::MAX_HARDWARE_CONTEXTS = "MAX_HARDWARE_CONTEXTS";
const std::string DeviceProperty::MAX_COMMAND_QUEUE_PRIORITY = "MAX_COMMAND_QUEUE_PRIORITY";
const std::string DeviceProperty::NUM_THREADS_PER_EU = "NUM_THREADS_PER_EU";
const std::string DeviceProperty::PYSICAL_EU_SIMD_WIDTH = "PYSICAL_EU_SIMD_WIDTH";
const std::string DeviceProperty::NUM_EUS_PER_SUB_SLICE = "NUM_EUS_PER_SUB_SLICE";
const std::string DeviceProperty::NUM_SUB_SLICES_PER_SLICE = "NUM_SUB_SLICES_PER_SLICE";
const std::string DeviceProperty::NUM_SLICES = "NUM_SLICES";
const std::string DeviceProperty::TIMER_RESOLUTION = "TIMER_RESOLUTION";
const std::string DeviceProperty::TIMESTAMP_VALID_BITS = "TIMESTAMP_VALID_BITS";
const std::string DeviceProperty::KERNEL_TIMESTAMP_VALID_BITS = "KERNEL_TIMESTAMP_VALID_BITS";
const std::string DeviceProperty::UUID = "UUID";
const std::string DeviceProperty::DEVICE_NAME = "DEVICE_NAME";
const std::string DeviceProperty::VENDOR_NAME = "VENDOR_NAME";
const std::string DeviceProperty::MODEL_NAME = "MODEL_NAME";
const std::string DeviceProperty::BOARD_NUMBER = "BOARD_NUMBER";
const std::string DeviceProperty::BRAND_NAME = "BRAND_NAME";
const std::string DeviceProperty::DRIVER_VERSION = "DRIVER_VERSION";
const std::string DeviceProperty::NUM_SUB_DEVICES = "NUM_SUB_DEVICES";
const std::string DeviceProperty::SERIAL_NUMBER = "SERIAL_NUMBER";
const std::string DeviceProperty::FLAGS = "FLAGS";
const std::string DeviceProperty::MEMORY_PHYSICAL_SIZE = "MEMORY_PHYSICAL_SIZE";
const std::string DeviceProperty::MEMORY_FREE_SIZE = "MEMORY_FREE_SIZE";
const std::string DeviceProperty::MEMORY_ALLOCATABLE_SIZE = "MEMORY_ALLOCATABLE_SIZE";
const std::string DeviceProperty::MEMORY_HEALTH = "MEMORY_HEALTH";
const std::string DeviceProperty::FIRMWARE_NAME = "FIRMWARE_NAME";
const std::string DeviceProperty::FIRMWARE_VERSION = "FIRMWARE_VERSION";
const std::string DeviceProperty::BDF_ADDRESS = "BDF ADDRESS";
const std::string DeviceProperty::PCI_SLOT = "PCI SLOT";
const std::string DeviceProperty::SOCKET_ID = "SOCKET ID";
const std::string DeviceProperty::PCIE_GEN = "PCIE_GEN";
const std::string DeviceProperty::PCIE_MAX_LINK_WIDTH = "PCIE_MAX_LINK_WIDTH";
const std::string DeviceProperty::PCIE_MAX_BANDWIDTH = "PCIE_MAX_BANDWIDTH";
const std::string DeviceProperty::MEM_BUS_WIDTH = "MEM_BUS_WIDTH";
const std::string DeviceProperty::MEM_CHANNEL_NUM = "MEM_CHANNEL_NUM";

} // end namespace xpum
