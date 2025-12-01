/*
 *  Copyright (C) 2025 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *
 *  Example C program demonstrating XPUM C API usage
 */

#include <stdio.h>
#include <stdlib.h>
#include "../../xpum_api.h"

int main(void)
{
	printf("XPUM C API Example\n");
	printf("==================\n\n");

	// Initialize XPUM
	printf("Initializing XPUM...\n");
	xpum_result_t result = xpumInit();
	if (result != XPUM_OK) {
		fprintf(stderr, "Failed to initialize XPUM: %d\n", result);
		return 1;
	}
	printf("XPUM initialized successfully\n\n");

	// Get version info
	printf("Getting version information...\n");
	int versionCount = 0;
	result = xpumVersionInfo(NULL, &versionCount);
	if (result == XPUM_OK && versionCount > 0) {
		XpumVersionInfo *versions = malloc(versionCount * sizeof(XpumVersionInfo));
		result = xpumVersionInfo(versions, &versionCount);
		if (result == XPUM_OK) {
			for (int i = 0; i < versionCount; i++) {
				printf("  Version %d: %s\n", versions[i].version, versions[i].versionString);
			}
		}
		free(versions);
	}
	printf("\n");

	// Get device list
	printf("Enumerating devices...\n");
	int deviceCount = 0;
	result = xpumGetDeviceList(NULL, &deviceCount);
	if (result != XPUM_OK) {
		fprintf(stderr, "Failed to get device count: %d\n", result);
		xpumShutdown();
		return 1;
	}

	printf("Found %d device(s)\n\n", deviceCount);

	if (deviceCount > 0) {
		XpumDeviceBasicInfo *devices = malloc(deviceCount * sizeof(XpumDeviceBasicInfo));
		result = xpumGetDeviceList(devices, &deviceCount);

		if (result == XPUM_OK) {
			for (int i = 0; i < deviceCount; i++) {
				printf("Device %d:\n", devices[i].deviceId);
				printf("  Name: %s\n", devices[i].deviceName);
			printf("  Vendor: %s\n", devices[i].vendorName);
			printf("  BDF: %s\n", devices[i].pcibdfAddress);
				printf("  UUID: %s\n", devices[i].uuid);
				printf("\n");

				// Get detailed properties
				xpum_device_properties_t props;
				result = xpumGetDeviceProperties(devices[i].deviceId, &props);
				if (result == XPUM_OK) {
					printf("  Detailed Properties (%d properties):\n", props.propertyLen);
					for (int j = 0; j < props.propertyLen; j++) {
						printf("    Property %d: %s\n", props.properties[j].name, props.properties[j].value);
					}
					printf("\n");
				}
			}
		}

		free(devices);
	}

	// Test group creation
	printf("Testing group management...\n");
	xpum_group_id_t groupId = 0;
	result = xpumGroupCreate("test_group", &groupId);
	if (result == XPUM_OK) {
		printf("Created group 'test_group' with ID: %u\n", groupId);

		// Add first device to group if available
		if (deviceCount > 0) {
			result = xpumGroupAddDevice(groupId, 0);
			if (result == XPUM_OK) {
				printf("Added device 0 to group\n");
			}

			// Get group info
			xpum_group_info_t groupInfo;
			result = xpumGroupGetInfo(groupId, &groupInfo);
			if (result == XPUM_OK) {
				printf("Group info:\n");
				printf("  Name: %s\n", groupInfo.groupName);
				printf("  Device count: %d\n", groupInfo.count);
			}
		}

		// Cleanup group
		result = xpumGroupDestroy(groupId);
		if (result == XPUM_OK) {
			printf("Destroyed group\n");
		}
	}
	printf("\n");

	// Shutdown XPUM
	printf("Shutting down XPUM...\n");
	result = xpumShutdown();
	if (result != XPUM_OK) {
		fprintf(stderr, "Failed to shutdown XPUM: %d\n", result);
		return 1;
	}

	printf("XPUM shutdown successfully\n");
	printf("\nExample completed!\n");

	return 0;
}
