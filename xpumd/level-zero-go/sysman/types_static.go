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
import "github.com/intel/level-zero-go/internal"

// Wrappers for handles
//
// This section defines wrapper types for the Sysman API handles

// Driver provides access to Sysman API driver functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#driver-functions
type Driver struct {
	handle     driverHandle
	extensions map[string]bool
}

func (w *Driver) setHandle(h driverHandle) {
	w.handle = h
}

// Device provides access to Sysman API device functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#device-functions
type Device struct {
	handle     deviceHandle
	extensions map[string]bool
}

func (w *Device) setHandle(h deviceHandle) {
	w.handle = h
}

func (w *Device) getHandle() deviceHandle {
	return w.handle
}

// Overclock provides access to Sysman API overclock functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#overclock-functions
type Overclock struct {
	handle overclockHandle
}

func (w *Overclock) setHandle(h overclockHandle) {
	w.handle = h
}

// Diagnostics provides access to Sysman API diagnostics functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#diagnostics-functions
type Diagnostics struct {
	handle diagHandle
}

func (w *Diagnostics) setHandle(h diagHandle) {
	w.handle = h
}

// Engine provides access to Sysman API engine functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#engine-functions
type Engine struct {
	handle engineHandle
	device *Device
}

func (w *Engine) setHandle(h engineHandle) {
	w.handle = h
}

func (w *Engine) setDevice(d *Device) {
	w.device = d
}

// FabricPort provides access to Sysman API fabric functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#fabric-functions
type FabricPort struct {
	handle fabricPortHandle
}

func (w *FabricPort) setHandle(h fabricPortHandle) {
	w.handle = h
}

func (w *FabricPort) getHandle() fabricPortHandle {
	return w.handle
}

// Fan provides access to Sysman API fan functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#fan-functions
type Fan struct {
	handle fanHandle
}

func (w *Fan) setHandle(h fanHandle) {
	w.handle = h
}

// Firmware provides access to Sysman API firmware functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#firmware-functions
type Firmware struct {
	handle firmwareHandle
}

func (w *Firmware) setHandle(h firmwareHandle) {
	w.handle = h
}

// Frequency provides access to Sysman API frequency functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#frequency-functions
type Frequency struct {
	handle freqHandle
}

func (w *Frequency) setHandle(h freqHandle) {
	w.handle = h
}

// Led provides access to Sysman API LED functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#led-functions
type Led struct {
	handle ledHandle
}

func (w *Led) setHandle(h ledHandle) {
	w.handle = h
}

// Memory provides access to Sysman API memory functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#memory-functions
type Memory struct {
	handle memHandle
}

func (w *Memory) setHandle(h memHandle) {
	w.handle = h
}

// Performance provides access to Sysman API performance functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#performance-functions
type Performance struct {
	handle perfHandle
}

func (w *Performance) setHandle(h perfHandle) {
	w.handle = h
}

// Power provides access to Sysman API power functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#power-functions
type Power struct {
	handle pwrHandle
	device *Device
}

func (w *Power) setHandle(h pwrHandle) {
	w.handle = h
}

func (w *Power) setDevice(d *Device) {
	w.device = d
}

// Psu provides access to Sysman API psu (power supply) functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#psu-functions
type Psu struct {
	handle psuHandle
}

func (w *Psu) setHandle(h psuHandle) {
	w.handle = h
}

// Ras provides access to Sysman API RAS functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#ras-functions
type Ras struct {
	handle rasHandle
	device *Device
}

func (w *Ras) setHandle(h rasHandle) {
	w.handle = h
}

func (w *Ras) setDevice(d *Device) {
	w.device = d
}

// Scheduler provides access to Sysman API scheduler functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#scheduler-functions
type Scheduler struct {
	handle schedHandle
}

func (w *Scheduler) setHandle(h schedHandle) {
	w.handle = h
}

// Standby provides access to Sysman API standby functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#standby-functions
type Standby struct {
	handle standbyHandle
}

func (w *Standby) setHandle(h standbyHandle) {
	w.handle = h
}

// Temperature provides access to Sysman API temperature functions:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#temperature-functions
type Temperature struct {
	handle tempHandle
}

func (w *Temperature) setHandle(h tempHandle) {
	w.handle = h
}

// Generics for handle wrappers
//
// This section defines generic functions to convert between Sysman handles and the Golang wrapper types.

func handlesToWrappers[H any, V any, W interface {
	*V
	setHandle(H)
}](handles []H) []W {
	wrappers := make([]W, len(handles))
	for i, handle := range handles {
		v := new(V)
		w := W(v)
		w.setHandle(handle)
		wrappers[i] = w
	}
	return wrappers
}

func handlesToWrappersWithDevice[H any, V any, W interface {
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

func wrappersToHandles[H any, V any, W interface {
	*V
	getHandle() H
}](wrappers []W) []H {
	handles := make([]H, len(wrappers))
	for i, wrapper := range wrappers {
		handles[i] = wrapper.getHandle()
	}
	return handles
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

// ExtendedDeviceProperties wraps the device property structures from the Sysman API.
type DeviceProperties struct {
	DeviceBaseProperties
	DeviceExtProperties
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
