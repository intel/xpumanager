// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package sysman

/*
#include "ze_api.h"
#include "zes_api.h"
#include <stdlib.h>
#include "cgo_helpers.h"
*/
import "C"
import (
	"unsafe"

	"github.com/intel/level-zero-go/internal"
)

// Wrappers for handles
//
// This section defines wrapper types for the Sysman API handles, plus generic
// helpers for converting between handles and the wrapper types

// zesHandle is the set of Level Zero handle types that the higher level wrapper types are built on.
type zesHandle interface {
	driverHandle | deviceHandle | schedHandle | perfHandle | pwrHandle |
		freqHandle | engineHandle | standbyHandle | firmwareHandle | memHandle |
		fabricPortHandle | tempHandle | psuHandle | fanHandle | ledHandle |
		rasHandle | diagHandle | overclockHandle
}

// handleWrapper is embedded in the wrapper types to hold the Level Zero handle of the wrapped component.
type handleWrapper[H zesHandle] struct {
	handle H
}

// Handle returns the underlying Level Zero handle of the component
// (zes_engine_handle_t for Engine, zes_mem_handle_t for Memory etc).
//
// It exists for implementing bindings for experimental or vendor extension
// APIs that this package does not cover (see the sysman/exp packages). Not
// meant for general use.
func (w *handleWrapper[H]) Handle() unsafe.Pointer {
	return unsafe.Pointer(w.handle)
}

func (w *handleWrapper[H]) setHandle(h H) {
	w.handle = h
}

func (w *handleWrapper[H]) getHandle() H {
	return w.handle
}

// deviceRef is embedded in the wrapper types of the components that are enumerated from a device.
type deviceRef struct {
	device *Device
}

// Device returns the device that the component was enumerated from. See Handle for the intended use.
func (w *deviceRef) Device() *Device {
	return w.device
}

func (w *deviceRef) setDevice(d *Device) {
	w.device = d
}

// handlesToWrappers converts a slice of L0 handles to higher-level wrapper types.
func handlesToWrappers[V any, H zesHandle, W interface {
	*V
	setHandle(H)
	setDevice(*Device)
}](handles []H, device *Device) []W {
	wrappers := make([]W, len(handles))
	for i, handle := range handles {
		v := new(V)
		w := W(v)
		w.setHandle(handle)
		w.setDevice(device)
		wrappers[i] = w
	}
	return wrappers
}

// wrappersToHandles converts a slice of higher-level wrapper types back into their underlying L0 handles.
func wrappersToHandles[V any, H zesHandle, W interface {
	*V
	getHandle() H
}](wrappers []W) []H {
	handles := make([]H, len(wrappers))
	for i, wrapper := range wrappers {
		handles[i] = wrapper.getHandle()
	}
	return handles
}

// Driver provides access to Sysman API driver functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#driver-functions
type Driver struct {
	handleWrapper[driverHandle]
	extensions map[string]bool
}

// HasExtension reports whether the driver advertises the given extension.
func (w *Driver) HasExtension(name string) bool {
	return w.extensions[name]
}

// Device provides access to Sysman API device functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#device-functions
type Device struct {
	handleWrapper[deviceHandle]
	extensions map[string]bool
}

// HasExtension reports whether the driver of this device advertises the given
// extension.
func (w *Device) HasExtension(name string) bool {
	return w.extensions[name]
}

// Overclock provides access to Sysman API overclock functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#overclock-functions
type Overclock struct {
	handleWrapper[overclockHandle]
	deviceRef
}

// Diagnostics provides access to Sysman API diagnostics functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#diagnostics-functions
type Diagnostics struct {
	handleWrapper[diagHandle]
	deviceRef
}

// Engine provides access to Sysman API engine functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#engine-functions
type Engine struct {
	handleWrapper[engineHandle]
	deviceRef
}

// FabricPort provides access to Sysman API fabric functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#fabric-functions
type FabricPort struct {
	handleWrapper[fabricPortHandle]
	deviceRef
}

// Fan provides access to Sysman API fan functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#fan-functions
type Fan struct {
	handleWrapper[fanHandle]
	deviceRef
}

// Firmware provides access to Sysman API firmware functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#firmware-functions
type Firmware struct {
	handleWrapper[firmwareHandle]
	deviceRef
}

// Frequency provides access to Sysman API frequency functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#frequency-functions
type Frequency struct {
	handleWrapper[freqHandle]
	deviceRef
}

