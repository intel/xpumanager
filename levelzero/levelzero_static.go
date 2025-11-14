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
