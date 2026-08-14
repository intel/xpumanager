// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package intel

//go:generate ../../../hack/generate-stringer.sh

import (
	"fmt"
	"sync"
	"time"

	"github.com/intel/level-zero-go/core"
	"github.com/intel/level-zero-go/internal"
	"github.com/intel/level-zero-go/sysman"
)

// resolveLock guards the function pointer table of the shim which is global
var resolveLock sync.Mutex

// resolveFunctions makes the functions of one extension of the backend driver
// available to the shim. The extension must be advertised by the driver in a
// version that supports the given version. The functions of an extension are
// resolved only once on the first call.
func (z *Driver) resolveFunctions(state *extensionState, extension string, version uint32,
	resolve func(driverHandle) core.Result) core.Result {
	resolveLock.Lock()
	defer resolveLock.Unlock()

	if !state.resolved {
		state.ret = core.RESULT_ERROR_UNSUPPORTED_FEATURE
		if z.HasExtension(extension, version) {
			state.ret = resolve(z.handle)
		}
		state.resolved = true
	}
	return state.ret
}

// resolveInfoLogFunctions makes the info log extension functions of the backend driver available to the shim.
func (z *Driver) resolveInfoLogFunctions() core.Result {
	return z.resolveFunctions(&z.infoLog, DRIVER_INFO_LOGS_EXP_NAME,
		uint32(DRIVER_INFO_LOGS_EXP_VERSION_CURRENT), infoLogResolveFunctions)
}

// resolveDriverEventFunctions makes the driver scoped event extension functions of the backend driver available to the shim.
func (z *Driver) resolveDriverEventFunctions() core.Result {
	return z.resolveFunctions(&z.driverEvent, DRIVER_EVENT_EXP_NAME,
		uint32(DRIVER_EVENT_EXP_VERSION_CURRENT), driverEventResolveFunctions)
}

// EnumInfoLogs wraps the (experimental) zesIntelDriverEnumInfoLogsExp function
// declared in include/intel/zes_intel_gpu_sysman.h.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *Driver) EnumInfoLogs() ([]*InfoLog, error) {
	if ret := z.resolveInfoLogFunctions(); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}

	count := uint32(0)
	if ret := zesIntelDriverEnumInfoLogsExp(z.handle, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]infoLogHandle, count)
	ret := zesIntelDriverEnumInfoLogsExp(z.handle, &count, handles)

	infoLogs := make([]*InfoLog, len(handles))
	for i, handle := range handles {
		infoLogs[i] = &InfoLog{handle: handle}
	}
	return infoLogs, ret.ToError()
}

// GetProperties wraps the (experimental) zesIntelInfoLogGetPropertiesExp
// function declared in include/intel/zes_intel_gpu_sysman.h.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLog) GetProperties() (InfoLogPropertiesExp, error) {
	props := InfoLogPropertiesExp{stype: _STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP}
	ret := zesIntelInfoLogGetPropertiesExp(z.handle, &props)
	return props, ret.ToError()
}

// Read wraps the (experimental) zesIntelInfoLogReadExp function declared in
// include/intel/zes_intel_gpu_sysman.h.
//
// It reads pending info log records into the buffer and returns the number of
// bytes read. Reading consumes the records, i.e. subsequent calls do not
// return the same data again. Only whole records are read. As many pending
// records as fit are copied into buffer and nothing is read if the next
// pending record does not fit into the buffer. Thus, zero bytes read does not
// necessarily mean that the log is empty. Use ReadAll or a buffer of size
// InfoLogPropertiesExp.MaxSize to always read all pending records.
//
// The core.RESULT_WARNING_DROPPED_DATA error means that one record did not fit
// in the buffer and was lost. The data that was read is valid (and consumed),
// i.e. it should be processed despite the error. Reading with a buffer of size
// InfoLogPropertiesExp.MaxSize or using InfoLog.ReadAll() avoids this.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLog) Read(buf []byte) (int, error) {
	size := uint32(len(buf))
	ret := zesIntelInfoLogReadExp(z.handle, &size, buf)
	if ret != core.RESULT_SUCCESS && ret != core.RESULT_WARNING_DROPPED_DATA {
		return 0, ret.ToError()
	}
	return int(size), ret.ToError()
}

