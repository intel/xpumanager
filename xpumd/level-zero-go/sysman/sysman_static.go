// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

//go:generate ../hack/generate-stringer.sh

package sysman

import (
	"math"
	"runtime"
	"time"
	"unsafe"

	"github.com/intel/level-zero-go/core"
)

func boolToByte(b bool) byte {
	if b {
		return 1
	}
	return 0
}

// Init wraps the zesInit function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesinit
func Init(flags InitFlags) error {
	ret := zesInit(flags)
	return ret.ToError()
}

// DriverGet wraps the zesDriverGet function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdriverget
func DriverGet() ([]*Driver, error) {
	count := uint32(0)
	if ret := zesDriverGet(&count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]driverHandle, count)
	ret := zesDriverGet(&count, handles)
	if ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}

	drivers := handlesToWrappers[driverHandle, Driver](handles)

	// Populate extensions for each driver
	for _, driver := range drivers {
		extProps, ret := driver.GetExtensionProperties()
		if ret != nil {
			// TODO: should we somehow communicate if error != RESULT_ERROR_UNSUPPORTED_FEATURE?
			// For now just ignore and assume no extensions
			continue
		}

		driver.extensions = make(map[string]bool, len(extProps))
		for _, ext := range extProps {
			driver.extensions[ext.Name.String()] = true
		}
	}

	return drivers, nil
}

// GetExtensionProperties wraps the zesDriverGetExtensionProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdrivergetextensionproperties
func (z *Driver) GetExtensionProperties() ([]DriverExtensionProperties, error) {
	count := uint32(0)
	if ret := zesDriverGetExtensionProperties(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	extensions := make([]DriverExtensionProperties, count)
	ret := zesDriverGetExtensionProperties(z.handle, &count, extensions)
	return extensions, ret.ToError()
}

// DeviceGet wraps the zesDeviceGet function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceget
func (z *Driver) DeviceGet() ([]*Device, error) {
	count := uint32(0)
	if ret := zesDeviceGet(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]deviceHandle, count)
	ret := zesDeviceGet(z.handle, &count, handles)
	if ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}

	devices := handlesToWrappers[deviceHandle, Device](handles)

	// Pass driver extensions to devices
	for _, device := range devices {
		device.extensions = z.extensions
	}

	return devices, nil
}

// EventListen wraps the zesDriverEventListen function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdrivereventlisten
func (z *Driver) EventListen(timeout time.Duration, devices []*Device) (uint32, []EventTypeFlags, error) {
	handles := wrappersToHandles[deviceHandle, Device](devices)
	var numEvents uint32
	events := make([]EventTypeFlags, len(handles))
	ms := durationToMillisecondsUint32(timeout)
	ret := zesDriverEventListen(z.handle, ms, uint32(len(handles)), handles, &numEvents, events)
	return numEvents, events, ret.ToError()
}

// EventListenEx wraps the zesDriverEventListenEx function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdrivereventlistenex
func (z *Driver) EventListenEx(timeout time.Duration, devices []*Device) (uint32, []EventTypeFlags, error) {
	handles := wrappersToHandles[deviceHandle, Device](devices)
	var numEvents uint32
	events := make([]EventTypeFlags, len(handles))
	ms := durationToMillisecondsUint64(timeout)
	ret := zesDriverEventListenEx(z.handle, ms, uint32(len(handles)), handles, &numEvents, events)
	return numEvents, events, ret.ToError()
}

// GetProperties wraps the zesDeviceGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegetproperties
func (z *Device) GetProperties() (DeviceProperties, error) {
	props := DeviceProperties{
		DeviceExtProperties: DeviceExtProperties{
			stype: _STRUCTURE_TYPE_DEVICE_EXT_PROPERTIES,
		},
	}

	var pinner runtime.Pinner
	pinner.Pin(&props.DeviceExtProperties)
	defer pinner.Unpin()

	props.DeviceBaseProperties.pnext = unsafe.Pointer(&props.DeviceExtProperties)

	ret := zesDeviceGetProperties(z.handle, &props.DeviceBaseProperties)

	return props, ret.ToError()
}

// GetState wraps the zesDeviceGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegetstate
func (z *Device) GetState() (DeviceState, error) {
	var state DeviceState
	ret := zesDeviceGetState(z.handle, &state)
	return state, ret.ToError()
}

// Reset wraps the zesDeviceReset function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicereset
func (z *Device) Reset(force bool) error {
	ret := zesDeviceReset(z.handle, boolToByte(force))
	return ret.ToError()
}

// ResetExt wraps the zesDeviceResetExt function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceresetext
func (z *Device) ResetExt(properties *ResetProperties) error {
	ret := zesDeviceResetExt(z.handle, properties)
	return ret.ToError()
}