// Led provides access to Sysman API LED functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#led-functions
type Led struct {
	handleWrapper[ledHandle]
	deviceRef
}

// Memory provides access to Sysman API memory functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#memory-functions
type Memory struct {
	handleWrapper[memHandle]
	deviceRef
}

// Performance provides access to Sysman API performance functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#performance-functions
type Performance struct {
	handleWrapper[perfHandle]
	deviceRef
}

// Power provides access to Sysman API power functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#power-functions
type Power struct {
	handleWrapper[pwrHandle]
	deviceRef
}

// Psu provides access to Sysman API psu (power supply) functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#psu-functions
type Psu struct {
	handleWrapper[psuHandle]
	deviceRef
}

// Ras provides access to Sysman API RAS functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#ras-functions
type Ras struct {
	handleWrapper[rasHandle]
	deviceRef
}

// Scheduler provides access to Sysman API scheduler functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#scheduler-functions
type Scheduler struct {
	handleWrapper[schedHandle]
	deviceRef
}

// Standby provides access to Sysman API standby functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#standby-functions
type Standby struct {
	handleWrapper[standbyHandle]
	deviceRef
}

// Temperature provides access to Sysman API temperature functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#temperature-functions
type Temperature struct {
	handleWrapper[tempHandle]
	deviceRef
}

// Types for the higher level golang API
//
// This section defines wrapper and convenience types used by the higher level Golang API.

// Overclock domains. Alias bitmap to wrap multiple domain types in a single value.
type OverclockDomains OverclockDomain

// Overclock controls. Alias bitmap to wrap multiple control types in a single value.
type OverclockControls OverclockControl

// OverclockState wraps the overclocking state from zesDeviceReadOverclockState.
type OverclockState struct {
	Mode          OverclockMode
	WaiverSetting bool
	State         bool
	PendingAction PendingAction
	PendingReset  bool
}

// PowerUsage wraps the power readings from zesPowerGetUsage.
type PowerUsage struct {
	// InstantPower is the instantaneous power consumption in milliwatts.
	InstantPower uint32
	// AveragePower is the average power consumption in milliwatts.
	AveragePower uint32
}

// ExtendedDeviceProperties wraps the device property structures from the Sysman API.
type DeviceProperties struct {
	DeviceBaseProperties
	DeviceExtProperties
	// OemSerialId provides the OEM serial ID of the device, if available via
	// the OEM serial ID extension. Value is empty if not available.
	OemSerialId string
}

// DeviceState wraps the device state structures from the Sysman API.
type DeviceState struct {
	DeviceBaseState
	// ExtendedState provides optional additional state related to the device
	// state extension. Value is nil if not available.
	ExtendedState *DeviceExtState
}

// EccProperties wraps the device ECC property structures from the Sysman API.
type EccProperties struct {
	DeviceEccProperties
	// ExtendedProperties provides optional additional properties that may not
	// be supported on all devices or drivers. Value is nil if not available.
	ExtendedProperties *DeviceEccDefaultPropertiesExt
}

// EngineProperties wraps the engine property structures from the Sysman API.
type EngineProperties struct {
	EngineBaseProperties
	// ExtendedProperties provides optional additional properties that may not
	// be supported on all devices or drivers. Value is nil if not available.
	ExtendedProperties *EngineExtProperties
}

// PciProperties wraps the PCI property structures from the Sysman API.
type PciProperties struct {
	PciBaseProperties
	// LinkSpeedDowngrade provides optional additional properties related to
	// PCIe link speed downgrade extension. Value is nil if not available.
	LinkSpeedDowngrade *PciLinkSpeedDowngradeExtProperties
}

// PciState wraps the PCI state structures from the Sysman API.
type PciState struct {
	PciBaseState
	// LinkSpeedDowngrade provides optional additional state related to
	// PCIe link speed downgrade extension. Value is nil if not available.
	LinkSpeedDowngrade *PciLinkSpeedDowngradeExtState
}

// PowerProperties wraps the power property structures from the Sysman API.
type PowerProperties struct {
	PowerBaseProperties
	// ExtendedProperties provides optional additional properties that may not
	// be supported on all devices or drivers. Value is nil if not available.
	ExtendedProperties *PowerExtProperties
}

// String representation of all set bits of OverclockDomains.
func (o OverclockDomains) String() string {
	return internal.FlagsToString[OverclockDomain](OverclockDomain(o))
}

