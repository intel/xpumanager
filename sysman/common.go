//
// Copyright (C) 2025 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"strconv"
)

func subDeviceIdString(onSubdevice uint8, subdeviceId uint32) string {
	if onSubdevice != 0 {
		return strconv.Itoa(int(subdeviceId))
	}
	return ""
}