// ProcessesGetState wraps the zesDeviceProcessesGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceprocessesgetstate
func (z *Device) ProcessesGetState() ([]ProcessState, error) {
	count := uint32(0)
	if ret := zesDeviceProcessesGetState(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	processes := make([]ProcessState, count)
	ret := zesDeviceProcessesGetState(z.handle, &count, processes)
	return processes, ret.ToError()
}

// EventRegister wraps the zesDeviceEventRegister function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceeventregister
//
// The method has a built-in fallback to handle cases where the backend driver:
// a) does not recognize some of the event flags (e.g. due to older driver version) and returns ERROR_INVALID_ENUMERATION
// b) does not support some event and returns ERROR_UNSUPPORTED_ENUMERATION
// In this case the method tries to register each flag individually. The method
// returns the successfully registered set of flags.
func (z *Device) EventRegister(flags EventTypeFlags) (EventTypeFlags, error) {
	// Fastpath: try to register all requested flags
	ret := zesDeviceEventRegister(z.handle, flags)
	if ret == core.RESULT_SUCCESS {
		return flags, nil
	}
	if flags <= EventTypeFlags(1) || (ret != core.RESULT_ERROR_INVALID_ENUMERATION && ret != core.RESULT_ERROR_UNSUPPORTED_ENUMERATION) {
		// No flags to retry individually, or an error other than invalid/unsupported enumeration occurred
		return 0, ret.ToError()
	}

	// Slowpath: iteratively register bit-by-bit
	// NOTE: this may leave us in partially registered state, but there's no way to roll back
	// (there's no way to know the original state before any registration attempts)
	var registered EventTypeFlags
	for _, flag := range flags.Bits() {
		f := EventTypeFlags(flag)
		if ret := zesDeviceEventRegister(z.handle, f); ret != core.RESULT_SUCCESS {
			if ret == core.RESULT_ERROR_INVALID_ENUMERATION || ret == core.RESULT_ERROR_UNSUPPORTED_ENUMERATION {
				continue
			}
			// Unexpected (other than invalid/unsupported enumeration) error
			return 0, ret.ToError()
		}
		registered |= f
	}
	ret = zesDeviceEventRegister(z.handle, registered)
	if ret != core.RESULT_SUCCESS {
		return 0, ret.ToError()
	}
	return registered, nil
}

// PciGetProperties wraps the zesDevicePciGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetproperties
func (z *Device) PciGetProperties() (PciProperties, error) {
	var props PciProperties

	if z.extensions[PCI_LINK_SPEED_DOWNGRADE_EXT_NAME] {
		props.LinkSpeedDowngrade = &PciLinkSpeedDowngradeExtProperties{
			stype: _STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_PROPERTIES,
		}

		pinner := runtime.Pinner{}
		pinner.Pin(props.LinkSpeedDowngrade)
		defer pinner.Unpin()

		//nolint:staticcheck // could remove embedded field from selector
		props.PciBaseProperties.pnext = unsafe.Pointer(props.LinkSpeedDowngrade)
	}

	ret := zesDevicePciGetProperties(z.handle, &props.PciBaseProperties)
	return props, ret.ToError()
}

// PciGetState wraps the zesDevicePciGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetstate
func (z *Device) PciGetState() (PciState, error) {
	var state PciState

	if z.extensions[PCI_LINK_SPEED_DOWNGRADE_EXT_NAME] {
		state.LinkSpeedDowngrade = &PciLinkSpeedDowngradeExtState{
			stype: _STRUCTURE_TYPE_PCI_LINK_SPEED_DOWNGRADE_EXT_STATE,
		}

		pinner := runtime.Pinner{}
		pinner.Pin(state.LinkSpeedDowngrade)
		defer pinner.Unpin()

		//nolint:staticcheck // could remove embedded field from selector
		state.PciBaseState.pnext = unsafe.Pointer(state.LinkSpeedDowngrade)
	}
	ret := zesDevicePciGetState(z.handle, &state.PciBaseState)
	return state, ret.ToError()
}

// PciGetBars wraps the zesDevicePciGetBars function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetbars
func (z *Device) PciGetBars() ([]PciBarProperties, error) {
	count := uint32(0)
	if ret := zesDevicePciGetBars(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	bars := make([]PciBarProperties, count)
	ret := zesDevicePciGetBars(z.handle, &count, bars)
	return bars, ret.ToError()
}

// PciGetStats wraps the zesDevicePciGetStats function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetstats
func (z *Device) PciGetStats() (PciStats, error) {
	var stats PciStats
	ret := zesDevicePciGetStats(z.handle, &stats)
	return stats, ret.ToError()
}

// PciLinkSpeedUpdateExt wraps the zesDevicePciLinkSpeedUpdateExt function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcilinkspeedupdateext
func (z *Device) PciLinkSpeedUpdateExt(shouldDowngrade bool) (DeviceAction, error) {
	var pendingAction DeviceAction
	ret := zesDevicePciLinkSpeedUpdateExt(z.handle, boolToByte(shouldDowngrade), &pendingAction)
	return pendingAction, ret.ToError()
}

// SetOverclockWaiver wraps the zesDeviceSetOverclockWaiver function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicesetoverclockwaiver
func (z *Device) SetOverclockWaiver() error {
	ret := zesDeviceSetOverclockWaiver(z.handle)
	return ret.ToError()
}

// GetOverclockDomains wraps the zesDeviceGetOverclockDomains function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegetoverclockdomains
func (z *Device) GetOverclockDomains() (OverclockDomains, error) {
	var domains uint32
	ret := zesDeviceGetOverclockDomains(z.handle, &domains)
	return OverclockDomains(domains), ret.ToError()
}

// GetOverclockControls wraps the zesDeviceGetOverclockControls function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegetoverclockcontrols
func (z *Device) GetOverclockControls(domainType OverclockDomain) (OverclockControls, error) {
	var controls uint32
	ret := zesDeviceGetOverclockControls(z.handle, domainType, &controls)
	return OverclockControls(controls), ret.ToError()
}

// ResetOverclockSettings wraps the zesDeviceResetOverclockSettings function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceresetoverclocksettings
func (z *Device) ResetOverclockSettings(onShippedState bool) error {
	ret := zesDeviceResetOverclockSettings(z.handle, boolToByte(onShippedState))
	return ret.ToError()
}

// ReadOverclockState wraps the zesDeviceReadOverclockState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicereadoverclockstate
func (z *Device) ReadOverclockState() (OverclockState, error) {
	var (
		mode          OverclockMode
		waiverSetting byte
		state         byte
		pendingAction PendingAction
		pendingReset  byte
	)
	ret := zesDeviceReadOverclockState(z.handle, &mode, &waiverSetting, &state, &pendingAction, &pendingReset)
	return OverclockState{
		Mode:          mode,
		WaiverSetting: waiverSetting != 0,
		State:         state != 0,
		PendingAction: pendingAction,
		PendingReset:  pendingReset != 0,
	}, ret.ToError()
}

// EccAvailable wraps the zesDeviceEccAvailable function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceeccavailable
func (z *Device) EccAvailable() (bool, error) {
	var available byte
	ret := zesDeviceEccAvailable(z.handle, &available)
	return available != 0, ret.ToError()
}

// EccConfigurable wraps the zesDeviceEccConfigurable function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceeccconfigurable
func (z *Device) EccConfigurable() (bool, error) {
	var configurable byte
	ret := zesDeviceEccConfigurable(z.handle, &configurable)
	return configurable != 0, ret.ToError()
}

// GetEccState wraps the zesDeviceGetEccState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegeteccstate
func (z *Device) GetEccState() (EccProperties, error) {
	props := EccProperties{}

	if z.extensions[DEVICE_ECC_DEFAULT_PROPERTIES_EXT_NAME] {
		props.ExtendedProperties = &DeviceEccDefaultPropertiesExt{
			stype: _STRUCTURE_TYPE_DEVICE_ECC_DEFAULT_PROPERTIES_EXT,
		}
		var pinner runtime.Pinner
		pinner.Pin(props.ExtendedProperties)
		defer pinner.Unpin()

		//nolint:staticcheck // could remove embedded field from selector
		props.DeviceEccProperties.pnext = unsafe.Pointer(props.ExtendedProperties)
	}

	ret := zesDeviceGetEccState(z.handle, &props.DeviceEccProperties)

	return props, ret.ToError()
}

// SetEccState wraps the zesDeviceSetEccState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceseteccstate
func (z *Device) SetEccState(newState DeviceEccDesc) (DeviceEccProperties, error) {
	var state DeviceEccProperties
	ret := zesDeviceSetEccState(z.handle, &newState, &state)
	return state, ret.ToError()
}

// EnumOverclockDomains wraps the zesDeviceEnumOverclockDomains function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumoverclockdomains
func (z *Device) EnumOverclockDomains() ([]*Overclock, error) {
	count := uint32(0)
	if ret := zesDeviceEnumOverclockDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]overclockHandle, count)
	ret := zesDeviceEnumOverclockDomains(z.handle, &count, handles)
	return handlesToWrappers[overclockHandle, Overclock](handles), ret.ToError()
}

