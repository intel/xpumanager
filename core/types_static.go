// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package core

import (
	"github.com/google/uuid"
)

/* Types for the higher level golang API */
type DeviceProperties struct {
	Type                     uint32
	VendorId                 uint32
	DeviceId                 uint32
	Flags                    uint32
	SubdeviceId              uint32
	CoreClockRate            uint32
	MaxMemAllocSize          uint64
	MaxHardwareContexts      uint32
	MaxCommandQueuePriority  uint32
	NumThreadsPerEU          uint32
	PhysicalEUSimdWidth      uint32
	NumEUsPerSubslice        uint32
	NumSubslicesPerSlice     uint32
	NumSlices                uint32
	TimerResolution          uint64
	TimestampValidBits       uint32
	KernelTimestampValidBits uint32
	Uuid                     uuid.UUID
	Name                     string
}
