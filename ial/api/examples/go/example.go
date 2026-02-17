/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

// Copyright (C) 2025 Intel Corporation
// SPDX-License-Identifier: MIT
//
// Example Go program demonstrating XPUM C API usage via cgo

package main

/*
#cgo pkg-config: xpum_api

#include <level_zero/ze_api.h>
#include "xpum_api.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"unsafe"
)

func main() {
	fmt.Println("XPUM C API Go Example")
	fmt.Println("======================\n")

	// Initialize XPUM
	fmt.Println("Initializing XPUM...")
	result := C.xpumInit()
	if result != C.XPUM_OK {
		panic(fmt.Sprintf("Failed to initialize XPUM: %d", result))
	}
	defer C.xpumShutdown()
	fmt.Println("XPUM initialized successfully\n")

	// Get version info
	fmt.Println("Getting version information...")
	var versionCount C.int
	result = C.xpumVersionInfo(nil, &versionCount)
	if result == C.XPUM_OK && versionCount > 0 {
		versions := make([]C.XpumVersionInfo, versionCount)
		result = C.xpumVersionInfo(&versions[0], &versionCount)
		if result == C.XPUM_OK {
			for i := 0; i < int(versionCount); i++ {
				versionStr := C.GoString(&versions[i].versionString[0])
				fmt.Printf("  Version %d: %s\n", versions[i].version, versionStr)
			}
		}
	}
	fmt.Println()

	// Get device list
	fmt.Println("Enumerating devices...")
	var deviceCount C.int
	result = C.xpumGetDeviceList(nil, &deviceCount)
	if result != C.XPUM_OK {
		panic(fmt.Sprintf("Failed to get device count: %d", result))
	}

	fmt.Printf("Found %d device(s)\n\n", deviceCount)

	if deviceCount > 0 {
		devices := make([]C.XpumDeviceBasicInfo, deviceCount)
		result = C.xpumGetDeviceList(&devices[0], &deviceCount)

		if result == C.XPUM_OK {
			for i := 0; i < int(deviceCount); i++ {
				fmt.Printf("Device %d:\n", devices[i].deviceId)
				fmt.Printf("  Name: %s\n", C.GoString(&devices[i].deviceName[0]))
				fmt.Printf("  Vendor: %s\n", C.GoString(&devices[i].vendorName[0]))
				fmt.Printf("  BDF: %s\n", C.GoString(&devices[i].pcibdfAddress[0]))
				fmt.Printf("  UUID: %s\n", C.GoString(&devices[i].uuid[0]))
				fmt.Println()

				// Get detailed properties
				var props C.xpum_device_properties_t
				result = C.xpumGetDeviceProperties(devices[i].deviceId, &props)
				if result == C.XPUM_OK {
					fmt.Printf("  Detailed Properties (%d properties):\n", props.propertyLen)
					for j := 0; j < int(props.propertyLen); j++ {
						fmt.Printf("    Property %d: %s\n", props.properties[j].name,
							C.GoString(&props.properties[j].value[0]))
					}
					fmt.Println()
				}
			}
		}
	}

	// Test group creation
	fmt.Println("Testing group management...")
	groupName := C.CString("test_group")
	defer C.free(unsafe.Pointer(groupName))

	var groupID C.xpum_group_id_t
	result = C.xpumGroupCreate(groupName, &groupID)
	if result == C.XPUM_OK {
		fmt.Printf("Created group 'test_group' with ID: %d\n", groupID)

		// Add first device to group if available
		if deviceCount > 0 {
			result = C.xpumGroupAddDevice(groupID, 0)
			if result == C.XPUM_OK {
				fmt.Println("Added device 0 to group")
			}

			// Get group info
			var groupInfo C.xpum_group_info_t
			result = C.xpumGroupGetInfo(groupID, &groupInfo)
			if result == C.XPUM_OK {
				fmt.Println("Group info:")
				fmt.Printf("  Name: %s\n", C.GoString(&groupInfo.groupName[0]))
				fmt.Printf("  Device count: %d\n", groupInfo.count)
			}
		}

		// Cleanup group
		result = C.xpumGroupDestroy(groupID)
		if result == C.XPUM_OK {
			fmt.Println("Destroyed group")
		}
	}
	fmt.Println()

	fmt.Println("Example completed!")
}