// FabricPortGetMultiPortThroughput wraps the zesFabricPortGetMultiPortThroughput function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetmultiportthroughput
func (z *Device) FabricPortGetMultiPortThroughput(ports []*FabricPort) ([]FabricPortThroughput, error) {
	handles := wrappersToHandles[fabricPortHandle, FabricPort](ports)
	count := uint32(len(handles))
	throughputs := make([]*FabricPortThroughput, count)
	for i := range throughputs {
		throughputs[i] = &FabricPortThroughput{}
	}
	ret := zesFabricPortGetMultiPortThroughput(z.handle, count, handles, throughputs)
	retThroughput := make([]FabricPortThroughput, count)
	for i, t := range throughputs {
		retThroughput[i] = *t
	}
	return retThroughput, ret.ToError()
}

// GetDomainProperties wraps the zesOverclockGetDomainProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetdomainproperties
func (z *Overclock) GetDomainProperties() (OverclockProperties, error) {
	var props OverclockProperties
	ret := zesOverclockGetDomainProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetDomainVFProperties wraps the zesOverclockGetDomainVFProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetdomainvfproperties
func (z *Overclock) GetDomainVFProperties() (VfProperty, error) {
	var props VfProperty
	ret := zesOverclockGetDomainVFProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetDomainControlProperties wraps the zesOverclockGetDomainControlProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetdomaincontrolproperties
func (z *Overclock) GetDomainControlProperties(domainControl OverclockControl) (ControlProperty, error) {
	var props ControlProperty
	ret := zesOverclockGetDomainControlProperties(z.handle, domainControl, &props)
	return props, ret.ToError()
}

// GetControlCurrentValue wraps the zesOverclockGetControlCurrentValue function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetcontrolcurrentvalue
func (z *Overclock) GetControlCurrentValue(domainControl OverclockControl) (float64, error) {
	var value float64
	ret := zesOverclockGetControlCurrentValue(z.handle, domainControl, &value)
	return value, ret.ToError()
}

// GetControlPendingValue wraps the zesOverclockGetControlPendingValue function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetcontrolpendingvalue
func (z *Overclock) GetControlPendingValue(domainControl OverclockControl) (float64, error) {
	var value float64
	ret := zesOverclockGetControlPendingValue(z.handle, domainControl, &value)
	return value, ret.ToError()
}

// SetControlUserValue wraps the zesOverclockSetControlUserValue function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclocksetcontroluservalue
func (z *Overclock) SetControlUserValue(domainControl OverclockControl, value float64) (PendingAction, error) {
	var pendingAction PendingAction
	ret := zesOverclockSetControlUserValue(z.handle, domainControl, value, &pendingAction)
	return pendingAction, ret.ToError()
}

// GetControlState wraps the zesOverclockGetControlState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetcontrolstate
func (z *Overclock) GetControlState(domainControl OverclockControl) (ControlState, PendingAction, error) {
	var (
		state         ControlState
		pendingAction PendingAction
	)
	ret := zesOverclockGetControlState(z.handle, domainControl, &state, &pendingAction)
	return state, pendingAction, ret.ToError()
}

// GetVFPointValues wraps the zesOverclockGetVFPointValues function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetvfpointvalues
func (z *Overclock) GetVFPointValues(vfType VfType, arrayType VfArrayType, pointIndex uint32) (uint32, error) {
	var value uint32
	ret := zesOverclockGetVFPointValues(z.handle, vfType, arrayType, pointIndex, &value)
	return value, ret.ToError()
}

// SetVFPointValues wraps the zesOverclockSetVFPointValues function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclocksetvfpointvalues
func (z *Overclock) SetVFPointValues(vfType VfType, pointIndex uint32, value uint32) error {
	ret := zesOverclockSetVFPointValues(z.handle, vfType, pointIndex, value)
	return ret.ToError()
}

// EnumDiagnosticTestSuites wraps the zesDeviceEnumDiagnosticTestSuites function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumdiagnostictestsuites
func (z *Device) EnumDiagnosticTestSuites() ([]*Diagnostics, error) {
	count := uint32(0)
	if ret := zesDeviceEnumDiagnosticTestSuites(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]diagHandle, count)
	ret := zesDeviceEnumDiagnosticTestSuites(z.handle, &count, handles)
	return handlesToWrappers[diagHandle, Diagnostics](handles), ret.ToError()
}

// GetProperties wraps the zesDiagnosticsGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdiagnosticsgetproperties
func (z *Diagnostics) GetProperties() (DiagProperties, error) {
	var props DiagProperties
	ret := zesDiagnosticsGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetTests wraps the zesDiagnosticsGetTests function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdiagnosticsgettests
// TODO: static type for DiagTest that contains golang strings
func (z *Diagnostics) GetTests() ([]DiagTest, error) {
	count := uint32(0)
	if ret := zesDiagnosticsGetTests(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	tests := make([]DiagTest, count)
	ret := zesDiagnosticsGetTests(z.handle, &count, tests)
	return tests, ret.ToError()
}

// RunTests wraps the zesDiagnosticsRunTests function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdiagnosticsruntests
func (z *Diagnostics) RunTests(startIdx, endIdx uint32) ([]DiagResult, error) {
	count := endIdx - startIdx + 1
	results := make([]DiagResult, count)
	ret := zesDiagnosticsRunTests(z.handle, startIdx, endIdx, results)
	return results, ret.ToError()

}

// EnumEngineGroups wraps the zesDeviceEnumEngineGroups function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumenginegroups
func (z *Device) EnumEngineGroups() ([]*Engine, error) {
	count := uint32(0)
	if ret := zesDeviceEnumEngineGroups(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]engineHandle, count)
	ret := zesDeviceEnumEngineGroups(z.handle, &count, handles)
	engines := handlesToWrappersWithDevice[engineHandle, Engine](handles, z)
	return engines, ret.ToError()
}

// GetProperties wraps the zesEngineGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesenginegetproperties
func (z *Engine) GetProperties() (EngineProperties, error) {
	props := EngineProperties{}

	if z.device.extensions[ENGINE_ACTIVITY_EXT_NAME] {
		props.ExtendedProperties = &EngineExtProperties{
			stype: _STRUCTURE_TYPE_ENGINE_EXT_PROPERTIES,
		}
		pinner := runtime.Pinner{}
		pinner.Pin(props.ExtendedProperties)
		defer pinner.Unpin()

		//nolint:staticcheck // could remove embedded field from selector
		props.EngineBaseProperties.pnext = unsafe.Pointer(props.ExtendedProperties)
	}

	ret := zesEngineGetProperties(z.handle, &props.EngineBaseProperties)

	return props, ret.ToError()
}

// GetActivity wraps the zesEngineGetActivity function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesenginegetactivity
func (z *Engine) GetActivity() (EngineStats, error) {
	var stats EngineStats
	ret := zesEngineGetActivity(z.handle, &stats)
	return stats, ret.ToError()
}

// GetActivityExt wraps the zesEngineGetActivityExt function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesenginegetactivityext
func (z *Engine) GetActivityExt() ([]EngineStats, error) {
	count := uint32(0)
	if ret := zesEngineGetActivityExt(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	stats := make([]EngineStats, count)
	ret := zesEngineGetActivityExt(z.handle, &count, stats)
	return stats, ret.ToError()
}

// EnumFabricPorts wraps the zesDeviceEnumFabricPorts function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumfabricports
func (z *Device) EnumFabricPorts() ([]*FabricPort, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFabricPorts(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]fabricPortHandle, count)
	ret := zesDeviceEnumFabricPorts(z.handle, &count, handles)
	return handlesToWrappers[fabricPortHandle, FabricPort](handles), ret.ToError()
}

// GetProperties wraps the zesFabricPortGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetproperties
func (z *FabricPort) GetProperties() (FabricPortProperties, error) {
	var props FabricPortProperties
	ret := zesFabricPortGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetLinkType wraps the zesFabricPortGetLinkType function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetlinktype
func (z *FabricPort) GetLinkType() (FabricLinkType, error) {
	var linkType FabricLinkType
	ret := zesFabricPortGetLinkType(z.handle, &linkType)
	return linkType, ret.ToError()
}

// GetConfig wraps the zesFabricPortGetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetconfig
func (z *FabricPort) GetConfig() (FabricPortConfig, error) {
	var config FabricPortConfig
	ret := zesFabricPortGetConfig(z.handle, &config)
	return config, ret.ToError()
}

// SetConfig wraps the zesFabricPortSetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportsetconfig
func (z *FabricPort) SetConfig(config *FabricPortConfig) error {
	ret := zesFabricPortSetConfig(z.handle, config)
	return ret.ToError()
}

// GetState wraps the zesFabricPortGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetstate
func (z *FabricPort) GetState() (FabricPortState, error) {
	var state FabricPortState
	ret := zesFabricPortGetState(z.handle, &state)
	return state, ret.ToError()
}

// GetThroughput wraps the zesFabricPortGetThroughputRaw function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetthroughput
func (z *FabricPort) GetThroughput() (FabricPortThroughput, error) {
	var throughput FabricPortThroughput
	ret := zesFabricPortGetThroughput(z.handle, &throughput)
	return throughput, ret.ToError()
}

// GetFabricErrorCounters wraps the zesFabricPortGetFabricErrorCounters function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetfabricerrorcounters
func (z *FabricPort) GetFabricErrorCounters() (FabricPortErrorCounters, error) {
	var counters FabricPortErrorCounters
	ret := zesFabricPortGetFabricErrorCounters(z.handle, &counters)
	return counters, ret.ToError()
}

// EnumFans wraps the zesDeviceEnumFans function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumfans
func (z *Device) EnumFans() ([]*Fan, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFans(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]fanHandle, count)
	ret := zesDeviceEnumFans(z.handle, &count, handles)
	return handlesToWrappers[fanHandle, Fan](handles), ret.ToError()
}

// GetProperties wraps the zesFanGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfangetproperties
func (z *Fan) GetProperties() (FanProperties, error) {
	var props FanProperties
	ret := zesFanGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetConfig wraps the zesFanGetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfangetconfig
func (z *Fan) GetConfig() (FanConfig, error) {
	var config FanConfig
	ret := zesFanGetConfig(z.handle, &config)
	return config, ret.ToError()
}

// SetDefaultMode wraps the zesFanSetDefaultMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfansetdefaultmode
func (z *Fan) SetDefaultMode() error {
	ret := zesFanSetDefaultMode(z.handle)
	return ret.ToError()
}

// SetFixedSpeedMode wraps the zesFanSetFixedSpeedMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfansetfixedspeedmode
func (z *Fan) SetFixedSpeedMode(speed FanSpeed) error {
	ret := zesFanSetFixedSpeedMode(z.handle, &speed)
	return ret.ToError()
}

// SetSpeedTableMode wraps the zesFanSetSpeedTableMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfansetspeedtablemode
func (z *Fan) SetSpeedTableMode(speedTable *FanSpeedTable) error {
	ret := zesFanSetSpeedTableMode(z.handle, speedTable)
	return ret.ToError()
}

// GetState wraps the zesFanGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfangetstate
func (z *Fan) GetState(units FanSpeedUnits) (int32, error) {
	var speed int32
	ret := zesFanGetState(z.handle, units, &speed)
	return speed, ret.ToError()
}

// EnumFirmwares wraps the zesDeviceEnumFirmwares function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumfirmwares
func (z *Device) EnumFirmwares() ([]*Firmware, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFirmwares(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]firmwareHandle, count)
	ret := zesDeviceEnumFirmwares(z.handle, &count, handles)
	return handlesToWrappers[firmwareHandle, Firmware](handles), ret.ToError()
}

// GetProperties wraps the zesFirmwareGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfirmwaregetproperties
func (z *Firmware) GetProperties() (FirmwareProperties, error) {
	var props FirmwareProperties
	ret := zesFirmwareGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// Flash wraps the zesFirmwareFlash function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfirmwareflash
// TODO: what a good way to handle FW blobs, size??
func (z *Firmware) Flash(firmwareImage []byte) error {
	ret := zesFirmwareFlash(z.handle, unsafe.Pointer(&firmwareImage[0]), uint32(len(firmwareImage)))
	return ret.ToError()
}

// GetFlashProgress wraps the zesFirmwareGetFlashProgress function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfirmwaregetflashprogress
func (z *Firmware) GetFlashProgress() (uint32, error) {
	var progress uint32
	ret := zesFirmwareGetFlashProgress(z.handle, &progress)
	return progress, ret.ToError()
}

// GetConsoleLogs wraps the zesFirmwareGetConsoleLogs function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfirmwaregetconsolelogs
func (z *Firmware) GetConsoleLogs() (string, error) {
	var (
		size uint64
		logs string
	)
	// Get size
	ret := zesFirmwareGetConsoleLogs(z.handle, &size, nil)
	if ret != core.RESULT_SUCCESS {
		return "", ret.ToError()
	}
	if size == 0 {
		return "", nil
	}
	// Get logs
	buf := make([]byte, size)
	ret = zesFirmwareGetConsoleLogs(z.handle, &size, &buf[0])
	if ret != core.RESULT_SUCCESS {
		return "", ret.ToError()
	}
	logs = string(buf)
	return logs, nil
}

// EnumFrequencyDomains wraps the zesDeviceEnumFrequencyDomains function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumfrequencydomains
func (z *Device) EnumFrequencyDomains() ([]*Frequency, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFrequencyDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]freqHandle, count)
	ret := zesDeviceEnumFrequencyDomains(z.handle, &count, handles)
	return handlesToWrappers[freqHandle, Frequency](handles), ret.ToError()
}

// GetProperties wraps the zesFrequencyGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetproperties
func (z *Frequency) GetProperties() (FreqProperties, error) {
	var props FreqProperties
	ret := zesFrequencyGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetAvailableClocks wraps the zesFrequencyGetAvailableClocks function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetavailableclocks
func (z *Frequency) GetAvailableClocks() ([]float64, error) {
	count := uint32(0)
	if ret := zesFrequencyGetAvailableClocks(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	clocks := make([]float64, count)
	ret := zesFrequencyGetAvailableClocks(z.handle, &count, clocks)
	return clocks, ret.ToError()
}

// GetRange wraps the zesFrequencyGetRange function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetrange
func (z *Frequency) GetRange() (FreqRange, error) {
	var freqRange FreqRange
	ret := zesFrequencyGetRange(z.handle, &freqRange)
	return freqRange, ret.ToError()
}

// SetRange wraps the zesFrequencySetRange function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencysetrange
func (z *Frequency) SetRange(freqRange *FreqRange) error {
	ret := zesFrequencySetRange(z.handle, freqRange)
	return ret.ToError()
}

// GetState wraps the zesFrequencyGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetstate
func (z *Frequency) GetState() (FreqState, error) {
	var state FreqState
	ret := zesFrequencyGetState(z.handle, &state)
	return state, ret.ToError()
}

// GetThrottleTime wraps the zesFrequencyGetThrottleTime function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetthrottletime
func (z *Frequency) GetThrottleTime() (FreqThrottleTime, error) {
	var throttleTime FreqThrottleTime
	ret := zesFrequencyGetThrottleTime(z.handle, &throttleTime)
	return throttleTime, ret.ToError()
}

// EnumLeds wraps the zesDeviceEnumLeds function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumleds
func (z *Device) EnumLeds() ([]*Led, error) {
	count := uint32(0)
	if ret := zesDeviceEnumLeds(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]ledHandle, count)
	ret := zesDeviceEnumLeds(z.handle, &count, handles)
	return handlesToWrappers[ledHandle, Led](handles), ret.ToError()
}

// GetProperties wraps the zesLedGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesledgetproperties
func (z *Led) GetProperties() (LedProperties, error) {
	var props LedProperties
	ret := zesLedGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetState wraps the zesLedGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesledgetstate
func (z *Led) GetState() (LedState, error) {
	var state LedState
	ret := zesLedGetState(z.handle, &state)
	return state, ret.ToError()
}

// SetState wraps the zesLedSetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesledsetstate
func (z *Led) SetState(enable bool) error {
	ret := zesLedSetState(z.handle, boolToByte(enable))
	return ret.ToError()
}

// SetColor wraps the zesLedSetColor function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesledsetcolor
func (z *Led) SetColor(color LedColor) error {
	ret := zesLedSetColor(z.handle, &color)
	return ret.ToError()
}

// EnumMemoryModules wraps the zesDeviceEnumMemoryModules function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenummemorymodules
func (z *Device) EnumMemoryModules() ([]*Memory, error) {
	count := uint32(0)
	if ret := zesDeviceEnumMemoryModules(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]memHandle, count)
	ret := zesDeviceEnumMemoryModules(z.handle, &count, handles)
	return handlesToWrappers[memHandle, Memory](handles), ret.ToError()
}

// GetProperties wraps the zesMemoryGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesmemorygetproperties
func (z *Memory) GetProperties() (MemProperties, error) {
	var props MemProperties
	ret := zesMemoryGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetState wraps the zesMemoryGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesmemorygetstate
func (z *Memory) GetState() (MemState, error) {
	var state MemState
	ret := zesMemoryGetState(z.handle, &state)
	return state, ret.ToError()
}

// GetBandwidth wraps the zesMemoryGetBandwidth function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesmemorygetbandwidth
func (z *Memory) GetBandwidth() (MemBandwidth, error) {
	var bandwidth MemBandwidth
	ret := zesMemoryGetBandwidth(z.handle, &bandwidth)
	return bandwidth, ret.ToError()
}

// EnumPerformanceFactorDomains wraps the zesDeviceEnumPerformanceFactorDomains function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumperformancefactordomains
func (z *Device) EnumPerformanceFactorDomains() ([]*Performance, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPerformanceFactorDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]perfHandle, count)
	ret := zesDeviceEnumPerformanceFactorDomains(z.handle, &count, handles)
	return handlesToWrappers[perfHandle, Performance](handles), ret.ToError()
}

// GetProperties wraps the zesPerformanceFactorGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesperformancefactorgetproperties
func (z *Performance) GetProperties() (PerfProperties, error) {
	var props PerfProperties
	ret := zesPerformanceFactorGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetConfig wraps the zesPerformanceFactorGetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesperformancefactorgetconfig
func (z *Performance) GetConfig() (float64, error) {
	var factor float64
	ret := zesPerformanceFactorGetConfig(z.handle, &factor)
	return factor, ret.ToError()
}

// SetConfig wraps the zesPerformanceFactorSetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesperformancefactorsetconfig
func (z *Performance) SetConfig(factor float64) error {
	ret := zesPerformanceFactorSetConfig(z.handle, factor)
	return ret.ToError()
}

// EnumPowerDomains wraps the zesDeviceEnumPowerDomains function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumpowerdomains
func (z *Device) EnumPowerDomains() ([]*Power, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPowerDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]pwrHandle, count)
	ret := zesDeviceEnumPowerDomains(z.handle, &count, handles)
	powers := handlesToWrappersWithDevice[pwrHandle, Power](handles, z)
	return powers, ret.ToError()
}

