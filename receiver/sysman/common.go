//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"math"
	"strconv"
)

func subDeviceIdString(onSubdevice uint8, subdeviceId uint32) string {
	if onSubdevice != 0 {
		return strconv.Itoa(int(subdeviceId))
	}
	return ""
}

func u64CounterDiff(oldVal, newVal uint64) uint64 {
	if oldVal <= newVal {
		return newVal - oldVal
	}
	// it's a wrap!
	return (math.MaxUint64 - oldVal) + newVal + 1
}
