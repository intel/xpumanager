//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysman

import (
	"math"
	"strconv"
	"strings"
)

func subDeviceIdString(onSubdevice uint8, subdeviceId uint32) string {
	if onSubdevice != 0 {
		return strconv.Itoa(int(subdeviceId))
	}
	return ""
}

// hwTypeString returns (Sysman define) name converted to a format suitable as
// hw.<type> attribute value (lower-cased, with underscores replaced by dashes)
func hwTypeString(name string) string {
	return strings.ToLower(strings.ReplaceAll(name, "_", "-"))
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
