//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"fmt"
	"math"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/intel/level-zero-go/core"
	l0sysman "github.com/intel/level-zero-go/sysman"
)

const sysfsPciDevicesPath = "/sys/bus/pci/devices"

func subDeviceIdString(onSubdevice core.Bool, subdeviceId uint32) string {
	if onSubdevice.IsTrue() {
		return strconv.Itoa(int(subdeviceId))
	}
	return ""
}

// hwTypeString returns (Sysman define) name converted to a format suitable as
// hw.<type> attribute value (lower-cased, with underscores replaced by dashes)
func hwTypeString(name string) string {
	return strings.ToLower(strings.ReplaceAll(name, "_", "-"))
}

// pciBDF returns the PCI address as a domain:bus:device.function string
func pciBDF(addr l0sysman.PciAddress) string {
	return fmt.Sprintf("%04x:%02x:%02x.%x", addr.Domain, addr.Bus, addr.Device, addr.Function)
}

// isPciVF tells if a device is an SR-IOV virtual function.
func isPciVF(bdf string) bool {
	_, err := os.Lstat(filepath.Join(sysfsPciDevicesPath, bdf, "physfn"))
	return err == nil
}

// u64CounterDiff returns diff from old to new value, assuming that if
// new<old, counter has wrapped at math.MaxUint64
func u64CounterDiff(oldVal, newVal uint64) uint64 {
	if oldVal <= newVal {
		return newVal - oldVal
	}
	// it's a wrap!
	return (math.MaxUint64 - oldVal) + newVal + 1
}
