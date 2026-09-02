// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package intel

import (
	"testing"
	"time"

	"github.com/intel/level-zero-go/core"
	th "github.com/intel/level-zero-go/internal/testhelper"
	"github.com/intel/level-zero-go/sysman"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Driver config paths (relative to this package)
const (
	driverConfigDefault       = th.ConfigDefault
	driverConfigNoExtension   = "testdata/no_extension.yaml"
	driverConfigNoInfoLogExt  = "testdata/no_info_log_extension.yaml"
	driverConfigExtVersionErr = "testdata/unsupported_extension_version.yaml"
	driverConfigResolveErr    = "testdata/error_resolve.yaml"
	driverConfigTranslateErr  = "testdata/error_translate.yaml"
	driverConfigDriverErrs    = "testdata/error_driver.yaml"
	driverConfigComponentErrs = "testdata/error_component.yaml"
)

// infoLogTimeOutReadAll is the read timeout that searches all the records that a collection instance holds
const infoLogTimeOutReadAll = -1 * time.Millisecond

// getDriver reloads the stub with the given driver config file and returns intel.Driver
func getDriver(t *testing.T, path string) *Driver {
	t.Helper()
	th.LoadConfig(t, path)
	return NewDriver(th.GetIndexed(t, "driver", 0, sysman.DriverGet))
}

func getInfoLog(t *testing.T, path string, idx int) *InfoLog {
	t.Helper()
	return th.GetIndexed(t, "component", idx, getDriver(t, path).EnumInfoLogs)
}

// getInfoLogInstance returns a collection instance of the default buffer of one info log
func getInfoLogInstance(t *testing.T, path string, idx int) *InfoLogInstance {
	t.Helper()
	instance, err := getInfoLog(t, path, idx).CreateInstance("", 0)
	require.NoError(t, err)
	return instance
}

func TestNewDriver(t *testing.T) {
	assert.Nil(t, NewDriver(nil))
}

func TestEnumInfoLogs(t *testing.T) {
	t.Run("ErrorNoExtension", func(t *testing.T) {
		_, err := getDriver(t, driverConfigNoExtension).EnumInfoLogs()
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorNoInfoLogExtension", func(t *testing.T) {
		// the extensions are resolved independently of each other
		_, err := getDriver(t, driverConfigNoInfoLogExt).EnumInfoLogs()
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorExtensionVersion", func(t *testing.T) {
		// the version advertised by the driver must be supported, too
		_, err := getDriver(t, driverConfigExtVersionErr).EnumInfoLogs()
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorResolve", func(t *testing.T) {
		_, err := getDriver(t, driverConfigResolveErr).EnumInfoLogs()
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorTranslate", func(t *testing.T) {
		_, err := getDriver(t, driverConfigTranslateErr).EnumInfoLogs()
		require.ErrorIs(t, err, core.RESULT_ERROR_UNINITIALIZED)
	})
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, err := getDriver(t, driverConfigDriverErrs).EnumInfoLogs()
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("Success", func(t *testing.T) {
		infoLogs, err := getDriver(t, driverConfigDefault).EnumInfoLogs()
		require.NoError(t, err)
		assert.Len(t, infoLogs, 2)
	})
}

func TestInfoLogGetProperties(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, err := getInfoLog(t, driverConfigComponentErrs, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("Success", func(t *testing.T) {
		props, err := getInfoLog(t, driverConfigDefault, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, InfoLogPropertiesExp{
			InfoLogType:                         INFO_LOG_TYPE_EXP_DEVICE,
			InfoLogFormat:                       INFO_LOG_FORMAT_CPER,
			IsNamedInstancedCollectionSupported: core.NewBool(true),
			IsPeekSupported:                     core.NewBool(true),
		}, props)
	})
}

func TestInfoLogCreateInstance(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, err := getInfoLog(t, driverConfigComponentErrs, 0).CreateInstance("", 0)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("SuccessDefaults", func(t *testing.T) {
		// zero values collect from the default buffer of the log, leaving the size
		// of the buffer as it is
		instance, err := getInfoLog(t, driverConfigDefault, 1).CreateInstance("", 0)
		require.NoError(t, err)
		assert.Zero(t, instance.BufferSizeInKb)
	})
	t.Run("SuccessRequestedBufferSize", func(t *testing.T) {
		// without a configured size the stub applies the requested one
		instance, err := getInfoLog(t, driverConfigDefault, 1).CreateInstance("", 6)
		require.NoError(t, err)
		assert.Equal(t, uint32(6), instance.BufferSizeInKb)
	})
	t.Run("SuccessAllArguments", func(t *testing.T) {
		// the driver reports the buffer size that it applied, which may differ from
		// the requested one
		instance, err := getInfoLog(t, driverConfigDefault, 0).CreateInstance("level-zero-go", 6)
		require.NoError(t, err)
		assert.Equal(t, uint32(8), instance.BufferSizeInKb)
	})
}

func TestInfoLogInstanceReadWithMetadata(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, _, err := getInfoLogInstance(t, driverConfigComponentErrs, 1).ReadWithMetadata(infoLogTimeOutReadAll)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("SuccessNothingPending", func(t *testing.T) {
		// nothing is read if the instance holds nothing, the status is reported anyway
		records, status, err := getInfoLogInstance(t, driverConfigDefault, 1).ReadWithMetadata(infoLogTimeOutReadAll)
		require.NoError(t, err)
		assert.Empty(t, records)
		assert.False(t, status.HasDataToRead.IsTrue())
		assert.Equal(t, uint32(5), status.DroppedRecordCount)
		assert.True(t, status.IsDroppedRecordCountValid.IsTrue())
	})
	t.Run("SuccessZeroTimeout", func(t *testing.T) {
		// returns immediately, without searching for records, the status still reporting that the instance holds records
		records, status, err := getInfoLogInstance(t, driverConfigDefault, 0).ReadWithMetadata(0)
		require.NoError(t, err)
		assert.Empty(t, records)
		assert.True(t, status.HasDataToRead.IsTrue())
	})
	t.Run("Success", func(t *testing.T) {
		// all the records read in one go
		instance := getInfoLogInstance(t, driverConfigDefault, 0)
		records, status, err := instance.ReadWithMetadata(infoLogTimeOutReadAll)
		require.NoError(t, err)
		require.Len(t, records, 2)
		assert.Equal(t, "CPER-RECORD-1", string(records[0].Data))
		assert.Equal(t, "CPER-RECORD-2", string(records[1].Data))
		// NOTE: the stub always returns the status as configured, also when it returned every record that it holds
		assert.True(t, status.HasDataToRead.IsTrue())
		// the records dropped since the previous read are reported by both the query and the read call
		assert.Equal(t, uint32(4), status.DroppedRecordCount)
		assert.True(t, status.IsDroppedRecordCountValid.IsTrue())

		meta := records[0].Metadata
		assert.Equal(t, sysman.PciAddress{Bus: 1, Device: 2, Function: 3}, meta.Address)
		assert.Equal(t, "12345678-abcd-ef01-2345-6789abcdef01", meta.Uuid.Id.String())
		assert.Equal(t, uint64(1234567), meta.Timestamp)
		assert.Equal(t, INFO_LOG_RECORD_TYPE_EXP_ERROR_CORRECTED, meta.RecordType)
		assert.Equal(t, uint32(len("CPER-RECORD-1")), meta.LengthOfData)
		assert.Zero(t, meta.Offset)
		assert.Equal(t, uint32(len("CPER-RECORD-1")), records[1].Metadata.Offset)

		// NOTE: the stub does not consume the records, so they are read again here
		records, _, err = instance.ReadWithMetadata(infoLogTimeOutReadAll)
		require.NoError(t, err)
		assert.Len(t, records, 2)
	})
}

func TestInfoLogInstanceCollect(t *testing.T) {
	const data = "CPER-RECORD-1"
	// The stub reports the same result and status for the query and the read
	// call, so a fake collect function is used to make them differ.
	fake := func(query InfoLogReadStatusExp, readRet core.Result, read InfoLogReadStatusExp,
		offset uint32) infoLogCollectFunc {
		return func(_ infoLogInstanceHandle, _ uint64, pSize *uint32, buf []byte, pCount *uint32,
			metadata []InfoLogMetadataExp, status *InfoLogReadStatusExp) core.Result {
			if *pSize == 0 || *pCount == 0 {
				*pSize, *pCount = uint32(len(data)), 1
				*status = query
				return core.RESULT_SUCCESS
			}
			copy(buf, data)
			metadata[0].Offset = offset
			metadata[0].LengthOfData = uint32(len(data))
			*status = read
			return readRet
		}
	}
	valid := func(dropped uint32) InfoLogReadStatusExp {
		return InfoLogReadStatusExp{DroppedRecordCount: dropped, IsDroppedRecordCountValid: core.NewBool(true)}
	}

	t.Run("Success", func(t *testing.T) {
		// the counts of the query and the read add up
		records, status, err := (&InfoLogInstance{}).collect(time.Second, fake(valid(3), core.RESULT_SUCCESS, valid(2), 0))
		require.NoError(t, err)
		require.Len(t, records, 1)
		assert.Equal(t, data, string(records[0].Data))
		assert.Equal(t, uint32(5), status.DroppedRecordCount)
		assert.True(t, status.IsDroppedRecordCountValid.IsTrue())
	})
	t.Run("QueryCountIncomplete", func(t *testing.T) {
		// the driver reports what the query could not count with the read, i.e.
		// the sum is complete when the count of the read is
		_, status, err := (&InfoLogInstance{}).collect(time.Second,
			fake(InfoLogReadStatusExp{}, core.RESULT_SUCCESS, valid(5), 0))
		require.NoError(t, err)
		assert.Equal(t, uint32(5), status.DroppedRecordCount)
		assert.True(t, status.IsDroppedRecordCountValid.IsTrue())
	})
	t.Run("ReadCountIncomplete", func(t *testing.T) {
		// what the read could not count is reported by a later call
		_, status, err := (&InfoLogInstance{}).collect(time.Second,
			fake(valid(3), core.RESULT_SUCCESS, InfoLogReadStatusExp{}, 0))
		require.NoError(t, err)
		assert.Equal(t, uint32(3), status.DroppedRecordCount)
		assert.False(t, status.IsDroppedRecordCountValid.IsTrue())
	})
	t.Run("ReadError", func(t *testing.T) {
		// the status is returned also when the read fails
		records, status, err := (&InfoLogInstance{}).collect(time.Second,
			fake(valid(3), core.RESULT_ERROR_NOT_AVAILABLE, valid(2), 0))
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
		assert.Empty(t, records)
		assert.Equal(t, uint32(5), status.DroppedRecordCount)
	})
	t.Run("ErrorRecordOutsideData", func(t *testing.T) {
		_, _, err := (&InfoLogInstance{}).collect(time.Second, fake(valid(0), core.RESULT_SUCCESS, valid(0), 1))
		require.ErrorIs(t, err, core.RESULT_ERROR_UNKNOWN)
	})
}

func TestInfoLogInstancePeekWithMetadata(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, _, err := getInfoLogInstance(t, driverConfigComponentErrs, 1).PeekWithMetadata(infoLogTimeOutReadAll)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("Success", func(t *testing.T) {
		// peeking does not consume the records, they are read again in the next iteration
		instance := getInfoLogInstance(t, driverConfigDefault, 0)
		for i := 0; i < 2; i++ {
			records, status, err := instance.PeekWithMetadata(infoLogTimeOutReadAll)
			require.NoError(t, err)
			require.Len(t, records, 2)
			assert.Equal(t, "CPER-RECORD-1", string(records[0].Data))
			assert.Equal(t, "CPER-RECORD-2", string(records[1].Data))
			assert.True(t, status.HasDataToRead.IsTrue())
		}
	})
}

func TestInfoLogInstanceDelete(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		err := getInfoLogInstance(t, driverConfigComponentErrs, 1).Delete()
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("Success", func(t *testing.T) {
		require.NoError(t, getInfoLogInstance(t, driverConfigDefault, 0).Delete())
	})
}

func TestDriverEventRegister(t *testing.T) {
	t.Run("ErrorNoExtension", func(t *testing.T) {
		err := getDriver(t, driverConfigNoExtension).EventRegister(CPER_DATA_AVAILABLE)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorExtensionVersion", func(t *testing.T) {
		// the version advertised by the driver must be supported, too
		err := getDriver(t, driverConfigExtVersionErr).EventRegister(CPER_DATA_AVAILABLE)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorResolve", func(t *testing.T) {
		err := getDriver(t, driverConfigResolveErr).EventRegister(CPER_DATA_AVAILABLE)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorTranslate", func(t *testing.T) {
		err := getDriver(t, driverConfigTranslateErr).EventRegister(CPER_DATA_AVAILABLE)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNINITIALIZED)
	})
	t.Run("ErrorFromDriver", func(t *testing.T) {
		err := getDriver(t, driverConfigDriverErrs).EventRegister(CPER_DATA_AVAILABLE)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("ErrorDeviceScopedEvent", func(t *testing.T) {
		// only driver scoped events can be registered
		deviceEvent := sysman.EventTypeFlags(sysman.EVENT_TYPE_FLAG_DEVICE_DETACH)
		err := getDriver(t, driverConfigDefault).EventRegister(deviceEvent)
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ENUMERATION)
	})
	t.Run("Success", func(t *testing.T) {
		driver := getDriver(t, driverConfigDefault)
		require.NoError(t, driver.EventRegister(CPER_DATA_AVAILABLE))
		// an empty set is accepted, it clears the registered events
		require.NoError(t, driver.EventRegister(0))
	})
}

func TestDriverEventListenExp(t *testing.T) {
	t.Run("ErrorNoExtension", func(t *testing.T) {
		_, _, _, err := getDriver(t, driverConfigNoExtension).EventListenExp(0, nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorResolve", func(t *testing.T) {
		_, _, _, err := getDriver(t, driverConfigResolveErr).EventListenExp(0, nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNSUPPORTED_FEATURE)
	})
	t.Run("ErrorTranslate", func(t *testing.T) {
		_, _, _, err := getDriver(t, driverConfigTranslateErr).EventListenExp(0, nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNINITIALIZED)
	})
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, _, _, err := getDriver(t, driverConfigDriverErrs).EventListenExp(0, nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("SuccessNoDevices", func(t *testing.T) {
		// listening without devices only reports the driver scoped events
		driver := getDriver(t, driverConfigDefault)
		require.NoError(t, driver.EventRegister(CPER_DATA_AVAILABLE))

		hasDeviceEvents, deviceEvents, driverEvents, err := driver.EventListenExp(0, nil)
		require.NoError(t, err)
		assert.False(t, hasDeviceEvents)
		assert.Empty(t, deviceEvents)
		assert.Equal(t, sysman.EventTypeFlags(CPER_DATA_AVAILABLE), driverEvents)
	})
	t.Run("SuccessWithDevices", func(t *testing.T) {
		// both the device and the driver scoped events are reported
		driver := getDriver(t, driverConfigDefault)
		devices, err := driver.DeviceGet()
		require.NoError(t, err)
		require.NoError(t, driver.EventRegister(CPER_DATA_AVAILABLE))

		hasDeviceEvents, deviceEvents, driverEvents, err := driver.EventListenExp(0, devices)
		require.NoError(t, err)
		assert.True(t, hasDeviceEvents)
		deviceEvent := sysman.EventTypeFlags(sysman.EVENT_TYPE_FLAG_DEVICE_DETACH)
		assert.Equal(t, []sysman.EventTypeFlags{deviceEvent}, deviceEvents)
		assert.Equal(t, sysman.EventTypeFlags(CPER_DATA_AVAILABLE), driverEvents)
	})
}
