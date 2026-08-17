// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package intel

import (
	"testing"

	"github.com/intel/level-zero-go/core"
	th "github.com/intel/level-zero-go/internal/testhelper"
	"github.com/intel/level-zero-go/sysman"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Driver config paths (relative to this package)
const (
	driverConfigDefault        = th.ConfigDefault
	driverConfigNoExtension    = "testdata/no_extension.yaml"
	driverConfigNoInfoLogExt   = "testdata/no_info_log_extension.yaml"
	driverConfigExtVersionErr  = "testdata/unsupported_extension_version.yaml"
	driverConfigResolveErr     = "testdata/error_resolve.yaml"
	driverConfigTranslateErr   = "testdata/error_translate.yaml"
	driverConfigDriverErrs     = "testdata/error_driver.yaml"
	driverConfigComponentErrs  = "testdata/error_component.yaml"
	driverConfigNoMaxSize      = "testdata/no_maxsize.yaml"
	driverConfigWarningDropped = "testdata/warning_dropped.yaml"
)

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
			InfoLogType:                    INFO_LOG_TYPE_EXP_DEVICE,
			InfoLogFormat:                  INFO_LOG_FORMAT_CPER,
			MaxSize:                        1,
			IsInstancedCollectionSupported: 1,
		}, props)
	})
}

func TestInfoLogRead(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, err := getInfoLog(t, driverConfigComponentErrs, 0).Read(make([]byte, 16))
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("ErrorEmptyBuffer", func(t *testing.T) {
		_, err := getInfoLog(t, driverConfigDefault, 0).Read(nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("SuccessTooSmallBuffer", func(t *testing.T) {
		// only whole records are read, nothing is returned if the buffer cannot
		// hold one
		infoLog := getInfoLog(t, driverConfigDefault, 0)
		buf := make([]byte, 4)
		n, err := infoLog.Read(buf)
		require.NoError(t, err)
		assert.Zero(t, n)
	})
	t.Run("SuccessOneRecord", func(t *testing.T) {
		// only one record is returned as the buffer below has room for one only
		infoLog := getInfoLog(t, driverConfigDefault, 0)
		buf := make([]byte, 16)
		n, err := infoLog.Read(buf)
		require.NoError(t, err)
		assert.Equal(t, "CPER-RECORD-1", string(buf[:n]))
	})
	t.Run("SuccessMultipleRecords", func(t *testing.T) {
		infoLog := getInfoLog(t, driverConfigDefault, 0)
		buf := make([]byte, 4096)
		n, err := infoLog.Read(buf)
		require.NoError(t, err)
		assert.Equal(t, "CPER-RECORD-1CPER-RECORD-2", string(buf[:n]))
	})
	t.Run("WarningDropped", func(t *testing.T) {
		// the records that were read are returned despite the warning
		infoLog := getInfoLog(t, driverConfigWarningDropped, 0)
		buf := make([]byte, 16)
		n, err := infoLog.Read(buf)
		require.ErrorIs(t, err, core.RESULT_WARNING_DROPPED_DATA)
		assert.Equal(t, "CPER-RECORD-1", string(buf[:n]))
	})
}

func TestInfoLogReadAll(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, err := getInfoLog(t, driverConfigComponentErrs, 0).ReadAll()
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("ErrorNoMaxSize", func(t *testing.T) {
		_, err := getInfoLog(t, driverConfigNoMaxSize, 0).ReadAll()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("WarningDropped", func(t *testing.T) {
		data, err := getInfoLog(t, driverConfigWarningDropped, 0).ReadAll()
		require.ErrorIs(t, err, core.RESULT_WARNING_DROPPED_DATA)
		assert.Equal(t, "CPER-RECORD-1", string(data))
	})
	t.Run("Success", func(t *testing.T) {
		// all pending records are read in one go
		data, err := getInfoLog(t, driverConfigDefault, 0).ReadAll()
		require.NoError(t, err)
		assert.Equal(t, "CPER-RECORD-1CPER-RECORD-2", string(data))
	})
}

func TestInfoLogReadWithMetadata(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		_, err := getInfoLog(t, driverConfigComponentErrs, 0).ReadWithMetadata()
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("SuccessNothingPending", func(t *testing.T) {
		// nothing is read if the driver reports nothing pending
		records, err := getInfoLog(t, driverConfigDefault, 1).ReadWithMetadata()
		require.NoError(t, err)
		assert.Empty(t, records)
	})
	t.Run("WarningDropped", func(t *testing.T) {
		// the records that were read are returned despite the warning
		records, err := getInfoLog(t, driverConfigWarningDropped, 0).ReadWithMetadata()
		require.ErrorIs(t, err, core.RESULT_WARNING_DROPPED_DATA)
		require.Len(t, records, 1)
		assert.Equal(t, "CPER-RECORD-1", string(records[0].Data))
	})
	t.Run("Success", func(t *testing.T) {
		// all pending records are read in one go, each one located by its metadata
		records, err := getInfoLog(t, driverConfigDefault, 0).ReadWithMetadata()
		require.NoError(t, err)
		require.Len(t, records, 2)
		assert.Equal(t, "CPER-RECORD-1", string(records[0].Data))
		assert.Equal(t, "CPER-RECORD-2", string(records[1].Data))

		meta := records[0].Metadata
		assert.Equal(t, sysman.PciAddress{Bus: 1, Device: 2, Function: 3}, meta.Address)
		assert.Equal(t, "12345678-abcd-ef01-2345-6789abcdef01", meta.Uuid.Id.String())
		assert.Equal(t, uint64(1234567), meta.Timestamp)
		assert.Equal(t, uint32(len("CPER-RECORD-1")), meta.LengthOfData)
		assert.Zero(t, meta.Offset)
		assert.Equal(t, uint32(len("CPER-RECORD-1")), records[1].Metadata.Offset)
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

func TestInfoLogEnable(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		err := getInfoLog(t, driverConfigComponentErrs, 0).Enable("", 0, 0)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("SuccessDefaults", func(t *testing.T) {
		// zero values leave the whole configuration to the driver
		require.NoError(t, getInfoLog(t, driverConfigDefault, 0).Enable("", 0, 0))
	})
	t.Run("SuccessAllArguments", func(t *testing.T) {
		require.NoError(t, getInfoLog(t, driverConfigDefault, 0).Enable("level-zero-go", 4096, 50))
	})
}

func TestInfoLogDisable(t *testing.T) {
	t.Run("ErrorFromDriver", func(t *testing.T) {
		err := getInfoLog(t, driverConfigComponentErrs, 0).Disable()
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})
	t.Run("Success", func(t *testing.T) {
		require.NoError(t, getInfoLog(t, driverConfigDefault, 0).Disable())
	})
}