// GetProperties wraps the zesPowerGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowergetproperties
func (z *Power) GetProperties() (PowerProperties, error) {
	props := PowerProperties{}

	if z.device.extensions[POWER_LIMITS_EXT_NAME] {
		props.ExtendedProperties = &PowerExtProperties{
			stype:        _STRUCTURE_TYPE_POWER_EXT_PROPERTIES,
			DefaultLimit: &PowerLimitExtDesc{},
		}

		pinner := runtime.Pinner{}
		pinner.Pin(props.ExtendedProperties)
		pinner.Pin(props.ExtendedProperties.DefaultLimit)
		defer pinner.Unpin()

		//nolint:staticcheck // could remove embedded field from selector
		props.PowerBaseProperties.pnext = unsafe.Pointer(props.ExtendedProperties)
	}

	ret := zesPowerGetProperties(z.handle, &props.PowerBaseProperties)

	return props, ret.ToError()
}

// GetEnergyCounter wraps the zesPowerGetEnergyCounter function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowergetenergycounter
func (z *Power) GetEnergyCounter() (PowerEnergyCounter, error) {
	var counter PowerEnergyCounter
	ret := zesPowerGetEnergyCounter(z.handle, &counter)
	return counter, ret.ToError()
}

// GetEnergyThreshold wraps the zesPowerGetEnergyThreshold function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowergetenergythreshold
func (z *Power) GetEnergyThreshold() (EnergyThreshold, error) {
	var threshold EnergyThreshold
	ret := zesPowerGetEnergyThreshold(z.handle, &threshold)
	return threshold, ret.ToError()
}

