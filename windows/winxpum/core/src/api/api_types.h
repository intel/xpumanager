/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file api_types.h
 */

#pragma once

#include "infrastructure/measurement_type.h"

namespace xpum {

    extern "C" {

        struct Api_result_t {
            int error_code;
            const char* msg;
        };

        struct Property_t {
            const char* name;
            const char* value;
        };

        const int MAX_PROPERTY = 100;
        struct Device_t {
            const char* device_id;
            Property_t properties[MAX_PROPERTY];
            int property_len;
        };
    }

    typedef enum xpum_device_internal_property_name_enum {
        XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_TYPE,                    ///< Device type
        XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_NAME,                    ///< Device name
        XPUM_DEVICE_PROPERTY_INTERNAL_VENDOR_NAME,                    ///< Vendor name
        XPUM_DEVICE_PROPERTY_INTERNAL_UUID,                           ///< Device uuid
        XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_FUNCTION_TYPE,           ///< Device function type, PF or VF
        XPUM_DEVICE_PROPERTY_INTERNAL_PCI_DEVICE_ID,                  ///< The PCI device id of device
        XPUM_DEVICE_PROPERTY_INTERNAL_PCI_VENDOR_ID,                  ///< The PCI vendor id of device
        XPUM_DEVICE_PROPERTY_INTERNAL_PCI_BDF_ADDRESS,                ///< The PCI bdf address of device
        XPUM_DEVICE_PROPERTY_INTERNAL_DRM_DEVICE,                     ///< DRM Device 
        XPUM_DEVICE_PROPERTY_INTERNAL_PCI_SLOT,                       ///< PCI slot of device
        XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_GENERATION,                ///< PCIe generation
        XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_MAX_LINK_WIDTH,            ///< PCIe max link width
        XPUM_DEVICE_PROPERTY_INTERNAL_OAM_SOCKET_ID,                  ///< Socket ID of OAM socket GPU
        XPUM_DEVICE_PROPERTY_INTERNAL_DEVICE_STEPPING,                ///< The stepping of device
        XPUM_DEVICE_PROPERTY_INTERNAL_DRIVER_VERSION,                 ///< The driver version
        XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_NAME,              ///< The GFX firmware name of device
        XPUM_DEVICE_PROPERTY_INTERNAL_GFX_FIRMWARE_VERSION,           ///< The GFX firmware version of device
        XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_NAME,         ///< The GFX data firmware name of device
        XPUM_DEVICE_PROPERTY_INTERNAL_GFX_DATA_FIRMWARE_VERSION,      ///< The GFX data firmware version of device
        XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_NAME,       ///< The GFX_PSCBIN firmware name of device
        XPUM_DEVICE_PROPERTY_INTERNAL_GFX_PSCBIN_FIRMWARE_VERSION,    ///< The GFX_PSCBIN firmware version of device
        XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_NAME,              ///< The AMC firmware name of device
        XPUM_DEVICE_PROPERTY_INTERNAL_AMC_FIRMWARE_VERSION,           ///< The AMC firmware version of device
        XPUM_DEVICE_PROPERTY_INTERNAL_SERIAL_NUMBER,                  ///< Serial number
        XPUM_DEVICE_PROPERTY_INTERNAL_CORE_CLOCK_RATE_MHZ,            ///< Clock rate for device core, in MHz
        XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_PHYSICAL_SIZE_BYTE,      ///< Device free memory size, in bytes
        XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_FREE_SIZE_BYTE,          ///< The free memory, in bytes
        XPUM_DEVICE_PROPERTY_INTERNAL_MAX_MEM_ALLOC_SIZE_BYTE,        ///< The total allocatable memory, in bytes
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEMORY_CHANNELS,      ///< Number of memory channels
        XPUM_DEVICE_PROPERTY_INTERNAL_MEMORY_BUS_WIDTH,               ///< Memory bus width
        XPUM_DEVICE_PROPERTY_INTERNAL_MAX_HARDWARE_CONTEXTS,          ///< Maximum number of logical hardware contexts
        XPUM_DEVICE_PROPERTY_INTERNAL_MAX_COMMAND_QUEUE_PRIORITY,     ///< Maximum priority for command queues. Higher value is higher priority
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS,                  ///< The number of EUs
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUBDEVICE,            ///< The number of subdevices
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_TILES,                ///< The number of tiles
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SLICES,               ///< Maximum number of slices
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_SUB_SLICES_PER_SLICE, ///< Maximum number of sub-slices per slice
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_EUS_PER_SUB_SLICE,    ///< Maximum number of EUs per sub-slice
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_THREADS_PER_EU,       ///< Maximum number of threads per EU
        XPUM_DEVICE_PROPERTY_INTERNAL_PHYSICAL_EU_SIMD_WIDTH,         ///< The physical EU simd width
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENGINES,        ///< The number of media engines
        XPUM_DEVICE_PROPERTY_INTERNAL_NUMBER_OF_MEDIA_ENH_ENGINES,    ///< The number of media enhancement engines
        XPUM_DEVICE_PROPERTY_INTERNAL_LINUX_KERNEL_VERSION,           ///< Linux kenel version
        XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_NUMBER,             ///< Number of fabric ports
        XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_MAX_RX_SPEED,       ///< Maximum speed supported by the receive side of the port (sum of all lanes)
        XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_MAX_TX_SPEED,       ///< Maximum speed supported by the transmit side of the port (sum of all lanes)
        XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_RX_LANES_NUMBER,    ///< The number of lanes per the receive side of the port
        XPUM_DEVICE_PROPERTY_INTERNAL_FABRIC_PORT_TX_LANES_NUMBER,    ///< The number of lanes per the transmit side of the port
        XPUM_DEVICE_PROPERTY_INTERNAL_SKU_TYPE,                       ///< The type of SKU
        XPUM_DEVICE_PROPERTY_INTERNAL_PCIE_MAX_BANDWIDTH,             ///< PCIe max link speed
        XPUM_DEVICE_PROPERTY_INTERNAL_MAX
    } xpum_device_internal_property_name_t;

} // end namespace xpum
