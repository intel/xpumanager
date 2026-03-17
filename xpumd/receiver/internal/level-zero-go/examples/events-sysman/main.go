// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package main

import (
	"fmt"
	"log"
	"sync"
	"time"

	"github.com/intel/level-zero-go/sysman"
)

// allEventFlags registers interest in every defined event type.
const allEventFlags = sysman.EventTypeFlags(
	sysman.EVENT_TYPE_FLAG_DEVICE_DETACH |
		sysman.EVENT_TYPE_FLAG_DEVICE_ATTACH |
		sysman.EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_ENTER |
		sysman.EVENT_TYPE_FLAG_DEVICE_SLEEP_STATE_EXIT |
		sysman.EVENT_TYPE_FLAG_FREQ_THROTTLED |
		sysman.EVENT_TYPE_FLAG_ENERGY_THRESHOLD_CROSSED |
		sysman.EVENT_TYPE_FLAG_TEMP_CRITICAL |
		sysman.EVENT_TYPE_FLAG_TEMP_THRESHOLD1 |
		sysman.EVENT_TYPE_FLAG_TEMP_THRESHOLD2 |
		sysman.EVENT_TYPE_FLAG_MEM_HEALTH |
		sysman.EVENT_TYPE_FLAG_FABRIC_PORT_HEALTH |
		sysman.EVENT_TYPE_FLAG_PCI_LINK_HEALTH |
		sysman.EVENT_TYPE_FLAG_RAS_CORRECTABLE_ERRORS |
		sysman.EVENT_TYPE_FLAG_RAS_UNCORRECTABLE_ERRORS |
		sysman.EVENT_TYPE_FLAG_DEVICE_RESET_REQUIRED,
	// NOTE: sysman.EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED disabled, causes ERROR_INVALID_ENUMERATION error
)

const (
	pollTimeout    time.Duration = -1 // negative value indicates infinite timeout
	pollRetryDelay               = 1 * time.Second
)

func main() {
	if ret := sysman.Init(0); ret != nil {
		log.Fatalf("Failed to initialize sysman: %v", ret)
	}

	drivers, err := sysman.DriverGet()
	if err != nil {
		log.Fatalf("Failed to get drivers: %v", err)
	}
	if len(drivers) == 0 {
		log.Fatal("No drivers found")
	}

	var wg sync.WaitGroup
	for i, driver := range drivers {
		wg.Go(func() {
			pollDriver(i, driver)
		})
	}

	fmt.Println("Listening for events. Press Ctrl+C to exit.")

	wg.Wait()
}

func pollDriver(idx int, driver *sysman.Driver) {
	sysDevices, err := driver.DeviceGet()
	if err != nil {
		log.Printf("Error: driver %d: failed to get devices: %v", idx, err)
		return
	}
	if len(sysDevices) == 0 {
		log.Printf("Driver %d: no devices found", idx)
		return
	}

	var devices []*sysman.Device
	var deviceIdentifiers []string

	for i, device := range sysDevices {
		if err := device.EventRegister(allEventFlags); err != nil {
			log.Printf("Error: driver %d device %d: EventRegister: %v", idx, i, err)
			continue
		}

		deviceIdentifier := fmt.Sprintf("driver %d device %d", idx, i)

		pciBDF := "????:??:??.?"
		if pciProps, err := device.PciGetProperties(); err == nil {
			pciBDF = fmt.Sprintf("%04x:%02x:%02x.%x", pciProps.Address.Domain, pciProps.Address.Bus, pciProps.Address.Device, pciProps.Address.Function)
			deviceIdentifier = pciBDF
		}

		devices = append(devices, device)
		deviceIdentifiers = append(deviceIdentifiers, deviceIdentifier)

		modelName := ""
		if props, err := device.GetProperties(); err == nil {
			modelName = props.ModelName.String()
		}
		log.Printf("Registered for events on driver %d device %d: [%s] (%s)", idx, i, pciBDF, modelName)
	}

	if len(devices) == 0 {
		log.Printf("Error: driver %d: no devices registered for events", idx)
		return
	}

	for {
		numEvents, events, err := driver.EventListenEx(pollTimeout, devices)
		if err != nil {
			log.Printf("Error: driver %d: EventListenEx: %v", idx, err)
			time.Sleep(pollRetryDelay)
			continue
		}
		if numEvents == 0 {
			continue
		}
		for j, flags := range events {
			if flags != 0 {
				fmt.Printf("[%s] %s\n", deviceIdentifiers[j], flags)
			}
		}
	}
}