// SetEnergyThreshold wraps the zesPowerSetEnergyThreshold function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowersetenergythreshold
func (z *Power) SetEnergyThreshold(threshold float64) error {
	ret := zesPowerSetEnergyThreshold(z.handle, threshold)
	return ret.ToError()
}

// GetLimitsExt wraps the zesPowerGetLimitsExt function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowergetlimitsext
func (z *Power) GetLimitsExt() ([]PowerLimitExtDesc, error) {
	count := uint32(0)
	if ret := zesPowerGetLimitsExt(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	limits := make([]PowerLimitExtDesc, count)
	ret := zesPowerGetLimitsExt(z.handle, &count, limits)
	return limits, ret.ToError()
}

// SetLimitsExt wraps the zesPowerSetLimitsExt function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowersetlimitsext
func (z *Power) SetLimitsExt(limits []PowerLimitExtDesc) error {
	count := uint32(len(limits))
	ret := zesPowerSetLimitsExt(z.handle, &count, limits)
	return ret.ToError()
}

// EnumPsus wraps the zesDeviceEnumPsus function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumpsus
func (z *Device) EnumPsus() ([]*Psu, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPsus(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]psuHandle, count)
	ret := zesDeviceEnumPsus(z.handle, &count, handles)
	return handlesToWrappers[psuHandle, Psu](handles), ret.ToError()
}

