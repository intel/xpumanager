// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

//go:generate go tool stringer -type ZeResult -output zz_strings.go -trimprefix ZE_RESULT_

package levelzero

import (
	"errors"
	"math"
	"time"
	"unsafe"

	"github.com/google/uuid"
)

func boolToByte(b bool) byte {
	if b {
		return 1
	}
	return 0
}

func (r *ZeResult) ToError() error {
	if *r == ZE_RESULT_SUCCESS {
		return nil
	}
	return errors.New(r.String())
}

func ZesInit(flags ZesInitFlags) error {
	ret := zesInit(flags)
	return ret.ToError()
}

func ZesDriverGet() ([]*ZeDriver, error) {
	count := uint32(0)
	if ret := zesDriverGet(&count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zeDriverHandle, count)
	ret := zesDriverGet(&count, handles)
	return handlesToWrappers[zeDriverHandle, ZeDriver](handles), ret.ToError()
}

func (z *ZeDriver) GetExtensionProperties() ([]ZesDriverExtensionProperties, error) {
	count := uint32(0)
	if ret := zesDriverGetExtensionProperties(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	extensions := make([]ZesDriverExtensionProperties, count)
	ret := zesDriverGetExtensionProperties(z.handle, &count, extensions)
	return extensions, ret.ToError()
}

func (z *ZeDriver) ZesDeviceGet() ([]*ZeDevice, error) {
	count := uint32(0)
	if ret := zesDeviceGet(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zeDeviceHandle, count)
	ret := zesDeviceGet(z.handle, &count, handles)
	return handlesToWrappers[zeDeviceHandle, ZeDevice](handles), ret.ToError()
}

func (z *ZeDriver) GetDeviceByUuidExp(uuid uuid.UUID) (*ZeDevice, bool, uint32, error) {
	var (
		deviceHandle zeDeviceHandle
		onSubdevice  byte
		subDeviceId  uint32
	)
	zesUUID := ZesUuid{Id: uuid}
	ret := zesDriverGetDeviceByUuidExp(z.handle, zesUUID, &deviceHandle, &onSubdevice, &subDeviceId)
	if ret != ZE_RESULT_SUCCESS {
		return nil, false, 0, ret.ToError()
	}
	device := &ZeDevice{}
	device.setHandle(deviceHandle)
	return device, onSubdevice != 0, subDeviceId, nil
}

func (z *ZeDriver) EventListen(timeout time.Duration, devices []*ZeDevice) (uint32, []ZesEventTypeFlag, error) {
	handles := wrappersToHandles[zeDeviceHandle, ZeDevice](devices)
	var numEvents uint32
	events := make([]ZesEventTypeFlags, len(handles))
	ms := durationToMillisecondsUint32(timeout)
	ret := zesDriverEventListen(z.handle, ms, uint32(len(handles)), handles, &numEvents, events)
	retE := make([]ZesEventTypeFlag, len(events))
	for i, e := range events {
		retE[i] = ZesEventTypeFlag(e)
	}
	return numEvents, retE, ret.ToError()
}

func (z *ZeDriver) EventListenEx(timeout time.Duration, devices []*ZeDevice) (uint32, []ZesEventTypeFlag, error) {
	handles := wrappersToHandles[zeDeviceHandle, ZeDevice](devices)
	var numEvents uint32
	events := make([]ZesEventTypeFlags, len(handles))
	ms := durationToMillisecondsUint64(timeout)
	ret := zesDriverEventListenEx(z.handle, ms, uint32(len(handles)), handles, &numEvents, events)
	retE := make([]ZesEventTypeFlag, len(events))
	for i, e := range events {
		retE[i] = ZesEventTypeFlag(e)
	}
	return numEvents, retE, ret.ToError()
}

func (z *ZeDevice) GetProperties() (ZesDeviceProperties, error) {
	var props zesDeviceProperties
	ret := zesDeviceGetProperties(z.handle, &props)
	return newZesDeviceProperties(&props), ret.ToError()
}

func (z *ZeDevice) GetSubDevicePropertiesExp() ([]ZesSubdeviceExpProperties, error) {
	count := uint32(0)
	if ret := zesDeviceGetSubDevicePropertiesExp(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	props := make([]ZesSubdeviceExpProperties, count)
	ret := zesDeviceGetSubDevicePropertiesExp(z.handle, &count, props)
	return props, ret.ToError()
}

func (z *ZeDevice) GetState() (ZesDeviceState, error) {
	var state ZesDeviceState
	ret := zesDeviceGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *ZeDevice) Reset(force bool) error {
	ret := zesDeviceReset(z.handle, boolToByte(force))
	return ret.ToError()
}

func (z *ZeDevice) ResetExt(properties *ZesResetProperties) error {
	ret := zesDeviceResetExt(z.handle, properties)
	return ret.ToError()
}

func (z *ZeDevice) ProcessesGetState() ([]ZesProcessState, error) {
	count := uint32(0)
	if ret := zesDeviceProcessesGetState(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	processes := make([]ZesProcessState, count)
	ret := zesDeviceProcessesGetState(z.handle, &count, processes)
	return processes, ret.ToError()
}

func (z *ZeDevice) EventRegister(eventType ZesEventTypeFlag) error {
	ret := zesDeviceEventRegister(z.handle, ZesEventTypeFlags(eventType))
	return ret.ToError()
}

func (z *ZeDevice) PciGetProperties() (ZesPciProperties, error) {
	var props ZesPciProperties
	ret := zesDevicePciGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZeDevice) PciGetState() (*ZesPciState, error) {
	var state ZesPciState
	ret := zesDevicePciGetState(z.handle, &state)
	return &state, ret.ToError()
}

func (z *ZeDevice) PciGetBars() ([]ZesPciBarProperties, error) {
	count := uint32(0)
	if ret := zesDevicePciGetBars(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	bars := make([]ZesPciBarProperties, count)
	ret := zesDevicePciGetBars(z.handle, &count, bars)
	return bars, ret.ToError()
}

func (z *ZeDevice) PciGetStats() (*ZesPciStats, error) {
	var stats ZesPciStats
	ret := zesDevicePciGetStats(z.handle, &stats)
	return &stats, ret.ToError()
}

func (z *ZeDevice) SetOverclockWaiver() error {
	ret := zesDeviceSetOverclockWaiver(z.handle)
	return ret.ToError()
}

func (z *ZeDevice) GetOverclockDomains() (ZesOverclockDomain, error) {
	var domains uint32
	ret := zesDeviceGetOverclockDomains(z.handle, &domains)
	return ZesOverclockDomain(domains), ret.ToError()
}

func (z *ZeDevice) GetOverclockControls(domainType ZesOverclockDomain) (ZesOverclockControl, error) {
	var controls uint32
	ret := zesDeviceGetOverclockControls(z.handle, domainType, &controls)
	return ZesOverclockControl(controls), ret.ToError()
}

func (z *ZeDevice) ResetOverclockSettings(onShippedState bool) error {
	ret := zesDeviceResetOverclockSettings(z.handle, boolToByte(onShippedState))
	return ret.ToError()
}

func (z *ZeDevice) ReadOverclockState() (ZesOverclockState, error) {
	var (
		mode          ZesOverclockMode
		waiverSetting byte
		state         byte
		pendingAction ZesPendingAction
		pendingReset  byte
	)
	ret := zesDeviceReadOverclockState(z.handle, &mode, &waiverSetting, &state, &pendingAction, &pendingReset)
	return ZesOverclockState{
		Mode:          mode,
		WaiverSetting: waiverSetting != 0,
		State:         state != 0,
		PendingAction: pendingAction,
		PendingReset:  pendingReset != 0,
	}, ret.ToError()
}

func (z *ZeDevice) EccAvailable() (bool, error) {
	var available byte
	ret := zesDeviceEccAvailable(z.handle, &available)
	return available != 0, ret.ToError()
}

func (z *ZeDevice) EccConfigurable() (bool, error) {
	var configurable byte
	ret := zesDeviceEccConfigurable(z.handle, &configurable)
	return configurable != 0, ret.ToError()
}

func (z *ZeDevice) GetEccState() (ZesDeviceEccProperties, error) {
	var state ZesDeviceEccProperties
	ret := zesDeviceGetEccState(z.handle, &state)
	return state, ret.ToError()
}

func (z *ZeDevice) SetEccState(newState ZesDeviceEccDesc) (ZesDeviceEccProperties, error) {
	var state ZesDeviceEccProperties
	ret := zesDeviceSetEccState(z.handle, &newState, &state)
	return state, ret.ToError()
}

func (z *ZeDevice) EnumOverclockDomains() ([]*ZesOverclock, error) {
	count := uint32(0)
	if ret := zesDeviceEnumOverclockDomains(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesOverclockHandle, count)
	ret := zesDeviceEnumOverclockDomains(z.handle, &count, handles)
	return handlesToWrappers[zesOverclockHandle, ZesOverclock](handles), ret.ToError()
}

func (z *ZeDevice) FabricPortGetMultiPortThroughput(ports []*ZesFabricPort) ([]ZesFabricPortThroughput, error) {
	handles := wrappersToHandles[zesFabricPortHandle, ZesFabricPort](ports)
	count := uint32(len(handles))
	throughputs := make([]*ZesFabricPortThroughput, count)
	for i := range throughputs {
		throughputs[i] = &ZesFabricPortThroughput{}
	}
	ret := zesFabricPortGetMultiPortThroughput(z.handle, count, handles, throughputs)
	retThroughput := make([]ZesFabricPortThroughput, count)
	for i, t := range throughputs {
		retThroughput[i] = *t
	}
	return retThroughput, ret.ToError()
}

func (z *ZesOverclock) GetDomainProperties() (ZesOverclockProperties, error) {
	var props ZesOverclockProperties
	ret := zesOverclockGetDomainProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesOverclock) GetDomainVFProperties() (ZesVfProperty, error) {
	var props ZesVfProperty
	ret := zesOverclockGetDomainVFProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesOverclock) GetDomainControlProperties(domainControl ZesOverclockControl) (ZesControlProperty, error) {
	var props ZesControlProperty
	ret := zesOverclockGetDomainControlProperties(z.handle, domainControl, &props)
	return props, ret.ToError()
}

func (z *ZesOverclock) GetControlCurrentValue(domainControl ZesOverclockControl) (float64, error) {
	var value float64
	ret := zesOverclockGetControlCurrentValue(z.handle, domainControl, &value)
	return value, ret.ToError()
}

func (z *ZesOverclock) GetControlPendingValue(domainControl ZesOverclockControl) (float64, error) {
	var value float64
	ret := zesOverclockGetControlPendingValue(z.handle, domainControl, &value)
	return value, ret.ToError()
}

func (z *ZesOverclock) SetControlUserValue(domainControl ZesOverclockControl, value float64) (ZesPendingAction, error) {
	var pendingAction ZesPendingAction
	ret := zesOverclockSetControlUserValue(z.handle, domainControl, value, &pendingAction)
	return pendingAction, ret.ToError()
}

func (z *ZesOverclock) GetControlState(domainControl ZesOverclockControl) (ZesControlState, ZesPendingAction, error) {
	var (
		state         ZesControlState
		pendingAction ZesPendingAction
	)
	ret := zesOverclockGetControlState(z.handle, domainControl, &state, &pendingAction)
	return state, pendingAction, ret.ToError()
}

func (z *ZesOverclock) GetVFPointValues(vfType ZesVfType, arrayType ZesVfArrayType, pointIndex uint32) (uint32, error) {
	var value uint32
	ret := zesOverclockGetVFPointValues(z.handle, vfType, arrayType, pointIndex, &value)
	return value, ret.ToError()
}

func (z *ZesOverclock) SetVFPointValues(vfType ZesVfType, pointIndex uint32, value uint32) error {
	ret := zesOverclockSetVFPointValues(z.handle, vfType, pointIndex, value)
	return ret.ToError()
}

func (z *ZeDevice) EnumDiagnosticTestSuites() ([]*ZesDiagnostics, error) {
	count := uint32(0)
	if ret := zesDeviceEnumDiagnosticTestSuites(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesDiagHandle, count)
	ret := zesDeviceEnumDiagnosticTestSuites(z.handle, &count, handles)
	return handlesToWrappers[zesDiagHandle, ZesDiagnostics](handles), ret.ToError()
}

func (z *ZesDiagnostics) GetProperties() (ZesDiagProperties, error) {
	var props ZesDiagProperties
	ret := zesDiagnosticsGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// TODO: static type for ZesDiagTest that contains golang strings
func (z *ZesDiagnostics) GetTests() ([]ZesDiagTest, error) {
	count := uint32(0)
	if ret := zesDiagnosticsGetTests(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	tests := make([]ZesDiagTest, count)
	ret := zesDiagnosticsGetTests(z.handle, &count, tests)
	return tests, ret.ToError()
}

func (z *ZesDiagnostics) RunTests(startIdx, endIdx uint32) ([]ZesDiagResult, error) {
	count := endIdx - startIdx + 1
	results := make([]ZesDiagResult, count)
	ret := zesDiagnosticsRunTests(z.handle, startIdx, endIdx, results)
	return results, ret.ToError()

}

func (z *ZeDevice) EnumEngineGroups() ([]*ZesEngine, error) {
	count := uint32(0)
	if ret := zesDeviceEnumEngineGroups(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesEngineHandle, count)
	ret := zesDeviceEnumEngineGroups(z.handle, &count, handles)
	return handlesToWrappers[zesEngineHandle, ZesEngine](handles), ret.ToError()
}

func (z *ZesEngine) GetProperties() (ZesEngineProperties, error) {
	var props ZesEngineProperties
	ret := zesEngineGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesEngine) GetActivity() (ZesEngineStats, error) {
	var stats ZesEngineStats
	ret := zesEngineGetActivity(z.handle, &stats)
	return stats, ret.ToError()
}

func (z *ZesEngine) GetActivityExt() ([]ZesEngineStats, error) {
	count := uint32(0)
	if ret := zesEngineGetActivityExt(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	stats := make([]ZesEngineStats, count)
	ret := zesEngineGetActivityExt(z.handle, &count, stats)
	return stats, ret.ToError()
}

func (z *ZeDevice) EnumFabricPorts() ([]*ZesFabricPort, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFabricPorts(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFabricPortHandle, count)
	ret := zesDeviceEnumFabricPorts(z.handle, &count, handles)
	return handlesToWrappers[zesFabricPortHandle, ZesFabricPort](handles), ret.ToError()
}

func (z *ZesFabricPort) GetProperties() (ZesFabricPortProperties, error) {
	var props ZesFabricPortProperties
	ret := zesFabricPortGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesFabricPort) GetLinkType() (ZesFabricLinkType, error) {
	var linkType ZesFabricLinkType
	ret := zesFabricPortGetLinkType(z.handle, &linkType)
	return linkType, ret.ToError()
}

func (z *ZesFabricPort) GetConfig() (ZesFabricPortConfig, error) {
	var config ZesFabricPortConfig
	ret := zesFabricPortGetConfig(z.handle, &config)
	return config, ret.ToError()
}

func (z *ZesFabricPort) SetConfig(config *ZesFabricPortConfig) error {
	ret := zesFabricPortSetConfig(z.handle, config)
	return ret.ToError()
}

func (z *ZesFabricPort) GetState() (ZesFabricPortState, error) {
	var state ZesFabricPortState
	ret := zesFabricPortGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *ZesFabricPort) GetThroughput() (ZesFabricPortThroughput, error) {
	var throughput ZesFabricPortThroughput
	ret := zesFabricPortGetThroughput(z.handle, &throughput)
	return throughput, ret.ToError()
}

func (z *ZesFabricPort) GetFabricErrorCounters() (ZesFabricPortErrorCounters, error) {
	var counters ZesFabricPortErrorCounters
	ret := zesFabricPortGetFabricErrorCounters(z.handle, &counters)
	return counters, ret.ToError()
}

func (z *ZeDevice) EnumFans() ([]*ZesFan, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFans(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFanHandle, count)
	ret := zesDeviceEnumFans(z.handle, &count, handles)
	return handlesToWrappers[zesFanHandle, ZesFan](handles), ret.ToError()
}

func (z *ZesFan) GetProperties() (ZesFanProperties, error) {
	var props ZesFanProperties
	ret := zesFanGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesFan) GetConfig() (ZesFanConfig, error) {
	var config ZesFanConfig
	ret := zesFanGetConfig(z.handle, &config)
	return config, ret.ToError()
}

func (z *ZesFan) SetDefaultMode() error {
	ret := zesFanSetDefaultMode(z.handle)
	return ret.ToError()
}

func (z *ZesFan) SetFixedSpeedMode(speed ZesFanSpeed) error {
	ret := zesFanSetFixedSpeedMode(z.handle, &speed)
	return ret.ToError()
}

func (z *ZesFan) SetSpeedTableMode(speedTable *ZesFanSpeedTable) error {
	ret := zesFanSetSpeedTableMode(z.handle, speedTable)
	return ret.ToError()
}

func (z *ZesFan) GetState(units ZesFanSpeedUnits) (int32, error) {
	var speed int32
	ret := zesFanGetState(z.handle, units, &speed)
	return speed, ret.ToError()
}

func (z *ZeDevice) EnumFirmwares() ([]*ZesFirmware, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFirmwares(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFirmwareHandle, count)
	ret := zesDeviceEnumFirmwares(z.handle, &count, handles)
	return handlesToWrappers[zesFirmwareHandle, ZesFirmware](handles), ret.ToError()
}

func (z *ZesFirmware) GetProperties() (ZesFirmwareProperties, error) {
	var props ZesFirmwareProperties
	ret := zesFirmwareGetProperties(z.handle, &props)
	return props, ret.ToError()
}

// TODO: what a good way to handle FW blobs, size??
func (z *ZesFirmware) Flash(firmwareImage []byte) error {
	ret := zesFirmwareFlash(z.handle, unsafe.Pointer(&firmwareImage[0]), uint32(len(firmwareImage)))
	return ret.ToError()
}

func (z *ZesFirmware) GetFlashProgress() (uint32, error) {
	var progress uint32
	ret := zesFirmwareGetFlashProgress(z.handle, &progress)
	return progress, ret.ToError()
}

func (z *ZesFirmware) GetConsoleLogs() (string, error) {
	var (
		size uint64
		logs string
	)
	// Get size
	ret := zesFirmwareGetConsoleLogs(z.handle, &size, nil)
	if ret != ZE_RESULT_SUCCESS {
		return "", ret.ToError()
	}
	if size == 0 {
		return "", nil
	}
	// Get logs
	buf := make([]byte, size)
	ret = zesFirmwareGetConsoleLogs(z.handle, &size, &buf[0])
	if ret != ZE_RESULT_SUCCESS {
		return "", ret.ToError()
	}
	logs = string(buf)
	return logs, nil
}

func (z *ZesFirmware) GetSecurityVersionExp() (string, error) {
	version := make([]byte, ZES_STRING_PROPERTY_SIZE)
	ret := zesFirmwareGetSecurityVersionExp(z.handle, &version[0])
	if ret != ZE_RESULT_SUCCESS {
		return "", ret.ToError()
	}
	return string(version), nil
}

func (z *ZesFirmware) SetSecurityVersionExp() error {
	ret := zesFirmwareSetSecurityVersionExp(z.handle)
	return ret.ToError()
}

func (z *ZeDevice) EnumFrequencyDomains() ([]*ZesFreq, error) {
	count := uint32(0)
	if ret := zesDeviceEnumFrequencyDomains(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesFreqHandle, count)
	ret := zesDeviceEnumFrequencyDomains(z.handle, &count, handles)
	return handlesToWrappers[zesFreqHandle, ZesFreq](handles), ret.ToError()
}

func (z *ZesFreq) GetProperties() (ZesFreqProperties, error) {
	var props ZesFreqProperties
	ret := zesFrequencyGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesFreq) GetAvailableClocks() ([]float64, error) {
	count := uint32(0)
	if ret := zesFrequencyGetAvailableClocks(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	clocks := make([]float64, count)
	ret := zesFrequencyGetAvailableClocks(z.handle, &count, clocks)
	return clocks, ret.ToError()
}

func (z *ZesFreq) GetRange() (ZesFreqRange, error) {
	var freqRange ZesFreqRange
	ret := zesFrequencyGetRange(z.handle, &freqRange)
	return freqRange, ret.ToError()
}

func (z *ZesFreq) SetRange(freqRange *ZesFreqRange) error {
	ret := zesFrequencySetRange(z.handle, freqRange)
	return ret.ToError()
}

func (z *ZesFreq) GetState() (ZesFreqState, error) {
	var state ZesFreqState
	ret := zesFrequencyGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *ZesFreq) GetThrottleTime() (ZesFreqThrottleTime, error) {
	var throttleTime ZesFreqThrottleTime
	ret := zesFrequencyGetThrottleTime(z.handle, &throttleTime)
	return throttleTime, ret.ToError()
}

func (z *ZeDevice) EnumLeds() ([]*ZesLed, error) {
	count := uint32(0)
	if ret := zesDeviceEnumLeds(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesLedHandle, count)
	ret := zesDeviceEnumLeds(z.handle, &count, handles)
	return handlesToWrappers[zesLedHandle, ZesLed](handles), ret.ToError()
}

func (z *ZesLed) GetProperties() (ZesLedProperties, error) {
	var props ZesLedProperties
	ret := zesLedGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesLed) GetState() (ZesLedState, error) {
	var state ZesLedState
	ret := zesLedGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *ZesLed) SetState(enable bool) error {
	ret := zesLedSetState(z.handle, boolToByte(enable))
	return ret.ToError()
}

func (z *ZesLed) SetColor(color ZesLedColor) error {
	ret := zesLedSetColor(z.handle, &color)
	return ret.ToError()
}

func (z *ZeDevice) EnumMemoryModules() ([]*ZesMem, error) {
	count := uint32(0)
	if ret := zesDeviceEnumMemoryModules(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesMemHandle, count)
	ret := zesDeviceEnumMemoryModules(z.handle, &count, handles)
	return handlesToWrappers[zesMemHandle, ZesMem](handles), ret.ToError()
}

func (z *ZesMem) GetProperties() (ZesMemProperties, error) {
	var props ZesMemProperties
	ret := zesMemoryGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesMem) GetState() (ZesMemState, error) {
	var state ZesMemState
	ret := zesMemoryGetState(z.handle, &state)
	return state, ret.ToError()
}

func (z *ZesMem) GetBandwidth() (ZesMemBandwidth, error) {
	var bandwidth ZesMemBandwidth
	ret := zesMemoryGetBandwidth(z.handle, &bandwidth)
	return bandwidth, ret.ToError()
}

func (z *ZeDevice) EnumPerformanceFactorDomains() ([]*ZesPerf, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPerformanceFactorDomains(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPerfHandle, count)
	ret := zesDeviceEnumPerformanceFactorDomains(z.handle, &count, handles)
	return handlesToWrappers[zesPerfHandle, ZesPerf](handles), ret.ToError()
}

func (z *ZesPerf) GetProperties() (ZesPerfProperties, error) {
	var props ZesPerfProperties
	ret := zesPerformanceFactorGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesPerf) GetConfig() (float64, error) {
	var factor float64
	ret := zesPerformanceFactorGetConfig(z.handle, &factor)
	return factor, ret.ToError()
}

func (z *ZesPerf) SetConfig(factor float64) error {
	ret := zesPerformanceFactorSetConfig(z.handle, factor)
	return ret.ToError()
}

func (z *ZeDevice) EnumPowerDomains() ([]*ZesPwr, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPowerDomains(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPwrHandle, count)
	ret := zesDeviceEnumPowerDomains(z.handle, &count, handles)
	return handlesToWrappers[zesPwrHandle, ZesPwr](handles), ret.ToError()
}

func (z *ZesPwr) GetProperties() (ZesPowerProperties, error) {
	var props ZesPowerProperties
	ret := zesPowerGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesPwr) GetEnergyCounter() (ZesPowerEnergyCounter, error) {
	var counter ZesPowerEnergyCounter
	ret := zesPowerGetEnergyCounter(z.handle, &counter)
	return counter, ret.ToError()
}

func (z *ZesPwr) GetEnergyThreshold() (ZesEnergyThreshold, error) {
	var threshold ZesEnergyThreshold
	ret := zesPowerGetEnergyThreshold(z.handle, &threshold)
	return threshold, ret.ToError()
}

func (z *ZesPwr) SetEnergyThreshold(threshold float64) error {
	ret := zesPowerSetEnergyThreshold(z.handle, threshold)
	return ret.ToError()
}

func (z *ZesPwr) GetLimitsExt() ([]ZesPowerLimitExtDesc, error) {
	count := uint32(0)
	if ret := zesPowerGetLimitsExt(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	limits := make([]ZesPowerLimitExtDesc, count)
	ret := zesPowerGetLimitsExt(z.handle, &count, limits)
	return limits, ret.ToError()
}

func (z *ZesPwr) SetLimitsExt(limits []ZesPowerLimitExtDesc) error {
	count := uint32(len(limits))
	ret := zesPowerSetLimitsExt(z.handle, &count, limits)
	return ret.ToError()
}

func (z *ZeDevice) EnumPsus() ([]*ZesPsu, error) {
	count := uint32(0)
	if ret := zesDeviceEnumPsus(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zesPsuHandle, count)
	ret := zesDeviceEnumPsus(z.handle, &count, handles)
	return handlesToWrappers[zesPsuHandle, ZesPsu](handles), ret.ToError()
}

func (z *ZesPsu) GetProperties() (ZesPsuProperties, error) {
	var props ZesPsuProperties
	ret := zesPsuGetProperties(z.handle, &props)
	return props, ret.ToError()
}

func (z *ZesPsu) GetState() (ZesPsuState, error) {
	var state ZesPsuState
	ret := zesPsuGetState(z.handle, &state)
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