// Bits returns a slice of all enabled bits of OverclockDomain.
func (f OverclockDomains) Bits() []OverclockDomain {
	return internal.FlagsToBits(OverclockDomain(f))
}

// String representation of all set bits of OverclockControls.
func (o OverclockControls) String() string {
	return internal.FlagsToString[OverclockControl](OverclockControl(o))
}

// Bits returns a slice of all enabled bits of OverclockControl.
func (f OverclockControls) Bits() []OverclockControl {
	return internal.FlagsToBits(OverclockControl(f))
}

// Extra methods for auto-generated types
//
// This section defines extra methods for the auto-generated types (types.go).

// String representation of all set bits of InitFlags.
func (f InitFlags) String() string {
	return internal.FlagsToString(InitFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of InitFlags.
func (f InitFlags) Bits() []InitFlag {
	return internal.FlagsToBits(InitFlag(f))
}

// String representation of all set bits of EngineTypeFlags.
func (f EngineTypeFlags) String() string {
	return internal.FlagsToString(EngineTypeFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of EngineTypeFlags.
func (f EngineTypeFlags) Bits() []EngineTypeFlag {
	return internal.FlagsToBits(EngineTypeFlag(f))
}

// String representation of all set bits of ResetReasonFlags.
func (f ResetReasonFlags) String() string {
	return internal.FlagsToString(ResetReasonFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of ResetReasonFlags.
func (f ResetReasonFlags) Bits() []ResetReasonFlag {
	return internal.FlagsToBits(ResetReasonFlag(f))
}

// String representation of all set bits of DevicePropertyFlags.
func (f DevicePropertyFlags) String() string {
	return internal.FlagsToString(DevicePropertyFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of DevicePropertyFlags.
func (f DevicePropertyFlags) Bits() []DevicePropertyFlag {
	return internal.FlagsToBits(DevicePropertyFlag(f))
}

// String representation of all set bits of DeviceStateExtFlags.
func (f DeviceStateExtFlags) String() string {
	return internal.FlagsToString(DeviceStateExtFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of DeviceStateExtFlags.
func (f DeviceStateExtFlags) Bits() []DeviceStateExtFlag {
	return internal.FlagsToBits(DeviceStateExtFlag(f))
}

// String representation of all set bits of PciLinkQualIssueFlags.
func (f PciLinkQualIssueFlags) String() string {
	return internal.FlagsToString(PciLinkQualIssueFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of PciLinkQualIssueFlags.
func (f PciLinkQualIssueFlags) Bits() []PciLinkQualIssueFlag {
	return internal.FlagsToBits(PciLinkQualIssueFlag(f))
}

// String representation of all set bits of PciLinkStabIssueFlags.
func (f PciLinkStabIssueFlags) String() string {
	return internal.FlagsToString(PciLinkStabIssueFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of PciLinkStabIssueFlags.
func (f PciLinkStabIssueFlags) Bits() []PciLinkStabIssueFlag {
	return internal.FlagsToBits(PciLinkStabIssueFlag(f))
}

// String representation of all set bits of EventTypeFlags.
func (f EventTypeFlags) String() string {
	return internal.FlagsToString(EventTypeFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of EventTypeFlags.
func (f EventTypeFlags) Bits() []EventTypeFlag {
	return internal.FlagsToBits(EventTypeFlag(f))
}

// String representation of all set bits of FabricPortQualIssueFlags.
func (f FabricPortQualIssueFlags) String() string {
	return internal.FlagsToString(FabricPortQualIssueFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of FabricPortQualIssueFlags.
func (f FabricPortQualIssueFlags) Bits() []FabricPortQualIssueFlag {
	return internal.FlagsToBits(FabricPortQualIssueFlag(f))
}

// String representation of all set bits of FabricPortFailureFlags.
func (f FabricPortFailureFlags) String() string {
	return internal.FlagsToString(FabricPortFailureFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of FabricPortFailureFlags.
func (f FabricPortFailureFlags) Bits() []FabricPortFailureFlag {
	return internal.FlagsToBits(FabricPortFailureFlag(f))
}

// String representation of all set bits of FreqThrottleReasonFlags.
func (f FreqThrottleReasonFlags) String() string {
	return internal.FlagsToString(FreqThrottleReasonFlag(f))
}

// Bits returns a slice of all enabled flags (set bits) of FreqThrottleReasonFlags.
func (f FreqThrottleReasonFlags) Bits() []FreqThrottleReasonFlag {
	return internal.FlagsToBits(FreqThrottleReasonFlag(f))
}