// GetProperties wraps the zesPsuGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespsugetproperties
func (z *Psu) GetProperties() (PsuProperties, error) {
	var props PsuProperties
	ret := zesPsuGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetState wraps the zesPsuGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespsugetstate
func (z *Psu) GetState() (PsuState, error) {
	var state PsuState
	ret := zesPsuGetState(z.handle, &state)
	return state, ret.ToError()
}

// EnumRasErrorSets wraps the zesDeviceEnumRasErrorSets function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumraserrorsets
func (z *Device) EnumRasErrorSets() ([]*Ras, error) {
	count := uint32(0)
	if ret := zesDeviceEnumRasErrorSets(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]rasHandle, count)
	ret := zesDeviceEnumRasErrorSets(z.handle, &count, handles)
	return handlesToWrappersWithDevice[rasHandle, Ras](handles, z), ret.ToError()
}

// GetProperties wraps the zesRasGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasgetproperties
func (z *Ras) GetProperties() (RasProperties, error) {
	var props RasProperties
	ret := zesRasGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetConfig wraps the zesRasGetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasgetconfig
func (z *Ras) GetConfig() (RasConfig, error) {
	var config RasConfig
	ret := zesRasGetConfig(z.handle, &config)
	return config, ret.ToError()
}

// SetConfig wraps the zesRasSetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrassetconfig
func (z *Ras) SetConfig(config *RasConfig) error {
	ret := zesRasSetConfig(z.handle, config)
	return ret.ToError()
}

// GetState wraps the zesRasGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasgetstate
func (z *Ras) GetState(resetCounters bool) (RasState, error) {
	var state RasState
	ret := zesRasGetState(z.handle, boolToByte(resetCounters), &state)
	return state, ret.ToError()
}

// GetStateExp wraps the (experimental) zesRasGetStateExp function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasgetstateexp
// and if that is not supported, it wraps (legacy) zesRasGetState instead.
func (z *Ras) GetStateExp() ([]RasStateExp, error) {
	// experimental API supported by backend?
	ret := core.RESULT_ERROR_UNSUPPORTED_FEATURE
	if z.device.extensions[RAS_GET_STATE_EXP_NAME] {
		count := uint32(0)
		if ret = zesRasGetStateExp(z.handle, &count, nil); ret == core.RESULT_SUCCESS {
			states := make([]RasStateExp, count)
			if ret = zesRasGetStateExp(z.handle, &count, states); ret == core.RESULT_SUCCESS {
				return states, ret.ToError()
			}
		}
	}
	if ret != core.RESULT_ERROR_UNSUPPORTED_FEATURE {
		return nil, ret.ToError()
	}

	// no => convert legacy API result instead
	var state RasState
	if ret = zesRasGetState(z.handle, boolToByte(false), &state); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	states := make([]RasStateExp, len(state.Category))
	for cat, value := range state.Category {
		states[cat] = RasStateExp{
			// RasErrorCategoryExp is superset of RasErrorCat
			Category:     RasErrorCategoryExp(cat),
			ErrorCounter: value,
		}
	}
	return states, ret.ToError()
}

// ClearStateExp wraps the (experimental) zesRasClearStateExp function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasclearstateexp
func (z *Ras) ClearStateExp(cat RasErrorCategoryExp) error {
	ret := core.RESULT_ERROR_UNSUPPORTED_FEATURE
	if z.device.extensions[RAS_GET_STATE_EXP_NAME] {
		ret = zesRasClearStateExp(z.handle, cat)
	}
	return ret.ToError()
}

// EnumSchedulers wraps the zesDeviceEnumSchedulers function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumschedulers
func (z *Device) EnumSchedulers() ([]*Scheduler, error) {
	count := uint32(0)
	if ret := zesDeviceEnumSchedulers(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]schedHandle, count)
	ret := zesDeviceEnumSchedulers(z.handle, &count, handles)
	return handlesToWrappers[schedHandle, Scheduler](handles), ret.ToError()
}

