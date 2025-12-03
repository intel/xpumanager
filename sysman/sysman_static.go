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

	"github.com/google/uuid"
	"github.com/intel/level-zero-go/core"
)

func boolToByte(b bool) byte {
	if b {
		return 1
	}
	return 0
}

func Init(flags InitFlags) error {
	ret := zesInit(flags)
	return ret.ToError()
}

func DriverGet() ([]*Driver, error) {
	count := uint32(0)
	if ret := zesDriverGet(&count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zeDriverHandle, count)
	ret := zesDriverGet(&count, handles)
	return handlesToWrappers[zeDriverHandle, Driver](handles), ret.ToError()
}

func (z *Driver) GetExtensionProperties() ([]DriverExtensionProperties, error) {
	count := uint32(0)
	if ret := zesDriverGetExtensionProperties(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	extensions := make([]DriverExtensionProperties, count)
	ret := zesDriverGetExtensionProperties(z.handle, &count, extensions)
	return extensions, ret.ToError()
}

func (z *Driver) DeviceGet() ([]*Device, error) {
	count := uint32(0)
	if ret := zesDeviceGet(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zeDeviceHandle, count)
	ret := zesDeviceGet(z.handle, &count, handles)
	return handlesToWrappers[zeDeviceHandle, Device](handles), ret.ToError()
}

func (z *Driver) GetDeviceByUuidExp(uuid uuid.UUID) (*Device, bool, uint32, error) {
	var (
		deviceHandle zeDeviceHandle
		onSubdevice  byte
		subDeviceId  uint32
	)
	zesUUID := Uuid{Id: uuid}
	ret := zesDriverGetDeviceByUuidExp(z.handle, zesUUID, &deviceHandle, &onSubdevice, &subDeviceId)
	if ret != core.RESULT_SUCCESS {
		return nil, false, 0, ret.ToError()
	}
	device := &Device{}
	device.setHandle(deviceHandle)
	return device, onSubdevice != 0, subDeviceId, nil
}

func (z *Driver) EventListen(timeout time.Duration, devices []*Device) (uint32, []EventTypeFlags, error) {
	handles := wrappersToHandles[zeDeviceHandle, Device](devices)
	var numEvents uint32
	events := make([]EventTypeFlags, len(handles))
	ms := durationToMillisecondsUint32(timeout)
	ret := zesDriverEventListen(z.handle, ms, uint32(len(handles)), handles, &numEvents, events)
	return numEvents, events, ret.ToError()
}

func (z *Driver) EventListenEx(timeout time.Duration, devices []*Device) (uint32, []EventTypeFlags, error) {
	handles := wrappersToHandles[zeDeviceHandle, Device](devices)
	var numEvents uint32
	events := make([]EventTypeFlags, len(handles))
	ms := durationToMillisecondsUint64(timeout)
	ret := zesDriverEventListenEx(z.handle, ms, uint32(len(handles)), handles, &numEvents, events)
	return numEvents, events, ret.ToError()
}

func (z *Device) GetProperties() (DeviceProperties, error) {
	var props DeviceProperties
	ret := zesDeviceGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Device) GetSubDevicePropertiesExp() ([]SubdeviceExpProperties, error) {
	count := uint32(0)
	if ret := zesDeviceGetSubDevicePropertiesExp(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	props := make([]SubdeviceExpProperties, count)
	ret := zesDeviceGetSubDevicePropertiesExp(z.handle, &count, props)
	return props, ret.ToError()
}

func (z *Device) GetState() (DeviceState, error) {
	var state DeviceState
	ret := zesDeviceGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *Device) Reset(force bool) error {
	ret := zesDeviceReset(z.handle, boolToByte(force))
	return ret.ToError()
}

func (z *Device) ResetExt(properties *ResetProperties) error {
	ret := zesDeviceResetExt(z.handle, properties)
	return ret.ToError()
}

func (z *Device) ProcessesGetState() ([]ProcessState, error) {
	count := uint32(0)
	if ret := zesDeviceProcessesGetState(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	processes := make([]ProcessState, count)
	ret := zesDeviceProcessesGetState(z.handle, &count, processes)
	return processes, ret.ToError()
}

func (z *Device) EventRegister(eventType EventTypeFlags) error {
	ret := zesDeviceEventRegister(z.handle, eventType)
	return ret.ToError()
}

func (z *Device) PciGetProperties() (PciProperties, error) {
	var props PciProperties
	ret := zesDevicePciGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Device) PciGetState() (*PciState, error) {
	var state PciState
	ret := zesDevicePciGetState(z.handle, &state)
	return &state, ret.ToError()
}

func (z *Device) PciGetBars() ([]PciBarProperties, error) {
	count := uint32(0)
	if ret := zesDevicePciGetBars(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	bars := make([]PciBarProperties, count)
	ret := zesDevicePciGetBars(z.handle, &count, bars)
	return bars, ret.ToError()
}

func (z *Device) PciGetStats() (*PciStats, error) {
	var stats PciStats
	ret := zesDevicePciGetStats(z.handle, &stats)
	return &stats, ret.ToError()
}

func (z *Device) SetOverclockWaiver() error {
	ret := zesDeviceSetOverclockWaiver(z.handle)
	return ret.ToError()
}

func (z *Device) GetOverclockDomains() (OverclockDomain, error) {
	var domains uint32
	ret := zesDeviceGetOverclockDomains(z.handle, &domains)
	return OverclockDomain(domains), ret.ToError()
}

func (z *Device) GetOverclockControls(domainType OverclockDomain) (OverclockControl, error) {
	var controls uint32
	ret := zesDeviceGetOverclockControls(z.handle, domainType, &controls)
	return OverclockControl(controls), ret.ToError()
}

func (z *Device) ResetOverclockSettings(onShippedState bool) error {
	ret := zesDeviceResetOverclockSettings(z.handle, boolToByte(onShippedState))
	return ret.ToError()
}

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

func (z *Device) EccAvailable() (bool, error) {
	var available byte
	ret := zesDeviceEccAvailable(z.handle, &available)
	return available != 0, ret.ToError()
}

func (z *Device) EccConfigurable() (bool, error) {
	var configurable byte
	ret := zesDeviceEccConfigurable(z.handle, &configurable)
	return configurable != 0, ret.ToError()
}

func (z *Device) GetEccState() (DeviceEccProperties, error) {
	var state DeviceEccProperties
	ret := zesDeviceGetEccState(z.handle, &state)
	return state, ret.ToError()
}

func (z *Device) SetEccState(newState DeviceEccDesc) (DeviceEccProperties, error) {
	var state DeviceEccProperties
	ret := zesDeviceSetEccState(z.handle, &newState, &state)
	return state, ret.ToError()
}

func (z *Device) EnumOverclockDomains() ([]*Overclock, error) {
	count := uint32(0)
	if ret := zesDeviceEnumOverclockDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesOverclockHandle, count)
	ret := zesDeviceEnumOverclockDomains(z.handle, &count, handles)
	return handlesToWrappers[zesOverclockHandle, Overclock](handles), ret.ToError()
}

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

func (z *Overclock) GetDomainProperties() (OverclockProperties, error) {
	var props OverclockProperties
	ret := zesOverclockGetDomainProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Overclock) GetDomainVFProperties() (VfProperty, error) {
	var props VfProperty
	ret := zesOverclockGetDomainVFProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Overclock) GetDomainControlProperties(domainControl OverclockControl) (ControlProperty, error) {
	var props ControlProperty
	ret := zesOverclockGetDomainControlProperties(z.handle, domainControl, &props)
	return props, ret.ToError()
}

func (z *Overclock) GetControlCurrentValue(domainControl OverclockControl) (float64, error) {
	var value float64
	ret := zesOverclockGetControlCurrentValue(z.handle, domainControl, &value)
	return value, ret.ToError()
}

func (z *Overclock) GetControlPendingValue(domainControl OverclockControl) (float64, error) {
	var value float64
	ret := zesOverclockGetControlPendingValue(z.handle, domainControl, &value)
	return value, ret.ToError()
}

func (z *Overclock) SetControlUserValue(domainControl OverclockControl, value float64) (PendingAction, error) {
	var pendingAction PendingAction
	ret := zesOverclockSetControlUserValue(z.handle, domainControl, value, &pendingAction)
	return pendingAction, ret.ToError()
}

func (z *Overclock) GetControlState(domainControl OverclockControl) (ControlState, PendingAction, error) {
	var (
		state         ControlState
		pendingAction PendingAction
	)
	ret := zesOverclockGetControlState(z.handle, domainControl, &state, &pendingAction)
	return state, pendingAction, ret.ToError()
}

func (z *Overclock) GetVFPointValues(vfType VfType, arrayType VfArrayType, pointIndex uint32) (uint32, error) {
	var value uint32
	ret := zesOverclockGetVFPointValues(z.handle, vfType, arrayType, pointIndex, &value)
	return value, ret.ToError()
}

func (z *Overclock) SetVFPointValues(vfType VfType, pointIndex uint32, value uint32) error {
	ret := zesOverclockSetVFPointValues(z.handle, vfType, pointIndex, value)
	return ret.ToError()
}

func (z *Device) EnumDiagnosticTestSuites() ([]*Diagnostics, error) {
	count := uint32(0)
	if ret := zesDeviceEnumDiagnosticTestSuites(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesDiagHandle, count)
	ret := zesDeviceEnumDiagnosticTestSuites(z.handle, &count, handles)
	return handlesToWrappers[zesDiagHandle, Diagnostics](handles), ret.ToError()
}

func (z *Diagnostics) GetProperties() (DiagProperties, error) {
	var props DiagProperties
	ret := zesDiagnosticsGetProperties(z.handle, &props)
	return props, ret.ToError()
}

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

func (z *Diagnostics) RunTests(startIdx, endIdx uint32) ([]DiagResult, error) {
	count := endIdx - startIdx + 1
	results := make([]DiagResult, count)
	ret := zesDiagnosticsRunTests(z.handle, startIdx, endIdx, results)
	return results, ret.ToError()

}

func (z *Device) EnumEngineGroups() ([]*Engine, error) {
	count := uint32(0)
	if ret := zesDeviceEnumEngineGroups(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesEngineHandle, count)
	ret := zesDeviceEnumEngineGroups(z.handle, &count, handles)
	return handlesToWrappers[zesEngineHandle, Engine](handles), ret.ToError()
}

func (z *Engine) GetProperties() (EngineProperties, error) {
	var props EngineProperties
	ret := zesEngineGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Engine) GetActivity() (EngineStats, error) {
	var stats EngineStats
	ret := zesEngineGetActivity(z.handle, &stats)
	return stats, ret.ToError()
}

func (z *Engine) GetActivityExt() ([]EngineStats, error) {
	count := uint32(0)
	if ret := zesEngineGetActivityExt(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	stats := make([]EngineStats, count)
	ret := zesEngineGetActivityExt(z.handle, &count, stats)
	return stats, ret.ToError()
}

func (z *Device) EnumFabricPorts() ([]*FabricPort, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFabricPorts(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFabricPortHandle, count)
	ret := zesDeviceEnumFabricPorts(z.handle, &count, handles)
	return handlesToWrappers[zesFabricPortHandle, FabricPort](handles), ret.ToError()
}

func (z *FabricPort) GetProperties() (FabricPortProperties, error) {
	var props FabricPortProperties
	ret := zesFabricPortGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *FabricPort) GetLinkType() (FabricLinkType, error) {
	var linkType FabricLinkType
	ret := zesFabricPortGetLinkType(z.handle, &linkType)
	return linkType, ret.ToError()
}

func (z *FabricPort) GetConfig() (FabricPortConfig, error) {
	var config FabricPortConfig
	ret := zesFabricPortGetConfig(z.handle, &config)
	return config, ret.ToError()
}

func (z *FabricPort) SetConfig(config *FabricPortConfig) error {
	ret := zesFabricPortSetConfig(z.handle, config)
	return ret.ToError()
}

func (z *FabricPort) GetState() (FabricPortState, error) {
	var state FabricPortState
	ret := zesFabricPortGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *FabricPort) GetThroughput() (FabricPortThroughput, error) {
	var throughput FabricPortThroughput
	ret := zesFabricPortGetThroughput(z.handle, &throughput)
	return throughput, ret.ToError()
}

func (z *FabricPort) GetFabricErrorCounters() (FabricPortErrorCounters, error) {
	var counters FabricPortErrorCounters
	ret := zesFabricPortGetFabricErrorCounters(z.handle, &counters)
	return counters, ret.ToError()
}

func (z *Device) EnumFans() ([]*Fan, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFans(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFanHandle, count)
	ret := zesDeviceEnumFans(z.handle, &count, handles)
	return handlesToWrappers[zesFanHandle, Fan](handles), ret.ToError()
}

func (z *Fan) GetProperties() (FanProperties, error) {
	var props FanProperties
	ret := zesFanGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Fan) GetConfig() (FanConfig, error) {
	var config FanConfig
	ret := zesFanGetConfig(z.handle, &config)
	return config, ret.ToError()
}

func (z *Fan) SetDefaultMode() error {
	ret := zesFanSetDefaultMode(z.handle)
	return ret.ToError()
}

func (z *Fan) SetFixedSpeedMode(speed FanSpeed) error {
	ret := zesFanSetFixedSpeedMode(z.handle, &speed)
	return ret.ToError()
}

func (z *Fan) SetSpeedTableMode(speedTable *FanSpeedTable) error {
	ret := zesFanSetSpeedTableMode(z.handle, speedTable)
	return ret.ToError()
}

func (z *Fan) GetState(units FanSpeedUnits) (int32, error) {
	var speed int32
	ret := zesFanGetState(z.handle, units, &speed)
	return speed, ret.ToError()
}

func (z *Device) EnumFirmwares() ([]*Firmware, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFirmwares(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFirmwareHandle, count)
	ret := zesDeviceEnumFirmwares(z.handle, &count, handles)
	return handlesToWrappers[zesFirmwareHandle, Firmware](handles), ret.ToError()
}

func (z *Firmware) GetProperties() (FirmwareProperties, error) {
	var props FirmwareProperties
	ret := zesFirmwareGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// TODO: what a good way to handle FW blobs, size??
func (z *Firmware) Flash(firmwareImage []byte) error {
	ret := zesFirmwareFlash(z.handle, unsafe.Pointer(&firmwareImage[0]), uint32(len(firmwareImage)))
	return ret.ToError()
}

func (z *Firmware) GetFlashProgress() (uint32, error) {
	var progress uint32
	ret := zesFirmwareGetFlashProgress(z.handle, &progress)
	return progress, ret.ToError()
}

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

func (z *Firmware) GetSecurityVersionExp() (string, error) {
	version := make([]byte, STRING_PROPERTY_SIZE)
	ret := zesFirmwareGetSecurityVersionExp(z.handle, &version[0])
	if ret != core.RESULT_SUCCESS {
		return "", ret.ToError()
	}
	return string(version), nil
}

func (z *Firmware) SetSecurityVersionExp() error {
	ret := zesFirmwareSetSecurityVersionExp(z.handle)
	return ret.ToError()
}

func (z *Device) EnumFrequencyDomains() ([]*Freq, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFrequencyDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFreqHandle, count)
	ret := zesDeviceEnumFrequencyDomains(z.handle, &count, handles)
	return handlesToWrappers[zesFreqHandle, Freq](handles), ret.ToError()
}

func (z *Freq) GetProperties() (FreqProperties, error) {
	var props FreqProperties
	ret := zesFrequencyGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Freq) GetAvailableClocks() ([]float64, error) {
	count := uint32(0)
	if ret := zesFrequencyGetAvailableClocks(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	clocks := make([]float64, count)
	ret := zesFrequencyGetAvailableClocks(z.handle, &count, clocks)
	return clocks, ret.ToError()
}

func (z *Freq) GetRange() (FreqRange, error) {
	var freqRange FreqRange
	ret := zesFrequencyGetRange(z.handle, &freqRange)
	return freqRange, ret.ToError()
}

func (z *Freq) SetRange(freqRange *FreqRange) error {
	ret := zesFrequencySetRange(z.handle, freqRange)
	return ret.ToError()
}

func (z *Freq) GetState() (FreqState, error) {
	var state FreqState
	ret := zesFrequencyGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *Freq) GetThrottleTime() (FreqThrottleTime, error) {
	var throttleTime FreqThrottleTime
	ret := zesFrequencyGetThrottleTime(z.handle, &throttleTime)
	return throttleTime, ret.ToError()
}

func (z *Device) EnumLeds() ([]*Led, error) {
	count := uint32(0)
	if ret := zesDeviceEnumLeds(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesLedHandle, count)
	ret := zesDeviceEnumLeds(z.handle, &count, handles)
	return handlesToWrappers[zesLedHandle, Led](handles), ret.ToError()
}

func (z *Led) GetProperties() (LedProperties, error) {
	var props LedProperties
	ret := zesLedGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Led) GetState() (LedState, error) {
	var state LedState
	ret := zesLedGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *Led) SetState(enable bool) error {
	ret := zesLedSetState(z.handle, boolToByte(enable))
	return ret.ToError()
}

func (z *Led) SetColor(color LedColor) error {
	ret := zesLedSetColor(z.handle, &color)
	return ret.ToError()
}

func (z *Device) EnumMemoryModules() ([]*Mem, error) {
	count := uint32(0)
	if ret := zesDeviceEnumMemoryModules(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesMemHandle, count)
	ret := zesDeviceEnumMemoryModules(z.handle, &count, handles)
	return handlesToWrappers[zesMemHandle, Mem](handles), ret.ToError()
}

func (z *Mem) GetProperties() (MemProperties, error) {
	var props MemProperties
	ret := zesMemoryGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Mem) GetState() (MemState, error) {
	var state MemState
	ret := zesMemoryGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *Mem) GetBandwidth() (MemBandwidth, error) {
	var bandwidth MemBandwidth
	ret := zesMemoryGetBandwidth(z.handle, &bandwidth)
	return bandwidth, ret.ToError()
}

func (z *Device) EnumPerformanceFactorDomains() ([]*Perf, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPerformanceFactorDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPerfHandle, count)
	ret := zesDeviceEnumPerformanceFactorDomains(z.handle, &count, handles)
	return handlesToWrappers[zesPerfHandle, Perf](handles), ret.ToError()
}

func (z *Perf) GetProperties() (PerfProperties, error) {
	var props PerfProperties
	ret := zesPerformanceFactorGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Perf) GetConfig() (float64, error) {
	var factor float64
	ret := zesPerformanceFactorGetConfig(z.handle, &factor)
	return factor, ret.ToError()
}

func (z *Perf) SetConfig(factor float64) error {
	ret := zesPerformanceFactorSetConfig(z.handle, factor)
	return ret.ToError()
}

func (z *Device) EnumPowerDomains() ([]*Pwr, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPowerDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPwrHandle, count)
	ret := zesDeviceEnumPowerDomains(z.handle, &count, handles)
	return handlesToWrappers[zesPwrHandle, Pwr](handles), ret.ToError()
}

func (z *Pwr) GetProperties() (PowerProperties, error) {
	var props PowerProperties
	ret := zesPowerGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Pwr) GetEnergyCounter() (PowerEnergyCounter, error) {
	var counter PowerEnergyCounter
	ret := zesPowerGetEnergyCounter(z.handle, &counter)
	return counter, ret.ToError()
}

func (z *Pwr) GetEnergyThreshold() (EnergyThreshold, error) {
	var threshold EnergyThreshold
	ret := zesPowerGetEnergyThreshold(z.handle, &threshold)
	return threshold, ret.ToError()
}

func (z *Pwr) SetEnergyThreshold(threshold float64) error {
	ret := zesPowerSetEnergyThreshold(z.handle, threshold)
	return ret.ToError()
}

func (z *Pwr) GetLimitsExt() ([]PowerLimitExtDesc, error) {
	count := uint32(0)
	if ret := zesPowerGetLimitsExt(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	limits := make([]PowerLimitExtDesc, count)
	ret := zesPowerGetLimitsExt(z.handle, &count, limits)
	return limits, ret.ToError()
}

func (z *Pwr) SetLimitsExt(limits []PowerLimitExtDesc) error {
	count := uint32(len(limits))
	ret := zesPowerSetLimitsExt(z.handle, &count, limits)
	return ret.ToError()
}

func (z *Device) EnumPsus() ([]*Psu, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPsus(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPsuHandle, count)
	ret := zesDeviceEnumPsus(z.handle, &count, handles)
	return handlesToWrappers[zesPsuHandle, Psu](handles), ret.ToError()
}

func (z *Psu) GetProperties() (PsuProperties, error) {
	var props PsuProperties
	ret := zesPsuGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Psu) GetState() (PsuState, error) {
	var state PsuState
	ret := zesPsuGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *Device) EnumRasErrorSets() ([]*Ras, error) {
	count := uint32(0)
	if ret := zesDeviceEnumRasErrorSets(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesRasHandle, count)
	ret := zesDeviceEnumRasErrorSets(z.handle, &count, handles)
	return handlesToWrappers[zesRasHandle, Ras](handles), ret.ToError()
}

func (z *Ras) GetProperties() (RasProperties, error) {
	var props RasProperties
	ret := zesRasGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Ras) GetConfig() (RasConfig, error) {
	var config RasConfig
	ret := zesRasGetConfig(z.handle, &config)
	return config, ret.ToError()
}

func (z *Ras) SetConfig(config *RasConfig) error {
	ret := zesRasSetConfig(z.handle, config)
	return ret.ToError()
}

func (z *Ras) GetState(resetCounters bool) (RasState, error) {
	var state RasState
	ret := zesRasGetState(z.handle, boolToByte(resetCounters), &state)
	return state, ret.ToError()
}

func (z *Ras) GetStateExp() ([]RasStateExp, error) {
	count := uint32(0)
	if ret := zesRasGetStateExp(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	states := make([]RasStateExp, count)
	ret := zesRasGetStateExp(z.handle, &count, states)
	return states, ret.ToError()
}

func (z *Ras) ClearStateExp(category RasErrorCategoryExp) error {
	ret := zesRasClearStateExp(z.handle, category)
	return ret.ToError()
}

func (z *Device) EnumSchedulers() ([]*Sched, error) {
	count := uint32(0)
	if ret := zesDeviceEnumSchedulers(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesSchedHandle, count)
	ret := zesDeviceEnumSchedulers(z.handle, &count, handles)
	return handlesToWrappers[zesSchedHandle, Sched](handles), ret.ToError()
}

func (z *Sched) GetProperties() (SchedProperties, error) {
	var props SchedProperties
	ret := zesSchedulerGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Sched) GetCurrentMode() (SchedMode, error) {
	var mode SchedMode
	ret := zesSchedulerGetCurrentMode(z.handle, &mode)
	return mode, ret.ToError()
}

func (z *Sched) GetTimeoutModeProperties(getDefaults bool) (SchedTimeoutProperties, error) {
	var props SchedTimeoutProperties
	ret := zesSchedulerGetTimeoutModeProperties(z.handle, boolToByte(getDefaults), &props)
	return props, ret.ToError()
}

func (z *Sched) GetTimesliceModeProperties(getDefaults bool) (SchedTimesliceProperties, error) {
	var props SchedTimesliceProperties
	ret := zesSchedulerGetTimesliceModeProperties(z.handle, boolToByte(getDefaults), &props)
	return props, ret.ToError()
}

func (z *Sched) SetTimeoutMode(properties *SchedTimeoutProperties) (bool, error) {
	var needReload byte
	ret := zesSchedulerSetTimeoutMode(z.handle, properties, &needReload)
	return needReload != 0, ret.ToError()
}

func (z *Sched) SetTimesliceMode(properties *SchedTimesliceProperties) (bool, error) {
	var needReload byte
	ret := zesSchedulerSetTimesliceMode(z.handle, properties, &needReload)
	return needReload != 0, ret.ToError()
}

func (z *Sched) SetExclusiveMode() (bool, error) {
	var needReload byte
	ret := zesSchedulerSetExclusiveMode(z.handle, &needReload)
	return needReload != 0, ret.ToError()
}

func (z *Device) EnumStandbyDomains() ([]*Standby, error) {
	count := uint32(0)
	if ret := zesDeviceEnumStandbyDomains(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesStandbyHandle, count)
	ret := zesDeviceEnumStandbyDomains(z.handle, &count, handles)
	return handlesToWrappers[zesStandbyHandle, Standby](handles), ret.ToError()
}

func (z *Standby) GetProperties() (StandbyProperties, error) {
	var props StandbyProperties
	ret := zesStandbyGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Standby) GetMode() (StandbyPromoMode, error) {
	var mode StandbyPromoMode
	ret := zesStandbyGetMode(z.handle, &mode)
	return mode, ret.ToError()
}

func (z *Standby) SetMode(mode StandbyPromoMode) error {
	ret := zesStandbySetMode(z.handle, mode)
	return ret.ToError()
}

func (z *Device) EnumTemperatureSensors() ([]*Temp, error) {
	count := uint32(0)
	if ret := zesDeviceEnumTemperatureSensors(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesTempHandle, count)
	ret := zesDeviceEnumTemperatureSensors(z.handle, &count, handles)
	return handlesToWrappers[zesTempHandle, Temp](handles), ret.ToError()
}

func (z *Temp) GetProperties() (TempProperties, error) {
	var props TempProperties
	ret := zesTemperatureGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *Temp) GetConfig() (TempConfig, error) {
	var config TempConfig
	ret := zesTemperatureGetConfig(z.handle, &config)
	return config, ret.ToError()
}

func (z *Temp) SetConfig(config *TempConfig) error {
	ret := zesTemperatureSetConfig(z.handle, config)
	return ret.ToError()
}

func (z *Temp) GetState() (float64, error) {
	var state float64
	ret := zesTemperatureGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *Device) EnumEnabledVFExp() ([]*Vf, error) {
	count := uint32(0)
	if ret := zesDeviceEnumEnabledVFExp(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesVfHandle, count)
	ret := zesDeviceEnumEnabledVFExp(z.handle, &count, handles)
	return handlesToWrappers[zesVfHandle, Vf](handles), ret.ToError()
}

func (z *Vf) GetVFMemoryUtilizationExp2() ([]VfUtilMemExp2, error) {
	count := uint32(0)
	if ret := zesVFManagementGetVFMemoryUtilizationExp2(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	utils := make([]VfUtilMemExp2, count)
	ret := zesVFManagementGetVFMemoryUtilizationExp2(z.handle, &count, utils)
	return utils, ret.ToError()
}

func (z *Vf) GetVFEngineUtilizationExp2() ([]VfUtilEngineExp2, error) {
	count := uint32(0)
	if ret := zesVFManagementGetVFEngineUtilizationExp2(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	utils := make([]VfUtilEngineExp2, count)
	ret := zesVFManagementGetVFEngineUtilizationExp2(z.handle, &count, utils)
	return utils, ret.ToError()
}

func (z *Vf) GetVFCapabilitiesExp2() (VfExp2Capabilities, error) {
	var caps VfExp2Capabilities
	ret := zesVFManagementGetVFCapabilitiesExp2(z.handle, &caps)
	return caps, ret.ToError()
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