// ReadAll reads all pending info log records, see Read for the semantics of
// reading. The read buffer is sized according to the MaxSize property of the
// info log, i.e. the size of the log buffer of the driver, so that the read
// cannot be cut short because of a too small buffer.
//
// A core.RESULT_ERROR_NOT_AVAILABLE error is returned if the driver does not
// report the maximum size of the log.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLog) ReadAll() ([]byte, error) {
	props, err := z.GetProperties()
	if err != nil {
		return nil, err
	}
	if props.MaxSize == 0 {
		return nil, core.RESULT_ERROR_NOT_AVAILABLE
	}
	// MaxSize is in kilobytes
	buf := make([]byte, int(props.MaxSize)*1024)
	n, err := z.Read(buf)
	return buf[:n], err
}

// ReadWithMetadata wraps the (experimental) zesIntelInfoLogReadWithMetadataExp
// function declared in include/intel/zes_intel_gpu_sysman.h.
//
// It reads the pending info log records with their metadata. Reading consumes the
// records, i.e. subsequent calls do not return the same records again.
//
// The core.RESULT_WARNING_DROPPED_DATA error means that one record did not fit
// in the read buffer and was lost. Only that one record is lost, the records
// that follow it are left pending, and the ones that were read are valid (and
// consumed), i.e. they are returned despite the error.
//
// A core.RESULT_ERROR_UNKNOWN error means that the driver located a record
// outside the data that it read, i.e. the metadata does not match the data.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLog) ReadWithMetadata() ([]InfoLogRecord, error) {
	// Without the output buffers the driver only reports what is pending
	var size, count uint32
	if ret := zesIntelInfoLogReadWithMetadataExp(z.handle, &size, nil, &count, nil); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	if count == 0 || size == 0 {
		return nil, nil
	}

	// NOTE: what the driver reports as pending is a non-consuming snapshot of
	// its trace buffer, while the read below consumes it, i.e. records may
	// arrive in between and the buffers sized here do not account for them.
	// That is harmless (as long as the driver doesn't read beyond the
	// requested size).
	buf := make([]byte, size)
	metadata := make([]InfoLogMetadataExp, count)
	for i := range metadata {
		metadata[i].stype = _STRUCTURE_TYPE_INFO_LOG_METADATA_EXP
	}

	// NOTE: the data read is valid (and consumed) even if the driver had to drop records
	ret := zesIntelInfoLogReadWithMetadataExp(z.handle, &size, buf, &count, metadata)
	if ret != core.RESULT_SUCCESS && ret != core.RESULT_WARNING_DROPPED_DATA {
		return nil, ret.ToError()
	}

	// The Offset and LengthOfData fields of the metadata locate each record in buf
	records := make([]InfoLogRecord, count)
	for i, meta := range metadata[:count] {
		end := int(meta.Offset) + int(meta.LengthOfData)
		if end > int(size) {
			return nil, fmt.Errorf(
				"invalid info log metadata from the driver: record %d at offset %d with length %d is outside the %d bytes read: %w",
				i, meta.Offset, meta.LengthOfData, size, core.RESULT_ERROR_UNKNOWN)
		}
		records[i] = InfoLogRecord{Metadata: meta, Data: buf[meta.Offset:end]}
	}
	return records, ret.ToError()
}

