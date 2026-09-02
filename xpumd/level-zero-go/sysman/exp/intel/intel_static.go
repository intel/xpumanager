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
// It returns core.RESULT_ERROR_UNSUPPORTED_FEATURE if the driver does not
// advertise the info logs extension in a supported version, or if its functions
// cannot be resolved.
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

// CreateInstance wraps the (experimental) zesIntelInfoLogCreateInstanceExp
// function declared in include/intel/zes_intel_gpu_sysman.h.
//
// It creates a collection instance of the info log, which collects the records
// for the caller to read with InfoLogInstance.ReadWithMetadata. An empty
// instanceName selects the default buffer of the info log, which may be shared
// with other consumers and carry data from other sources. A named instance
// collects into a buffer of its own, which requires the
// IsNamedInstancedCollectionSupported property, and holds the name until it is
// deleted: creating another instance with the same name, in this or in another
// process, fails with core.RESULT_ERROR_HANDLE_OBJECT_IN_USE.
//
// The bufferSizeInKb argument requests the total size of the collection
// buffer, in kilobytes, a zero value keeps the current size. The size that the
// driver actually applied is reported in the BufferSizeInKb field of the
// instance.
//
// The instance must be deleted with InfoLogInstance.Delete when it is not
// needed anymore.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLog) CreateInstance(instanceName string, bufferSizeInKb uint32) (*InfoLogInstance, error) {
	var name []byte
	if instanceName != "" {
		// The driver expects a C string
		name = append([]byte(instanceName), 0)
	}

	// NOTE: the driver reports the size that it applied back in bufferSizeInKb
	var handle infoLogInstanceHandle
	if ret := zesIntelInfoLogCreateInstanceExp(z.handle, name, &bufferSizeInKb, &handle); ret != core.RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	return &InfoLogInstance{handle: handle, BufferSizeInKb: bufferSizeInKb}, nil
}

type infoLogCollectFunc func(infoLogInstanceHandle, uint64, *uint32, []byte, *uint32, []InfoLogMetadataExp,
	*InfoLogReadStatusExp) core.Result

// collect asks the collection instance what it holds and then reads (or peeks) that
func (z *InfoLogInstance) collect(timeout time.Duration,
	collect infoLogCollectFunc) ([]InfoLogRecord, InfoLogReadStatusExp, error) {
	ms := internal.DurationToMillisecondsUint64(timeout)

	// With no output buffer it only reports what the instance holds
	var size, count uint32
	status := InfoLogReadStatusExp{stype: _STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP}
	if ret := collect(z.handle, ms, &size, nil, &count, nil, &status); ret != core.RESULT_SUCCESS {
		return nil, status, ret.ToError()
	}
	if count == 0 || size == 0 {
		return nil, status, nil
	}

	// NOTE: what the query above reports is a non-consuming snapshot, while the
	// call below consumes what it returned. I.e. records may arrive in between
	// and the buffers sized here do not account for them. That is harmless, a
	// record that does not fit is not consumed but left for the next call, which
	// the HasDataToRead field of the read status reports.
	buf := make([]byte, size)
	metadata := make([]InfoLogMetadataExp, count)
	for i := range metadata {
		metadata[i].stype = _STRUCTURE_TYPE_INFO_LOG_METADATA_EXP
	}

	// NOTE: the dropped record count is reported since the previous call, the
	// query above included, so the counts of both calls add up to the one for
	// the interval. A count that the query above could not determine is
	// reported by the read, i.e. status.IsDroppedRecordCountValid from the
	// read is correct.
	queryStatus := status
	status = InfoLogReadStatusExp{stype: _STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP}
	ret := collect(z.handle, ms, &size, buf, &count, metadata, &status)
	status.DroppedRecordCount += queryStatus.DroppedRecordCount
	if ret != core.RESULT_SUCCESS {
		return nil, status, ret.ToError()
	}

	// The Offset and LengthOfData fields of the metadata locate each record in buf
	records := make([]InfoLogRecord, count)
	for i, meta := range metadata[:count] {
		end := int(meta.Offset) + int(meta.LengthOfData)
		if end > int(size) {
			return nil, status,
				fmt.Errorf("invalid info log metadata from the driver: record %d at offset %d with length %d is outside the %d bytes read: %w",
					i, meta.Offset, meta.LengthOfData, size, core.RESULT_ERROR_UNKNOWN)
		}
		records[i] = InfoLogRecord{Metadata: meta, Data: buf[meta.Offset:end]}
	}
	return records, status, nil
}

// ReadWithMetadata wraps the (experimental)
// zesIntelInfoLogInstanceReadWithMetadataExp function declared in
// include/intel/zes_intel_gpu_sysman.h.
//
// It reads the records that the collection instance holds, with their metadata,
// and the status of the read. Reading consumes the records, i.e. subsequent
// calls do not return the same records again.
//
// The timeout bounds the search for the records, it is not a wait for records to
// arrive. The driver returns as soon as it has searched what the instance holds.
// A zero timeout returns immediately, without searching, and a negative one
// searches everything that the instance holds (mapped to UINT64_MAX by
// internal.DurationToMillisecondsUint64).
//
// The HasDataToRead field of the status tells whether the instance still has
// records left, e.g. because one of them did not fit in the read buffer, i.e.
// whether it is worth reading again. The DroppedRecordCount field counts the
// records dropped since the previous call of this method or of
// PeekWithMetadata, even though the driver is called twice, see collect.
//
// A core.RESULT_ERROR_UNKNOWN error means that the driver located a record
// outside the data that it read, i.e. the metadata does not match the data.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLogInstance) ReadWithMetadata(timeout time.Duration) ([]InfoLogRecord, InfoLogReadStatusExp, error) {
	return z.collect(timeout, zesIntelInfoLogInstanceReadWithMetadataExp)
}

// PeekWithMetadata wraps the (experimental)
// zesIntelInfoLogInstancePeekWithMetadataExp function declared in
// include/intel/zes_intel_gpu_sysman.h.
//
// It is ReadWithMetadata without consuming the records: they are handed out
// again by the calls that follow. Peeking requires the IsPeekSupported property,
// otherwise it fails with core.RESULT_ERROR_UNSUPPORTED_FEATURE.
//
// A core.RESULT_ERROR_UNKNOWN error means that the driver located a record
// outside the data that it read, i.e. the metadata does not match the data.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLogInstance) PeekWithMetadata(timeout time.Duration) ([]InfoLogRecord, InfoLogReadStatusExp, error) {
	return z.collect(timeout, zesIntelInfoLogInstancePeekWithMetadataExp)
}

// Delete wraps the (experimental) zesIntelInfoLogInstanceDeleteExp function
// declared in include/intel/zes_intel_gpu_sysman.h.
//
// It stops the collection into the instance and releases the resources of the
// instance, including the name that it held, if any. The records that the
// instance has not handed out are lost. No other call may be in progress on the
// instance, and it must not be used anymore after this, not even to delete it
// again.
//
// NOTE: not thread-safe, see the note on the InfoLog type.
func (z *InfoLogInstance) Delete() error {
	ret := zesIntelInfoLogInstanceDeleteExp(z.handle)
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
// CPER_DATA_AVAILABLE requires a collection instance of the info log to have
// been created with InfoLog.CreateInstance before listening.
//
// Calling this function while another thread is blocked in EventListenExp
// updates that call, so an event registered after a listen has started can
// still be reported by it.
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
