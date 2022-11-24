/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file device_property.h
 */

#pragma once

#include <string>

namespace xpum {

class DeviceProperty {
   public:
    static const std::string TYPE;

    static const std::string VENDOR_ID;

    static const std::string DEVICE_ID;

    static const std::string SUB_DEVICE_ID;

    static const std::string CORE_CLOCK_RATE;

    static const std::string MAX_MEM_ALLOC_SIZE;

    static const std::string MAX_HARDWARE_CONTEXTS;

    static const std::string MAX_COMMAND_QUEUE_PRIORITY;

    static const std::string NUM_THREADS_PER_EU;

    static const std::string PYSICAL_EU_SIMD_WIDTH;

    static const std::string NUM_EUS_PER_SUB_SLICE;

    static const std::string NUM_SUB_SLICES_PER_SLICE;

    static const std::string NUM_SLICES;

    static const std::string TIMER_RESOLUTION;

    static const std::string TIMESTAMP_VALID_BITS;

    static const std::string KERNEL_TIMESTAMP_VALID_BITS;

    static const std::string UUID;

    static const std::string DEVICE_NAME;

    static const std::string VENDOR_NAME;

    static const std::string MODEL_NAME;

    static const std::string SERIAL_NUMBER;

    static const std::string BOARD_NUMBER;

    static const std::string BRAND_NAME;

    static const std::string NUM_SUB_DEVICES;

    static const std::string DRIVER_VERSION;

    static const std::string FLAGS;

    static const std::string MEMORY_PHYSICAL_SIZE;

    static const std::string MEMORY_FREE_SIZE;

    static const std::string MEMORY_ALLOCATABLE_SIZE;

    static const std::string MEMORY_HEALTH;

    static const std::string FIRMWARE_NAME;

    static const std::string FIRMWARE_VERSION;

    static const std::string BDF_ADDRESS;

    static const std::string PCI_SLOT;

    static const std::string SOCKET_ID;

    static const std::string PCIE_GEN;

    static const std::string PCIE_MAX_LINK_WIDTH;

    static const std::string MEM_BUS_WIDTH;

    static const std::string MEM_CHANNEL_NUM;
};

} // end namespace xpum