// GetProperties wraps the zesSchedulerGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulergetproperties
func (z *Scheduler) GetProperties() (SchedProperties, error) {
	var props SchedProperties
	ret := zesSchedulerGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetCurrentMode wraps the zesSchedulerGetCurrentMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulergetcurrentmode
func (z *Scheduler) GetCurrentMode() (SchedMode, error) {
	var mode SchedMode
	ret := zesSchedulerGetCurrentMode(z.handle, &mode)
	return mode, ret.ToError()
}

// GetTimeoutModeProperties wraps the zesSchedulerGetTimeoutModeProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulergettimeoutmodeproperties
func (z *Scheduler) GetTimeoutModeProperties(getDefaults bool) (SchedTimeoutProperties, error) {
	var props SchedTimeoutProperties
	ret := zesSchedulerGetTimeoutModeProperties(z.handle, boolToByte(getDefaults), &props)
	return props, ret.ToError()
}

// GetTimesliceModeProperties wraps the zesSchedulerGetTimesliceModeProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulergettimeslicemodeproperties
func (z *Scheduler) GetTimesliceModeProperties(getDefaults bool) (SchedTimesliceProperties, error) {
	var props SchedTimesliceProperties
	ret := zesSchedulerGetTimesliceModeProperties(z.handle, boolToByte(getDefaults), &props)
	return props, ret.ToError()
}

// SetTimeoutMode wraps the zesSchedulerSetTimeoutMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulersettimeoutmode
func (z *Scheduler) SetTimeoutMode(properties *SchedTimeoutProperties) (bool, error) {
	var needReload byte
	ret := zesSchedulerSetTimeoutMode(z.handle, properties, &needReload)
	return needReload != 0, ret.ToError()
}

// SetTimesliceMode wraps the zesSchedulerSetTimesliceMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulersettimeslicemode
func (z *Scheduler) SetTimesliceMode(properties *SchedTimesliceProperties) (bool, error) {
	var needReload byte
	ret := zesSchedulerSetTimesliceMode(z.handle, properties, &needReload)
	return needReload != 0, ret.ToError()
}

// SetExclusiveMode wraps the zesSchedulerSetExclusiveMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulersetexclusivemode
func (z *Scheduler) SetExclusiveMode() (bool, error) {
	var needReload byte
	ret := zesSchedulerSetExclusiveMode(z.handle, &needReload)
	return needReload != 0, ret.ToError()
}

// EnumStandbyDomains wraps the zesDeviceEnumStandbyDomains function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumstandbydomains
func (z *Device) EnumStandbyDomains() ([]*Standby, error) {
	count := uint32(0)
	if ret := zesDeviceEnumStandbyDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]standbyHandle, count)
	ret := zesDeviceEnumStandbyDomains(z.handle, &count, handles)
	return handlesToWrappers[standbyHandle, Standby](handles), ret.ToError()
}

