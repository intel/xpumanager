// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package exp

import (
	"github.com/intel/level-zero-go/sysman"
)

// Device provides access to the experimental (Exp) device level Sysman API.
// The stable device API is available through the embedded sysman.Device.
type Device struct {
	*sysman.Device
	// handle is the Level Zero handle of the embedded sysman.Device, re-typed
	// for this package (cgo types are package-specific).
	handle deviceHandle
}

// NewDevice returns a Device wrapping a sysman.Device instance, giving
// access to the experimental (Exp) device level Sysman API functionality.
// Returns nil if sysDevice is nil.
func NewDevice(sysDevice *sysman.Device) *Device {
	if sysDevice == nil {
		return nil
	}
	return &Device{Device: sysDevice, handle: deviceHandle(sysDevice.Handle())}
}

// Ras provides access to the experimental (Exp) RAS Sysman API functionality.
// The stable RAS API is available through the embedded sysman.Ras.
type Ras struct {
	*sysman.Ras
	// handle is the Level Zero handle of the embedded sysman.Ras, re-typed for
	// this package (cgo types are package-specific).
	handle rasHandle
}

// NewRas returns a Ras wrapping a sysman.Ras instance, giving
// access to the experimental (Exp) RAS Sysman API functionality.
// Returns nil if sysRas is nil.
func NewRas(sysRas *sysman.Ras) *Ras {
	if sysRas == nil {
		return nil
	}
	return &Ras{Ras: sysRas, handle: rasHandle(sysRas.Handle())}
}

// VirtualFunction is a VF (virtual function) enabled on the (PF) device.
// There is no stable-API counterpart to embed: VF management is exclusively
// an experimental API.
type VirtualFunction struct {
	handle vfHandle
	device *Device
}

// Device returns the (PF) device the VF is enabled on.
func (w *VirtualFunction) Device() *Device {
	return w.device
}
