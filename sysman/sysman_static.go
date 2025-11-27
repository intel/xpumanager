// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

//go:generate ../hack/generate-stringer.sh

package sysman

import (
	"math"
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

// Init wraps the zesInit function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesinit
// for more details.
func Init(flags InitFlags) error {
	ret := zesInit(flags)
	return ret.ToError()
}

// DriverGet wraps the zesDriverGet function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdriverget
// for more details.
func DriverGet() ([]*Driver, error) {
	count := uint32(0)
	if ret := zesDriverGet(&count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zeDriverHandle, count)
	ret := zesDriverGet(&count, handles)
	return handlesToWrappers[zeDriverHandle, Driver](handles), ret.ToError()
}

// GetExtensionProperties wraps the zesDriverGetExtensionProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdrivergetextensionproperties
// for more details.
func (z *Driver) GetExtensionProperties() ([]DriverExtensionProperties, error) {
	count := uint32(0)
	if ret := zesDriverGetExtensionProperties(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	extensions := make([]DriverExtensionProperties, count)
	ret := zesDriverGetExtensionProperties(z.handle, &count, extensions)
	return extensions, ret.ToError()
}

// DeviceGet wraps the zesDeviceGet function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceget
// for more details.
func (z *Driver) DeviceGet() ([]*Device, error) {
	count := uint32(0)
	if ret := zesDeviceGet(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zeDeviceHandle, count)
	ret := zesDeviceGet(z.handle, &count, handles)
	return handlesToWrappers[zeDeviceHandle, Device](handles), ret.ToError()
}

// EventListen wraps the zesDriverEventListen function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdrivereventlisten
// for more details.
func (z *Driver) EventListen(timeout time.Duration, devices []*Device) (uint32, []EventTypeFlags, error) {
	handles := wrappersToHandles[zeDeviceHandle, Device](devices)
	var numEvents uint32
	events := make([]EventTypeFlags, len(handles))
	ms := durationToMillisecondsUint32(timeout)
	ret := zesDriverEventListen(z.handle, ms, uint32(len(handles)), handles, &numEvents, events)
	return numEvents, events, ret.ToError()
}

// EventListenEx wraps the zesDriverEventListenEx function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdrivereventlistenex
// for more details.
func (z *Driver) EventListenEx(timeout time.Duration, devices []*Device) (uint32, []EventTypeFlags, error) {
	handles := wrappersToHandles[zeDeviceHandle, Device](devices)
	var numEvents uint32
	events := make([]EventTypeFlags, len(handles))
	ms := durationToMillisecondsUint64(timeout)
	ret := zesDriverEventListenEx(z.handle, ms, uint32(len(handles)), handles, &numEvents, events)
	return numEvents, events, ret.ToError()
}

// GetProperties wraps the zesDeviceGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegetproperties
// for more details.
func (z *Device) GetProperties() (DeviceProperties, error) {
	var props DeviceProperties
	ret := zesDeviceGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetState wraps the zesDeviceGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegetstate
// for more details.
func (z *Device) GetState() (DeviceState, error) {
	var state DeviceState
	ret := zesDeviceGetState(z.handle, &state)
	return state, ret.ToError()
}

// Reset wraps the zesDeviceReset function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicereset
// for more details.
func (z *Device) Reset(force bool) error {
	ret := zesDeviceReset(z.handle, boolToByte(force))
	return ret.ToError()
}

// ResetExt wraps the zesDeviceResetExt function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceresetext
// for more details.
func (z *Device) ResetExt(properties *ResetProperties) error {
	ret := zesDeviceResetExt(z.handle, properties)
	return ret.ToError()
}

// ProcessesGetState wraps the zesDeviceProcessesGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceprocessesgetstate
// for more details.
func (z *Device) ProcessesGetState() ([]ProcessState, error) {
	count := uint32(0)
	if ret := zesDeviceProcessesGetState(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	processes := make([]ProcessState, count)
	ret := zesDeviceProcessesGetState(z.handle, &count, processes)
	return processes, ret.ToError()
}

// EventRegister wraps the zesDeviceEventRegister function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceeventregister
// for more details.
func (z *Device) EventRegister(eventType EventTypeFlags) error {
	ret := zesDeviceEventRegister(z.handle, eventType)
	return ret.ToError()
}

// PciGetProperties wraps the zesDevicePciGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetproperties
// for more details.
func (z *Device) PciGetProperties() (PciProperties, error) {
	var props PciProperties
	ret := zesDevicePciGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// PciGetState wraps the zesDevicePciGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetstate
// for more details.
func (z *Device) PciGetState() (*PciState, error) {
	var state PciState
	ret := zesDevicePciGetState(z.handle, &state)
	return &state, ret.ToError()
}

// PciGetBars wraps the zesDevicePciGetBars function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetbars
// for more details.
func (z *Device) PciGetBars() ([]PciBarProperties, error) {
	count := uint32(0)
	if ret := zesDevicePciGetBars(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	bars := make([]PciBarProperties, count)
	ret := zesDevicePciGetBars(z.handle, &count, bars)
	return bars, ret.ToError()
}

// PciGetStats wraps the zesDevicePciGetStats function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicepcigetstats
// for more details.
func (z *Device) PciGetStats() (*PciStats, error) {
	var stats PciStats
	ret := zesDevicePciGetStats(z.handle, &stats)
	return &stats, ret.ToError()
}

// SetOverclockWaiver wraps the zesDeviceSetOverclockWaiver function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicesetoverclockwaiver
// for more details.
func (z *Device) SetOverclockWaiver() error {
	ret := zesDeviceSetOverclockWaiver(z.handle)
	return ret.ToError()
}

// GetOverclockDomains wraps the zesDeviceGetOverclockDomains function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegetoverclockdomains
// for more details.
func (z *Device) GetOverclockDomains() (OverclockDomain, error) {
	var domains uint32
	ret := zesDeviceGetOverclockDomains(z.handle, &domains)
	return OverclockDomain(domains), ret.ToError()
}

// GetOverclockControls wraps the zesDeviceGetOverclockControls function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegetoverclockcontrols
// for more details.
func (z *Device) GetOverclockControls(domainType OverclockDomain) (OverclockControl, error) {
	var controls uint32
	ret := zesDeviceGetOverclockControls(z.handle, domainType, &controls)
	return OverclockControl(controls), ret.ToError()
}

// ResetOverclockSettings wraps the zesDeviceResetOverclockSettings function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceresetoverclocksettings
// for more details.
func (z *Device) ResetOverclockSettings(onShippedState bool) error {
	ret := zesDeviceResetOverclockSettings(z.handle, boolToByte(onShippedState))
	return ret.ToError()
}

// ReadOverclockState wraps the zesDeviceReadOverclockState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicereadoverclockstate
// for more details.
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

// EccAvailable wraps the zesDeviceEccAvailable function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceeccavailable
// for more details.
func (z *Device) EccAvailable() (bool, error) {
	var available byte
	ret := zesDeviceEccAvailable(z.handle, &available)
	return available != 0, ret.ToError()
}

// EccConfigurable wraps the zesDeviceEccConfigurable function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceeccconfigurable
// for more details.
func (z *Device) EccConfigurable() (bool, error) {
	var configurable byte
	ret := zesDeviceEccConfigurable(z.handle, &configurable)
	return configurable != 0, ret.ToError()
}

// GetEccState wraps the zesDeviceGetEccState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdevicegeteccstate
// for more details.
func (z *Device) GetEccState() (DeviceEccProperties, error) {
	var state DeviceEccProperties
	ret := zesDeviceGetEccState(z.handle, &state)
	return state, ret.ToError()
}

// SetEccState wraps the zesDeviceSetEccState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceseteccstate
// for more details.
func (z *Device) SetEccState(newState DeviceEccDesc) (DeviceEccProperties, error) {
	var state DeviceEccProperties
	ret := zesDeviceSetEccState(z.handle, &newState, &state)
	return state, ret.ToError()
}

// EnumOverclockDomains wraps the zesDeviceEnumOverclockDomains function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumoverclockdomains
// for more details.
func (z *Device) EnumOverclockDomains() ([]*Overclock, error) {
	count := uint32(0)
	if ret := zesDeviceEnumOverclockDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesOverclockHandle, count)
	ret := zesDeviceEnumOverclockDomains(z.handle, &count, handles)
	return handlesToWrappers[zesOverclockHandle, Overclock](handles), ret.ToError()
}

// FabricPortGetMultiPortThroughput wraps the zesFabricPortGetMultiPortThroughput function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetmultiportthroughput
// for more details.
func (z *Device) FabricPortGetMultiPortThroughput(ports []*FabricPort) ([]FabricPortThroughput, error) {
	handles := wrappersToHandles[zesFabricPortHandle, FabricPort](ports)
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

// GetDomainProperties wraps the zesOverclockGetDomainProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetdomainproperties
// for more details.
func (z *Overclock) GetDomainProperties() (OverclockProperties, error) {
	var props OverclockProperties
	ret := zesOverclockGetDomainProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetDomainVFProperties wraps the zesOverclockGetDomainVFProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetdomainvfproperties
// for more details.
func (z *Overclock) GetDomainVFProperties() (VfProperty, error) {
	var props VfProperty
	ret := zesOverclockGetDomainVFProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetDomainControlProperties wraps the zesOverclockGetDomainControlProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetdomaincontrolproperties
// for more details.
func (z *Overclock) GetDomainControlProperties(domainControl OverclockControl) (ControlProperty, error) {
	var props ControlProperty
	ret := zesOverclockGetDomainControlProperties(z.handle, domainControl, &props)
	return props, ret.ToError()
}

// GetControlCurrentValue wraps the zesOverclockGetControlCurrentValue function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetcontrolcurrentvalue
// for more details.
func (z *Overclock) GetControlCurrentValue(domainControl OverclockControl) (float64, error) {
	var value float64
	ret := zesOverclockGetControlCurrentValue(z.handle, domainControl, &value)
	return value, ret.ToError()
}

// GetControlPendingValue wraps the zesOverclockGetControlPendingValue function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetcontrolpendingvalue
// for more details.
func (z *Overclock) GetControlPendingValue(domainControl OverclockControl) (float64, error) {
	var value float64
	ret := zesOverclockGetControlPendingValue(z.handle, domainControl, &value)
	return value, ret.ToError()
}

// SetControlUserValue wraps the zesOverclockSetControlUserValue function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclocksetcontroluservalue
// for more details.
func (z *Overclock) SetControlUserValue(domainControl OverclockControl, value float64) (PendingAction, error) {
	var pendingAction PendingAction
	ret := zesOverclockSetControlUserValue(z.handle, domainControl, value, &pendingAction)
	return pendingAction, ret.ToError()
}

// GetControlState wraps the zesOverclockGetControlState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetcontrolstate
// for more details.
func (z *Overclock) GetControlState(domainControl OverclockControl) (ControlState, PendingAction, error) {
	var (
		state         ControlState
		pendingAction PendingAction
	)
	ret := zesOverclockGetControlState(z.handle, domainControl, &state, &pendingAction)
	return state, pendingAction, ret.ToError()
}

// GetVFPointValues wraps the zesOverclockGetVFPointValues function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclockgetvfpointvalues
// for more details.
func (z *Overclock) GetVFPointValues(vfType VfType, arrayType VfArrayType, pointIndex uint32) (uint32, error) {
	var value uint32
	ret := zesOverclockGetVFPointValues(z.handle, vfType, arrayType, pointIndex, &value)
	return value, ret.ToError()
}

// SetVFPointValues wraps the zesOverclockSetVFPointValues function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesoverclocksetvfpointvalues
// for more details.
func (z *Overclock) SetVFPointValues(vfType VfType, pointIndex uint32, value uint32) error {
	ret := zesOverclockSetVFPointValues(z.handle, vfType, pointIndex, value)
	return ret.ToError()
}

// EnumDiagnosticTestSuites wraps the zesDeviceEnumDiagnosticTestSuites function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumdiagnostictestsuites
// for more details.
func (z *Device) EnumDiagnosticTestSuites() ([]*Diagnostics, error) {
	count := uint32(0)
	if ret := zesDeviceEnumDiagnosticTestSuites(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesDiagHandle, count)
	ret := zesDeviceEnumDiagnosticTestSuites(z.handle, &count, handles)
	return handlesToWrappers[zesDiagHandle, Diagnostics](handles), ret.ToError()
}

// GetProperties wraps the zesDiagnosticsGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdiagnosticsgetproperties
// for more details.
func (z *Diagnostics) GetProperties() (DiagProperties, error) {
	var props DiagProperties
	ret := zesDiagnosticsGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetTests wraps the zesDiagnosticsGetTests function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdiagnosticsgettests
// for more details.
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

// RunTests wraps the zesDiagnosticsRunTests function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdiagnosticsruntests
// for more details.
func (z *Diagnostics) RunTests(startIdx, endIdx uint32) ([]DiagResult, error) {
	count := endIdx - startIdx + 1
	results := make([]DiagResult, count)
	ret := zesDiagnosticsRunTests(z.handle, startIdx, endIdx, results)
	return results, ret.ToError()

}

// EnumEngineGroups wraps the zesDeviceEnumEngineGroups function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumenginegroups
// for more details.
func (z *Device) EnumEngineGroups() ([]*Engine, error) {
	count := uint32(0)
	if ret := zesDeviceEnumEngineGroups(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesEngineHandle, count)
	ret := zesDeviceEnumEngineGroups(z.handle, &count, handles)
	return handlesToWrappers[zesEngineHandle, Engine](handles), ret.ToError()
}

// GetProperties wraps the zesEngineGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesenginegetproperties
// for more details.
func (z *Engine) GetProperties() (EngineProperties, error) {
	var props EngineProperties
	ret := zesEngineGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetActivity wraps the zesEngineGetActivity function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesenginegetactivity
// for more details.
func (z *Engine) GetActivity() (EngineStats, error) {
	var stats EngineStats
	ret := zesEngineGetActivity(z.handle, &stats)
	return stats, ret.ToError()
}

// GetActivityExt wraps the zesEngineGetActivityExt function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesenginegetactivityext
// for more details.
func (z *Engine) GetActivityExt() ([]EngineStats, error) {
	count := uint32(0)
	if ret := zesEngineGetActivityExt(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	stats := make([]EngineStats, count)
	ret := zesEngineGetActivityExt(z.handle, &count, stats)
	return stats, ret.ToError()
}

// EnumFabricPorts wraps the zesDeviceEnumFabricPorts function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumfabricports
// for more details.
func (z *Device) EnumFabricPorts() ([]*FabricPort, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFabricPorts(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFabricPortHandle, count)
	ret := zesDeviceEnumFabricPorts(z.handle, &count, handles)
	return handlesToWrappers[zesFabricPortHandle, FabricPort](handles), ret.ToError()
}

// GetProperties wraps the zesFabricPortGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetproperties
// for more details.
func (z *FabricPort) GetProperties() (FabricPortProperties, error) {
	var props FabricPortProperties
	ret := zesFabricPortGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetLinkType wraps the zesFabricPortGetLinkType function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetlinktype
// for more details.
func (z *FabricPort) GetLinkType() (FabricLinkType, error) {
	var linkType FabricLinkType
	ret := zesFabricPortGetLinkType(z.handle, &linkType)
	return linkType, ret.ToError()
}

// GetConfig wraps the zesFabricPortGetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetconfig
// for more details.
func (z *FabricPort) GetConfig() (FabricPortConfig, error) {
	var config FabricPortConfig
	ret := zesFabricPortGetConfig(z.handle, &config)
	return config, ret.ToError()
}

// SetConfig wraps the zesFabricPortSetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportsetconfig
// for more details.
func (z *FabricPort) SetConfig(config *FabricPortConfig) error {
	ret := zesFabricPortSetConfig(z.handle, config)
	return ret.ToError()
}

// GetState wraps the zesFabricPortGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetstate
// for more details.
func (z *FabricPort) GetState() (FabricPortState, error) {
	var state FabricPortState
	ret := zesFabricPortGetState(z.handle, &state)
	return state, ret.ToError()
}

// GetThroughput wraps the zesFabricPortGetThroughputRaw function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetthroughput
// for more details.
func (z *FabricPort) GetThroughput() (FabricPortThroughput, error) {
	var throughput FabricPortThroughput
	ret := zesFabricPortGetThroughput(z.handle, &throughput)
	return throughput, ret.ToError()
}

// GetFabricErrorCounters wraps the zesFabricPortGetFabricErrorCounters function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfabricportgetfabricerrorcounters
// for more details.
func (z *FabricPort) GetFabricErrorCounters() (FabricPortErrorCounters, error) {
	var counters FabricPortErrorCounters
	ret := zesFabricPortGetFabricErrorCounters(z.handle, &counters)
	return counters, ret.ToError()
}

// EnumFans wraps the zesDeviceEnumFans function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumfans
// for more details.
func (z *Device) EnumFans() ([]*Fan, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFans(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFanHandle, count)
	ret := zesDeviceEnumFans(z.handle, &count, handles)
	return handlesToWrappers[zesFanHandle, Fan](handles), ret.ToError()
}

// GetProperties wraps the zesFanGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfangetproperties
// for more details.
func (z *Fan) GetProperties() (FanProperties, error) {
	var props FanProperties
	ret := zesFanGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetConfig wraps the zesFanGetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfangetconfig
// for more details.
func (z *Fan) GetConfig() (FanConfig, error) {
	var config FanConfig
	ret := zesFanGetConfig(z.handle, &config)
	return config, ret.ToError()
}

// SetDefaultMode wraps the zesFanSetDefaultMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfansetdefaultmode
// for more details.
func (z *Fan) SetDefaultMode() error {
	ret := zesFanSetDefaultMode(z.handle)
	return ret.ToError()
}

// SetFixedSpeedMode wraps the zesFanSetFixedSpeedMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfansetfixedspeedmode
// for more details.
func (z *Fan) SetFixedSpeedMode(speed FanSpeed) error {
	ret := zesFanSetFixedSpeedMode(z.handle, &speed)
	return ret.ToError()
}

// SetSpeedTableMode wraps the zesFanSetSpeedTableMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfansetspeedtablemode
// for more details.
func (z *Fan) SetSpeedTableMode(speedTable *FanSpeedTable) error {
	ret := zesFanSetSpeedTableMode(z.handle, speedTable)
	return ret.ToError()
}

// GetState wraps the zesFanGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfangetstate
// for more details.
func (z *Fan) GetState(units FanSpeedUnits) (int32, error) {
	var speed int32
	ret := zesFanGetState(z.handle, units, &speed)
	return speed, ret.ToError()
}

// EnumFirmwares wraps the zesDeviceEnumFirmwares function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumfirmwares
// for more details.
func (z *Device) EnumFirmwares() ([]*Firmware, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFirmwares(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFirmwareHandle, count)
	ret := zesDeviceEnumFirmwares(z.handle, &count, handles)
	return handlesToWrappers[zesFirmwareHandle, Firmware](handles), ret.ToError()
}

// GetProperties wraps the zesFirmwareGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfirmwaregetproperties
// for more details.
func (z *Firmware) GetProperties() (FirmwareProperties, error) {
	var props FirmwareProperties
	ret := zesFirmwareGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// Flash wraps the zesFirmwareFlash function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfirmwareflash
// for more details.
// TODO: what a good way to handle FW blobs, size??
func (z *Firmware) Flash(firmwareImage []byte) error {
	ret := zesFirmwareFlash(z.handle, unsafe.Pointer(&firmwareImage[0]), uint32(len(firmwareImage)))
	return ret.ToError()
}

// GetFlashProgress wraps the zesFirmwareGetFlashProgress function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfirmwaregetflashprogress
// for more details.
func (z *Firmware) GetFlashProgress() (uint32, error) {
	var progress uint32
	ret := zesFirmwareGetFlashProgress(z.handle, &progress)
	return progress, ret.ToError()
}

// GetConsoleLogs wraps the zesFirmwareGetConsoleLogs function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfirmwaregetconsolelogs
// for more details.
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

// EnumFrequencyDomains wraps the zesDeviceEnumFrequencyDomains function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumfrequencydomains
// for more details.
func (z *Device) EnumFrequencyDomains() ([]*Freq, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFrequencyDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFreqHandle, count)
	ret := zesDeviceEnumFrequencyDomains(z.handle, &count, handles)
	return handlesToWrappers[zesFreqHandle, Freq](handles), ret.ToError()
}

// GetProperties wraps the zesFrequencyGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetproperties
// for more details.
func (z *Freq) GetProperties() (FreqProperties, error) {
	var props FreqProperties
	ret := zesFrequencyGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetAvailableClocks wraps the zesFrequencyGetAvailableClocks function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetavailableclocks
// for more details.
func (z *Freq) GetAvailableClocks() ([]float64, error) {
	count := uint32(0)
	if ret := zesFrequencyGetAvailableClocks(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	clocks := make([]float64, count)
	ret := zesFrequencyGetAvailableClocks(z.handle, &count, clocks)
	return clocks, ret.ToError()
}

// GetRange wraps the zesFrequencyGetRange function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetrange
// for more details.
func (z *Freq) GetRange() (FreqRange, error) {
	var freqRange FreqRange
	ret := zesFrequencyGetRange(z.handle, &freqRange)
	return freqRange, ret.ToError()
}

// SetRange wraps the zesFrequencySetRange function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencysetrange
// for more details.
func (z *Freq) SetRange(freqRange *FreqRange) error {
	ret := zesFrequencySetRange(z.handle, freqRange)
	return ret.ToError()
}

// GetState wraps the zesFrequencyGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetstate
// for more details.
func (z *Freq) GetState() (FreqState, error) {
	var state FreqState
	ret := zesFrequencyGetState(z.handle, &state)
	return state, ret.ToError()
}

// GetThrottleTime wraps the zesFrequencyGetThrottleTime function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesfrequencygetthrottletime
// for more details.
func (z *Freq) GetThrottleTime() (FreqThrottleTime, error) {
	var throttleTime FreqThrottleTime
	ret := zesFrequencyGetThrottleTime(z.handle, &throttleTime)
	return throttleTime, ret.ToError()
}

// EnumLeds wraps the zesDeviceEnumLeds function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumleds
// for more details.
func (z *Device) EnumLeds() ([]*Led, error) {
	count := uint32(0)
	if ret := zesDeviceEnumLeds(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesLedHandle, count)
	ret := zesDeviceEnumLeds(z.handle, &count, handles)
	return handlesToWrappers[zesLedHandle, Led](handles), ret.ToError()
}

// GetProperties wraps the zesLedGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesledgetproperties
// for more details.
func (z *Led) GetProperties() (LedProperties, error) {
	var props LedProperties
	ret := zesLedGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetState wraps the zesLedGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesledgetstate
// for more details.
func (z *Led) GetState() (LedState, error) {
	var state LedState
	ret := zesLedGetState(z.handle, &state)
	return state, ret.ToError()
}

// SetState wraps the zesLedSetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesledsetstate
// for more details.
func (z *Led) SetState(enable bool) error {
	ret := zesLedSetState(z.handle, boolToByte(enable))
	return ret.ToError()
}

// SetColor wraps the zesLedSetColor function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesledsetcolor
// for more details.
func (z *Led) SetColor(color LedColor) error {
	ret := zesLedSetColor(z.handle, &color)
	return ret.ToError()
}

// EnumMemoryModules wraps the zesDeviceEnumMemoryModules function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenummemorymodules
// for more details.
func (z *Device) EnumMemoryModules() ([]*Mem, error) {
	count := uint32(0)
	if ret := zesDeviceEnumMemoryModules(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesMemHandle, count)
	ret := zesDeviceEnumMemoryModules(z.handle, &count, handles)
	return handlesToWrappers[zesMemHandle, Mem](handles), ret.ToError()
}

// GetProperties wraps the zesMemoryGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesmemorygetproperties
// for more details.
func (z *Mem) GetProperties() (MemProperties, error) {
	var props MemProperties
	ret := zesMemoryGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetState wraps the zesMemoryGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesmemorygetstate
// for more details.
func (z *Mem) GetState() (MemState, error) {
	var state MemState
	ret := zesMemoryGetState(z.handle, &state)
	return state, ret.ToError()
}

// GetBandwidth wraps the zesMemoryGetBandwidth function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesmemorygetbandwidth
// for more details.
func (z *Mem) GetBandwidth() (MemBandwidth, error) {
	var bandwidth MemBandwidth
	ret := zesMemoryGetBandwidth(z.handle, &bandwidth)
	return bandwidth, ret.ToError()
}

// EnumPerformanceFactorDomains wraps the zesDeviceEnumPerformanceFactorDomains function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumperformancefactordomains
// for more details.
func (z *Device) EnumPerformanceFactorDomains() ([]*Perf, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPerformanceFactorDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPerfHandle, count)
	ret := zesDeviceEnumPerformanceFactorDomains(z.handle, &count, handles)
	return handlesToWrappers[zesPerfHandle, Perf](handles), ret.ToError()
}

// GetProperties wraps the zesPerformanceFactorGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesperformancefactorgetproperties
// for more details.
func (z *Perf) GetProperties() (PerfProperties, error) {
	var props PerfProperties
	ret := zesPerformanceFactorGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetConfig wraps the zesPerformanceFactorGetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesperformancefactorgetconfig
// for more details.
func (z *Perf) GetConfig() (float64, error) {
	var factor float64
	ret := zesPerformanceFactorGetConfig(z.handle, &factor)
	return factor, ret.ToError()
}

// SetConfig wraps the zesPerformanceFactorSetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesperformancefactorsetconfig
// for more details.
func (z *Perf) SetConfig(factor float64) error {
	ret := zesPerformanceFactorSetConfig(z.handle, factor)
	return ret.ToError()
}

// EnumPowerDomains wraps the zesDeviceEnumPowerDomains function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumpowerdomains
// for more details.
func (z *Device) EnumPowerDomains() ([]*Pwr, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPowerDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPwrHandle, count)
	ret := zesDeviceEnumPowerDomains(z.handle, &count, handles)
	return handlesToWrappers[zesPwrHandle, Pwr](handles), ret.ToError()
}

// GetProperties wraps the zesPowerGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowergetproperties
// for more details.
func (z *Pwr) GetProperties() (PowerProperties, error) {
	var props PowerProperties
	ret := zesPowerGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetEnergyCounter wraps the zesPowerGetEnergyCounter function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowergetenergycounter
// for more details.
func (z *Pwr) GetEnergyCounter() (PowerEnergyCounter, error) {
	var counter PowerEnergyCounter
	ret := zesPowerGetEnergyCounter(z.handle, &counter)
	return counter, ret.ToError()
}

// GetEnergyThreshold wraps the zesPowerGetEnergyThreshold function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowergetenergythreshold
// for more details.
func (z *Pwr) GetEnergyThreshold() (EnergyThreshold, error) {
	var threshold EnergyThreshold
	ret := zesPowerGetEnergyThreshold(z.handle, &threshold)
	return threshold, ret.ToError()
}

// SetEnergyThreshold wraps the zesPowerSetEnergyThreshold function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowersetenergythreshold
// for more details.
func (z *Pwr) SetEnergyThreshold(threshold float64) error {
	ret := zesPowerSetEnergyThreshold(z.handle, threshold)
	return ret.ToError()
}

// GetLimitsExt wraps the zesPowerGetLimitsExt function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowergetlimitsext
// for more details.
func (z *Pwr) GetLimitsExt() ([]PowerLimitExtDesc, error) {
	count := uint32(0)
	if ret := zesPowerGetLimitsExt(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	limits := make([]PowerLimitExtDesc, count)
	ret := zesPowerGetLimitsExt(z.handle, &count, limits)
	return limits, ret.ToError()
}

// SetLimitsExt wraps the zesPowerSetLimitsExt function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespowersetlimitsext
// for more details.
func (z *Pwr) SetLimitsExt(limits []PowerLimitExtDesc) error {
	count := uint32(len(limits))
	ret := zesPowerSetLimitsExt(z.handle, &count, limits)
	return ret.ToError()
}

// EnumPsus wraps the zesDeviceEnumPsus function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumpsus
// for more details.
func (z *Device) EnumPsus() ([]*Psu, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPsus(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPsuHandle, count)
	ret := zesDeviceEnumPsus(z.handle, &count, handles)
	return handlesToWrappers[zesPsuHandle, Psu](handles), ret.ToError()
}

// GetProperties wraps the zesPsuGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespsugetproperties
// for more details.
func (z *Psu) GetProperties() (PsuProperties, error) {
	var props PsuProperties
	ret := zesPsuGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetState wraps the zesPsuGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zespsugetstate
// for more details.
func (z *Psu) GetState() (PsuState, error) {
	var state PsuState
	ret := zesPsuGetState(z.handle, &state)
	return state, ret.ToError()
}

// EnumRasErrorSets wraps the zesDeviceEnumRasErrorSets function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumraserrorsets
// for more details.
func (z *Device) EnumRasErrorSets() ([]*Ras, error) {
	count := uint32(0)
	if ret := zesDeviceEnumRasErrorSets(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesRasHandle, count)
	ret := zesDeviceEnumRasErrorSets(z.handle, &count, handles)
	return handlesToWrappers[zesRasHandle, Ras](handles), ret.ToError()
}

// GetProperties wraps the zesRasGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasgetproperties
// for more details.
func (z *Ras) GetProperties() (RasProperties, error) {
	var props RasProperties
	ret := zesRasGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetConfig wraps the zesRasGetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasgetconfig
// for more details.
func (z *Ras) GetConfig() (RasConfig, error) {
	var config RasConfig
	ret := zesRasGetConfig(z.handle, &config)
	return config, ret.ToError()
}

// SetConfig wraps the zesRasSetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrassetconfig
// for more details.
func (z *Ras) SetConfig(config *RasConfig) error {
	ret := zesRasSetConfig(z.handle, config)
	return ret.ToError()
}

// GetState wraps the zesRasGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesrasgetstate
// for more details.
func (z *Ras) GetState(resetCounters bool) (RasState, error) {
	var state RasState
	ret := zesRasGetState(z.handle, boolToByte(resetCounters), &state)
	return state, ret.ToError()
}

// EnumSchedulers wraps the zesDeviceEnumSchedulers function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumschedulers
// for more details.
func (z *Device) EnumSchedulers() ([]*Sched, error) {
	count := uint32(0)
	if ret := zesDeviceEnumSchedulers(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesSchedHandle, count)
	ret := zesDeviceEnumSchedulers(z.handle, &count, handles)
	return handlesToWrappers[zesSchedHandle, Sched](handles), ret.ToError()
}

// GetProperties wraps the zesSchedulerGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulergetproperties
// for more details.
func (z *Sched) GetProperties() (SchedProperties, error) {
	var props SchedProperties
	ret := zesSchedulerGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetCurrentMode wraps the zesSchedulerGetCurrentMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulergetcurrentmode
// for more details.
func (z *Sched) GetCurrentMode() (SchedMode, error) {
	var mode SchedMode
	ret := zesSchedulerGetCurrentMode(z.handle, &mode)
	return mode, ret.ToError()
}

// GetTimeoutModeProperties wraps the zesSchedulerGetTimeoutModeProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulergettimeoutmodeproperties
// for more details.
func (z *Sched) GetTimeoutModeProperties(getDefaults bool) (SchedTimeoutProperties, error) {
	var props SchedTimeoutProperties
	ret := zesSchedulerGetTimeoutModeProperties(z.handle, boolToByte(getDefaults), &props)
	return props, ret.ToError()
}

// GetTimesliceModeProperties wraps the zesSchedulerGetTimesliceModeProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulergettimeslicemodeproperties
// for more details.
func (z *Sched) GetTimesliceModeProperties(getDefaults bool) (SchedTimesliceProperties, error) {
	var props SchedTimesliceProperties
	ret := zesSchedulerGetTimesliceModeProperties(z.handle, boolToByte(getDefaults), &props)
	return props, ret.ToError()
}

// SetTimeoutMode wraps the zesSchedulerSetTimeoutMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulersettimeoutmode
// for more details.
func (z *Sched) SetTimeoutMode(properties *SchedTimeoutProperties) (bool, error) {
	var needReload byte
	ret := zesSchedulerSetTimeoutMode(z.handle, properties, &needReload)
	return needReload != 0, ret.ToError()
}

// SetTimesliceMode wraps the zesSchedulerSetTimesliceMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulersettimeslicemode
// for more details.
func (z *Sched) SetTimesliceMode(properties *SchedTimesliceProperties) (bool, error) {
	var needReload byte
	ret := zesSchedulerSetTimesliceMode(z.handle, properties, &needReload)
	return needReload != 0, ret.ToError()
}

// SetExclusiveMode wraps the zesSchedulerSetExclusiveMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesschedulersetexclusivemode
// for more details.
func (z *Sched) SetExclusiveMode() (bool, error) {
	var needReload byte
	ret := zesSchedulerSetExclusiveMode(z.handle, &needReload)
	return needReload != 0, ret.ToError()
}

// EnumStandbyDomains wraps the zesDeviceEnumStandbyDomains function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumstandbydomains
// for more details.
func (z *Device) EnumStandbyDomains() ([]*Standby, error) {
	count := uint32(0)
	if ret := zesDeviceEnumStandbyDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesStandbyHandle, count)
	ret := zesDeviceEnumStandbyDomains(z.handle, &count, handles)
	return handlesToWrappers[zesStandbyHandle, Standby](handles), ret.ToError()
}

// GetProperties wraps the zesStandbyGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesstandbygetproperties
// for more details.
func (z *Standby) GetProperties() (StandbyProperties, error) {
	var props StandbyProperties
	ret := zesStandbyGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetMode wraps the zesStandbyGetMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesstandbygetmode
// for more details.
func (z *Standby) GetMode() (StandbyPromoMode, error) {
	var mode StandbyPromoMode
	ret := zesStandbyGetMode(z.handle, &mode)
	return mode, ret.ToError()
}

// SetMode wraps the zesStandbySetMode function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesstandbysetmode
// for more details.
func (z *Standby) SetMode(mode StandbyPromoMode) error {
	ret := zesStandbySetMode(z.handle, mode)
	return ret.ToError()
}

// EnumTemperatureSensors wraps the zesDeviceEnumTemperatureSensors function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zesdeviceenumtemperaturesensors
// for more details.
func (z *Device) EnumTemperatureSensors() ([]*Temp, error) {
	count := uint32(0)
	if ret := zesDeviceEnumTemperatureSensors(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesTempHandle, count)
	ret := zesDeviceEnumTemperatureSensors(z.handle, &count, handles)
	return handlesToWrappers[zesTempHandle, Temp](handles), ret.ToError()
}

// GetProperties wraps the zesTemperatureGetProperties function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zestemperaturegetproperties
// for more details.
func (z *Temp) GetProperties() (TempProperties, error) {
	var props TempProperties
	ret := zesTemperatureGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// GetConfig wraps the zesTemperatureGetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zestemperaturegetconfig
// for more details.
func (z *Temp) GetConfig() (TempConfig, error) {
	var config TempConfig
	ret := zesTemperatureGetConfig(z.handle, &config)
	return config, ret.ToError()
}

// SetConfig wraps the zesTemperatureSetConfig function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zestemperaturesetconfig
// for more details.
func (z *Temp) SetConfig(config *TempConfig) error {
	ret := zesTemperatureSetConfig(z.handle, config)
	return ret.ToError()
}

// GetState wraps the zesTemperatureGetState function. See
// https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zestemperaturegetstate
// for more details.
func (z *Temp) GetState() (float64, error) {
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
		return math.MaxUint32
	}
	return uint64(d.Milliseconds())
}