// GetProperties wraps the zesStandbyGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesstandbygetproperties
func (z *Standby) GetProperties() (StandbyProperties, error) {
	var props StandbyProperties
	ret := zesStandbyGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetMode wraps the zesStandbyGetMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesstandbygetmode
func (z *Standby) GetMode() (StandbyPromoMode, error) {
	var mode StandbyPromoMode
	ret := zesStandbyGetMode(z.handle, &mode)
	return mode, ret.ToError()
}

// SetMode wraps the zesStandbySetMode function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesstandbysetmode
func (z *Standby) SetMode(mode StandbyPromoMode) error {
	ret := zesStandbySetMode(z.handle, mode)
	return ret.ToError()
}

// EnumTemperatureSensors wraps the zesDeviceEnumTemperatureSensors function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumtemperaturesensors
func (z *Device) EnumTemperatureSensors() ([]*Temperature, error) {
	count := uint32(0)
	if ret := zesDeviceEnumTemperatureSensors(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]tempHandle, count)
	ret := zesDeviceEnumTemperatureSensors(z.handle, &count, handles)
	return handlesToWrappers[tempHandle, Temperature](handles), ret.ToError()
}

// GetProperties wraps the zesTemperatureGetProperties function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zestemperaturegetproperties
func (z *Temperature) GetProperties() (TempProperties, error) {
	var props TempProperties
	ret := zesTemperatureGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetConfig wraps the zesTemperatureGetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zestemperaturegetconfig
func (z *Temperature) GetConfig() (TempConfig, error) {
	var config TempConfig
	ret := zesTemperatureGetConfig(z.handle, &config)
	return config, ret.ToError()
}

// SetConfig wraps the zesTemperatureSetConfig function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zestemperaturesetconfig
func (z *Temperature) SetConfig(config *TempConfig) error {
	ret := zesTemperatureSetConfig(z.handle, config)
	return ret.ToError()
}

// GetState wraps the zesTemperatureGetState function:
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zestemperaturegetstate
func (z *Temperature) GetState() (float64, error) {
	var state float64
	ret := zesTemperatureGetState(z.handle, &state)
	return state, ret.ToError()
}

// durationToMillisecondsUint32 converts a time.Duration to milliseconds (uint32).
// Negative durations are treated as infinite timeout (UINT32_MAX).
// Durations that exceed UINT32_MAX-1 milliseconds are clamped to UINT32_MAX-1.
func durationToMillisecondsUint32(d time.Duration) uint32 {
	if d < 0 {
		return math.MaxUint32
	}

	ms := d.Milliseconds()

	if ms >= math.MaxUint32 {
		return math.MaxUint32 - 1
	}

	return uint32(ms)
}

// durationToMillisecondsUint64 converts a time.Duration to milliseconds (uint64).
// Negative durations are treated as infinite timeout (UINT64_MAX).
func durationToMillisecondsUint64(d time.Duration) uint64 {
	if d < 0 {
		return math.MaxUint64
	}
	return uint64(d.Milliseconds())
}