// Enable wraps the (experimental) zesIntelInfoLogEnableExp function declared in
// include/intel/zes_intel_gpu_sysman.h.
//
// It enables the collection of the info log records into the tracefs instance
// named by instanceName, using a per-CPU trace buffer of bufferSizeInKb kilobytes
// and waking up the listeners of the CPER_DATA_AVAILABLE event when the buffer is
// percentFullThreshold percent full. An empty instanceName selects the global
// trace buffer and a zero value the default of the driver.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLog) Enable(instanceName string, bufferSizeInKb, percentFullThreshold uint32) error {
	var name []byte
	if instanceName != "" {
		// The driver expects a C string
		name = append([]byte(instanceName), 0)
	}

	// A nil pointer selects the default of the driver, which is what zero means here
	var pBufferSizeInKb, pPercentFullThreshold *uint32
	if bufferSizeInKb != 0 {
		pBufferSizeInKb = &bufferSizeInKb
	}
	if percentFullThreshold != 0 {
		pPercentFullThreshold = &percentFullThreshold
	}

	ret := zesIntelInfoLogEnableExp(z.handle, name, pBufferSizeInKb, pPercentFullThreshold)
	return ret.ToError()
}

// Disable wraps the (experimental) zesIntelInfoLogDisableExp function declared in
// include/intel/zes_intel_gpu_sysman.h.
//
// It disables the collection of the info log records, tearing down the named
// tracefs instance that Enable created, if any.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLog) Disable() error {
	ret := zesIntelInfoLogDisableExp(z.handle)
	return ret.ToError()
}

// EventRegister wraps the (experimental) zesIntelDriverEventRegisterExp function
// declared in include/intel/zes_intel_gpu_sysman.h.
//
// It registers the driver scoped events, e.g. CPER_DATA_AVAILABLE, that
// EventListenExp reports. The events are driver scoped, i.e. not tied to a
// device: the underlying data source is shared by all devices of the driver.
// Only driver scoped events are accepted, the standard event flags must be
// registered per device with sysman.Device.EventRegister.
//
// CPER_DATA_AVAILABLE requires the collection of the info log records to have
// been enabled with InfoLog.Enable before listening.
//
// NOTE: this function itself may be called from simultaneous threads, but
// listening for CPER_DATA_AVAILABLE must be serialized with the info log calls,
// see the note on the InfoLog type.
func (z *Driver) EventRegister(events sysman.EventTypeFlags) error {
	if ret := z.resolveDriverEventFunctions(); ret != core.RESULT_SUCCESS {
		return ret.ToError()
	}

	ret := zesIntelDriverEventRegisterExp(z.handle, uint32(events))
	return ret.ToError()
}

// EventListenExp wraps the (experimental) zesIntelDriverEventListenExp function
// declared in include/intel/zes_intel_gpu_sysman.h.
//
// It extends sysman.Driver.EventListenEx with the driver scoped events
// registered with EventRegister, returned as the third return value. The device
// scoped events are returned per device, in the order of the devices argument.
//
// NOTE: Unlike the stable EventListenEx, the first return value only tells
// whether any of the devices had events, i.e. it is not a count and the returned
// per-device events must be scanned to find them. A driver scoped event can
// occur without any device scoped event, so both return values must be checked.
//
// The devices argument may be empty to listen for driver scoped events only. A
// negative timeout means no timeout, i.e. the call blocks until an event occurs.
// The function must not be called from simultaneous threads with the same driver.
// Listening for CPER_DATA_AVAILABLE must also be serialized with the info log
// calls, see the note on the InfoLog type.
func (z *Driver) EventListenExp(timeout time.Duration,
	devices []*sysman.Device) (bool, []sysman.EventTypeFlags, sysman.EventTypeFlags, error) {
	if ret := z.resolveDriverEventFunctions(); ret != core.RESULT_SUCCESS {
		return false, nil, 0, ret.ToError()
	}

	handles := make([]deviceHandle, len(devices))
	for i, device := range devices {
		handles[i] = deviceHandle(device.Handle())
	}

	var numDeviceEvents, driverEvents uint32
	events := make([]uint32, len(handles))
	ms := internal.DurationToMillisecondsUint64(timeout)
	ret := zesIntelDriverEventListenExp(z.handle, ms, uint32(len(handles)), handles, &numDeviceEvents, events,
		&driverEvents)

	deviceEvents := make([]sysman.EventTypeFlags, len(events))
	for i, event := range events {
		deviceEvents[i] = sysman.EventTypeFlags(event)
	}

	return numDeviceEvents != 0, deviceEvents, sysman.EventTypeFlags(driverEvents), ret.ToError()
}
