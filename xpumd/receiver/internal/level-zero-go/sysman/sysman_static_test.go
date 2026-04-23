// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Intel Corporation

package sysman

import (
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/intel/level-zero-go/core"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Driver config paths (relative to the sysman/ package directory, i.e. go test CWD).
const (
	driverConfigDefault   = "testdata/default.yaml"
	driverConfigDriverGet = "testdata/error_driver_get.yaml"
	driverConfigDriver    = "testdata/error_driver.yaml"
	driverConfigDevice    = "testdata/error_device.yaml"
	driverConfigComponent = "testdata/error_component.yaml"
)

type fixedStringProperty interface {
	core.StringProperty64 | core.StringProperty256
}

func stringProperty[T fixedStringProperty](value string) T {
	var prop T
	switch p := any(&prop).(type) {
	case *core.StringProperty64:
		copy(p[:], value)
	case *core.StringProperty256:
		copy(p[:], value)
	default:
		panic("unsupported string property type")
	}
	return prop
}

// loadDriverConfig reloads the stub with the given driver config file.
func loadDriverConfig(t *testing.T, path string) {
	t.Helper()
	require.NoError(t, stubReload(path))
}

// getDriver returns drivers[idx] from the currently-loaded stub state.
func getDriver(t *testing.T, idx int) *Driver {
	t.Helper()
	drivers, err := DriverGet()
	require.NoError(t, err)
	require.Greater(t, len(drivers), idx, "driver index out of range")
	return drivers[idx]
}

// getDevice returns drivers[drvIdx].devices[idx] from the currently-loaded stub state.
func getDevice(t *testing.T, drvIdx, idx int) *Device {
	t.Helper()
	drv := getDriver(t, drvIdx)
	devices, err := drv.DeviceGet()
	require.NoError(t, err)
	require.Greater(t, len(devices), idx, "device index out of range")
	return devices[idx]
}

// getComponent is a generic helper that enumerates components on a device and returns
// the component at the given index.
func getComponent[C any](t *testing.T, drvIdx, devIdx, idx int, enum func(*Device) ([]*C, error)) *C {
	t.Helper()
	dev := getDevice(t, drvIdx, devIdx)
	items, err := enum(dev)
	require.NoError(t, err)
	require.Greater(t, len(items), idx, "component index out of range")
	return items[idx]
}

func getEngine(t *testing.T, drvIdx, devIdx, idx int) *Engine {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumEngineGroups)
}

func getFabricPort(t *testing.T, drvIdx, devIdx, idx int) *FabricPort {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumFabricPorts)
}

func getFan(t *testing.T, drvIdx, devIdx, idx int) *Fan {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumFans)
}

func getFirmware(t *testing.T, drvIdx, devIdx, idx int) *Firmware {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumFirmwares)
}

func getFrequency(t *testing.T, drvIdx, devIdx, idx int) *Frequency {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumFrequencyDomains)
}

func getLed(t *testing.T, drvIdx, devIdx, idx int) *Led {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumLeds)
}

func getMemory(t *testing.T, drvIdx, devIdx, idx int) *Memory {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumMemoryModules)
}

func getOcDomain(t *testing.T, drvIdx, devIdx, idx int) *Overclock {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumOverclockDomains)
}

func getPerf(t *testing.T, drvIdx, devIdx, idx int) *Performance {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumPerformanceFactorDomains)
}

func getPower(t *testing.T, drvIdx, devIdx, idx int) *Power {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumPowerDomains)
}

func getPsu(t *testing.T, drvIdx, devIdx, idx int) *Psu {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumPsus)
}

func getRas(t *testing.T, drvIdx, devIdx, idx int) *Ras {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumRasErrorSets)
}

func getScheduler(t *testing.T, drvIdx, devIdx, idx int) *Scheduler {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumSchedulers)
}

func getStandby(t *testing.T, drvIdx, devIdx, idx int) *Standby {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumStandbyDomains)
}

func getTemperature(t *testing.T, drvIdx, devIdx, idx int) *Temperature {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumTemperatureSensors)
}

func getDiagnostic(t *testing.T, drvIdx, devIdx, idx int) *Diagnostics {
	t.Helper()
	return getComponent(t, drvIdx, devIdx, idx, (*Device).EnumDiagnosticTestSuites)
}

// ------------------------------------------------------------------
// Init
// ------------------------------------------------------------------

func TestInit(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDriverGet)
		err := Init(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNINITIALIZED)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := Init(0)
		require.NoError(t, err)
	})
}

// ------------------------------------------------------------------
// Driver
// ------------------------------------------------------------------

func TestDriverGet(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDriverGet)
		_, err := DriverGet()
		require.ErrorIs(t, err, core.RESULT_ERROR_UNINITIALIZED)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		drivers, err := DriverGet()
		require.NoError(t, err)
		require.Len(t, drivers, 2)
	})
}

func TestDriverGetExtensionProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDriver)
		// DriverGet silently swallows GetExtensionProperties errors, so call directly.
		drv := getDriver(t, 0)
		_, err := drv.GetExtensionProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		drv := getDriver(t, 0)
		exts, err := drv.GetExtensionProperties()
		require.NoError(t, err)
		assert.Equal(t, []DriverExtensionProperties{
			{Name: stringProperty[core.StringProperty256]("ZES_extension_foo"), Version: 1},
			{Name: stringProperty[core.StringProperty256]("ZES_extension_bar"), Version: 2},
		}, exts)
	})
}

func TestDriverDeviceGet(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDriver)
		drv := getDriver(t, 0)
		_, err := drv.DeviceGet()
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		drv := getDriver(t, 0)
		devices, err := drv.DeviceGet()
		require.NoError(t, err)
		require.Len(t, devices, 1)
	})
}

func TestDriverEventListen(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDriver)
		drv := getDriver(t, 0)
		_, _, err := drv.EventListen(time.Second, nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		drv := getDriver(t, 0)
		dev := getDevice(t, 0, 0)
		numEvents, events, err := drv.EventListen(0, []*Device{dev})
		require.NoError(t, err)
		assert.Equal(t, uint32(1), numEvents)
		assert.Equal(t, EventTypeFlags(EVENT_TYPE_FLAG_DEVICE_DETACH), events[0])
	})
}

func TestDriverEventListenEx(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDriver)
		drv := getDriver(t, 0)
		_, _, err := drv.EventListenEx(time.Second, nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		drv := getDriver(t, 0)
		dev := getDevice(t, 0, 0)
		numEvents, events, err := drv.EventListenEx(0, []*Device{dev})
		require.NoError(t, err)
		assert.Equal(t, uint32(1), numEvents)
		assert.Equal(t, EventTypeFlags(EVENT_TYPE_FLAG_DEVICE_DETACH), events[0])
	})
}

func TestDeviceGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getDevice(t, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, DeviceBaseProperties{
			Core: core.DeviceProperties{
				Type:                     core.DEVICE_TYPE_GPU,
				VendorId:                 32902,
				DeviceId:                 3029,
				Flags:                    core.DevicePropertyFlags(core.DEVICE_PROPERTY_FLAG_INTEGRATED),
				SubdeviceId:              1,
				CoreClockRate:            1600,
				MaxMemAllocSize:          17179869184,
				MaxHardwareContexts:      256,
				MaxCommandQueuePriority:  2,
				NumThreadsPerEU:          8,
				PhysicalEUSimdWidth:      16,
				NumEUsPerSubslice:        16,
				NumSubslicesPerSlice:     8,
				NumSlices:                2,
				TimerResolution:          100,
				TimestampValidBits:       36,
				KernelTimestampValidBits: 32,
				Uuid: core.DeviceUuid{
					Id: uuid.MustParse("12345678-1234-5678-9abc-def000000000"),
				},
				Name: stringProperty[core.StringProperty256]("ACME Data Center GPU"),
			},
			NumSubdevices: 2,
			SerialNumber:  stringProperty[core.StringProperty64]("SN-0001"),
			BoardNumber:   stringProperty[core.StringProperty64]("BOARD-0001"),
			BrandName:     stringProperty[core.StringProperty64]("ACME"),
			ModelName:     stringProperty[core.StringProperty64]("Stub GPU 1234"),
			VendorName:    stringProperty[core.StringProperty64]("ACME Corporation"),
			DriverVersion: stringProperty[core.StringProperty64]("1.2.3"),
		}, props.DeviceBaseProperties)

	})

	t.Run("SuccessWithExtProps", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getDevice(t, 1, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, DeviceExtProperties{
			Uuid: Uuid{
				Id: uuid.MustParse("abcdef01-2345-6789-abcd-ef0000000000"),
			},
			Type:  DEVICE_TYPE_GPU,
			Flags: 1,
		}, props.DeviceExtProperties)
	})
}

func TestDeviceGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).GetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getDevice(t, 0, 0).GetState()
		require.NoError(t, err)
		assert.EqualExportedValues(t, DeviceState{
			Reset:    0,
			Repaired: REPAIR_STATUS_NOT_PERFORMED,
		}, state)
	})
}

func TestDeviceReset(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		err := getDevice(t, 0, 0).Reset(false)
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getDevice(t, 0, 0).Reset(false)
		require.NoError(t, err)
	})
}

func TestDeviceResetExt(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		err := getDevice(t, 0, 0).ResetExt(nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getDevice(t, 0, 0).ResetExt(nil)
		require.NoError(t, err)
	})
}

func TestDeviceProcessesGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).ProcessesGetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		procs, err := getDevice(t, 0, 0).ProcessesGetState()
		require.NoError(t, err)
		assert.EqualExportedValues(t, []ProcessState{
			{
				ProcessId:  1234,
				MemSize:    1073741824,
				SharedSize: 268435456,
				Engines:    7,
			},
			{
				ProcessId:  5678,
				MemSize:    536870912,
				SharedSize: 134217728,
				Engines:    2,
			},
		}, procs)
	})
}

func TestDeviceEventRegister(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		err := getDevice(t, 0, 0).EventRegister(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getDevice(t, 0, 0).EventRegister(0)
		require.NoError(t, err)
	})
}

// ------------------------------------------------------------------
// Device.Pci
// ------------------------------------------------------------------

func TestDevicePciGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).PciGetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getDevice(t, 0, 0).PciGetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, PciProperties{
			PciBaseProperties: PciBaseProperties{
				Address: PciAddress{Domain: 1, Bus: 5},
				MaxSpeed: PciSpeed{
					Gen:          4,
					Width:        16,
					MaxBandwidth: 256000000000,
				},
				HaveBandwidthCounters: 1,
				HavePacketCounters:    1,
				HaveReplayCounters:    1,
			},
		}, props)
		assert.Nil(t, props.LinkSpeedDowngrade)
	})

	t.Run("SuccessWithExt", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getDevice(t, 1, 0).PciGetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, PciProperties{
			PciBaseProperties: PciBaseProperties{
				Address: PciAddress{Domain: 1, Bus: 2, Device: 3, Function: 4},
				MaxSpeed: PciSpeed{
					Gen:          5,
					Width:        16,
					MaxBandwidth: 512000000000,
				},
				HaveBandwidthCounters: 1,
				HaveReplayCounters:    1,
			},
			LinkSpeedDowngrade: &PciLinkSpeedDowngradeExtProperties{
				PciLinkSpeedUpdateCapable: 1,
				MaxPciGenSupported:        5,
			},
		}, props)
	})
}

func TestDevicePciGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).PciGetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getDevice(t, 0, 0).PciGetState()
		require.NoError(t, err)
		assert.EqualExportedValues(t, PciState{
			PciBaseState: PciBaseState{
				Status:          PCI_LINK_STATUS_QUALITY_ISSUES,
				QualityIssues:   PciLinkQualIssueFlags(PCI_LINK_QUAL_ISSUE_FLAG_REPLAYS | PCI_LINK_QUAL_ISSUE_FLAG_SPEED),
				StabilityIssues: PciLinkStabIssueFlags(PCI_LINK_STAB_ISSUE_FLAG_RETRAINING),
				Speed: PciSpeed{
					Gen:          3,
					Width:        8,
					MaxBandwidth: 126000000000,
				},
			},
		}, state)
		assert.Nil(t, state.LinkSpeedDowngrade)
	})

	t.Run("SuccessWithExt", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getDevice(t, 1, 0).PciGetState()
		require.NoError(t, err)
		assert.EqualExportedValues(t, PciState{
			PciBaseState: PciBaseState{
				Status:        PCI_LINK_STATUS_QUALITY_ISSUES,
				QualityIssues: PciLinkQualIssueFlags(PCI_LINK_QUAL_ISSUE_FLAG_SPEED),
				Speed: PciSpeed{
					Gen:          3,
					Width:        8,
					MaxBandwidth: 126000000000,
				},
			},
			LinkSpeedDowngrade: &PciLinkSpeedDowngradeExtState{
				PciLinkSpeedDowngradeStatus: 1,
			},
		}, state)
	})
}

func TestDevicePciLinkSpeedUpdateExt(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).PciLinkSpeedUpdateExt(true)
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		action, err := getDevice(t, 1, 0).PciLinkSpeedUpdateExt(true)
		require.NoError(t, err)
		assert.Equal(t, DEVICE_ACTION_WARM_CARD_RESET, action)
	})
}

func TestDevicePciGetBars(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).PciGetBars()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		bars, err := getDevice(t, 0, 0).PciGetBars()
		require.NoError(t, err)
		assert.EqualExportedValues(t, []PciBarProperties{
			{Type: PCI_BAR_TYPE_MMIO, Index: 0, Base: 0x80000000, Size: 16777216},
			{Type: PCI_BAR_TYPE_MEM, Index: 2, Base: 0x100000000, Size: 268435456},
		}, bars)
	})
}

func TestDevicePciGetStats(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).PciGetStats()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})

	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		stats, err := getDevice(t, 0, 0).PciGetStats()
		require.NoError(t, err)
		assert.Equal(t, PciStats{
			Timestamp:     999999,
			ReplayCounter: 77,
			PacketCounter: 88,
			RxCounter:     1000000,
			TxCounter:     2000000,
			Speed: PciSpeed{
				Gen:          3,
				Width:        8,
				MaxBandwidth: 126000000000,
			},
		}, stats)
	})
}

// ------------------------------------------------------------------
// Device overclock helpers (no component handle needed)
// ------------------------------------------------------------------

func TestDeviceSetOverclockWaiver(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		err := getDevice(t, 0, 0).SetOverclockWaiver()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getDevice(t, 0, 0).SetOverclockWaiver()
		require.NoError(t, err)
	})
}

func TestDeviceGetOverclockDomains(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).GetOverclockDomains()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		mask, err := getDevice(t, 0, 0).GetOverclockDomains()
		require.NoError(t, err)
		assert.Equal(t, OverclockDomains(OVERCLOCK_DOMAIN_CARD|OVERCLOCK_DOMAIN_PACKAGE), mask, "mask")
	})
}

func TestDeviceGetOverclockControls(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).GetOverclockControls(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		mask, err := getDevice(t, 0, 0).GetOverclockControls(0)
		require.NoError(t, err)
		assert.Equal(t, OverclockControls(OVERCLOCK_CONTROL_VF|OVERCLOCK_CONTROL_FREQ_OFFSET|OVERCLOCK_CONTROL_VMAX_OFFSET), mask, "mask")
	})
}

func TestDeviceResetOverclockSettings(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		err := getDevice(t, 0, 0).ResetOverclockSettings(false)
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getDevice(t, 0, 0).ResetOverclockSettings(false)
		require.NoError(t, err)
	})
}

func TestDeviceReadOverclockState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).ReadOverclockState()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getDevice(t, 0, 0).ReadOverclockState()
		require.NoError(t, err)
		assert.Equal(t, OverclockState{
			Mode:          OVERCLOCK_MODE_MODE_ON,
			WaiverSetting: true,
			State:         false,
			PendingAction: PENDING_ACTION_PENDING_NONE,
			PendingReset:  false,
		}, state)
	})
}

func TestDeviceEccAvailable(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EccAvailable()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		available, err := getDevice(t, 0, 0).EccAvailable()
		require.NoError(t, err)
		assert.True(t, available, "EccAvailable")
	})
}

func TestDeviceEccConfigurable(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EccConfigurable()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		configurable, err := getDevice(t, 0, 0).EccConfigurable()
		require.NoError(t, err)
		assert.True(t, configurable, "EccConfigurable")
	})
}

func TestDeviceGetEccState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).GetEccState()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getDevice(t, 0, 0).GetEccState()
		require.NoError(t, err)
		assert.EqualExportedValues(t, EccProperties{
			DeviceEccProperties: DeviceEccProperties{
				CurrentState:  DEVICE_ECC_STATE_ENABLED,
				PendingState:  DEVICE_ECC_STATE_ENABLED,
				PendingAction: DEVICE_ACTION_NONE,
			},
		}, props)
	})
	t.Run("SuccessWithExtProps", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getDevice(t, 1, 0).GetEccState()
		require.NoError(t, err)
		assert.EqualExportedValues(t, EccProperties{
			DeviceEccProperties: DeviceEccProperties{},
			ExtendedProperties: &DeviceEccDefaultPropertiesExt{
				DefaultState: DEVICE_ECC_STATE_ENABLED,
			},
		}, props)
	})
}

func TestDeviceSetEccState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).SetEccState(DeviceEccDesc{})
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getDevice(t, 0, 0).SetEccState(DeviceEccDesc{})
		require.NoError(t, err)
		assert.EqualExportedValues(t, DeviceEccProperties{
			CurrentState: DEVICE_ECC_STATE_ENABLED,
			PendingState: DEVICE_ECC_STATE_ENABLED,
		}, state)
	})
}

// ------------------------------------------------------------------
// Device enum methods
// ------------------------------------------------------------------

func TestDeviceEnumEngineGroups(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumEngineGroups()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		engines, err := getDevice(t, 0, 0).EnumEngineGroups()
		require.NoError(t, err)
		require.Len(t, engines, 3)
	})
}

func TestDeviceEnumFrequencyDomains(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumFrequencyDomains()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		freqs, err := getDevice(t, 0, 0).EnumFrequencyDomains()
		require.NoError(t, err)
		require.Len(t, freqs, 2)
	})
}

func TestDeviceEnumMemoryModules(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumMemoryModules()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		mems, err := getDevice(t, 0, 0).EnumMemoryModules()
		require.NoError(t, err)
		require.Len(t, mems, 2)
	})
}

func TestDeviceEnumPowerDomains(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumPowerDomains()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		powers, err := getDevice(t, 0, 0).EnumPowerDomains()
		require.NoError(t, err)
		require.Len(t, powers, 2)
	})
}

func TestDeviceEnumSchedulers(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumSchedulers()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		scheds, err := getDevice(t, 0, 0).EnumSchedulers()
		require.NoError(t, err)
		require.Len(t, scheds, 2)
	})
}

func TestDeviceEnumTemperatureSensors(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumTemperatureSensors()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		temps, err := getDevice(t, 0, 0).EnumTemperatureSensors()
		require.NoError(t, err)
		require.Len(t, temps, 3)
	})
}

func TestDeviceEnumFabricPorts(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumFabricPorts()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		ports, err := getDevice(t, 0, 0).EnumFabricPorts()
		require.NoError(t, err)
		require.Len(t, ports, 2)
	})
}

func TestDeviceEnumFans(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumFans()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		fans, err := getDevice(t, 0, 0).EnumFans()
		require.NoError(t, err)
		require.Len(t, fans, 2)
	})
}

func TestDeviceEnumFirmwares(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumFirmwares()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		fws, err := getDevice(t, 0, 0).EnumFirmwares()
		require.NoError(t, err)
		require.Len(t, fws, 2)
	})
}

func TestDeviceEnumLeds(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumLeds()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		leds, err := getDevice(t, 0, 0).EnumLeds()
		require.NoError(t, err)
		require.Len(t, leds, 2)
	})
}

func TestDeviceEnumOverclockDomains(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumOverclockDomains()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		ocs, err := getDevice(t, 0, 0).EnumOverclockDomains()
		require.NoError(t, err)
		require.Len(t, ocs, 1)
	})
}

func TestDeviceEnumPerformanceFactorDomains(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumPerformanceFactorDomains()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		perfs, err := getDevice(t, 0, 0).EnumPerformanceFactorDomains()
		require.NoError(t, err)
		require.Len(t, perfs, 2)
	})
}

func TestDeviceEnumPsus(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumPsus()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		psus, err := getDevice(t, 0, 0).EnumPsus()
		require.NoError(t, err)
		require.Len(t, psus, 2)
	})
}

func TestDeviceEnumRasErrorSets(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumRasErrorSets()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		ras, err := getDevice(t, 0, 0).EnumRasErrorSets()
		require.NoError(t, err)
		require.Len(t, ras, 2)
	})
}

func TestDeviceEnumStandbyDomains(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumStandbyDomains()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		standbys, err := getDevice(t, 0, 0).EnumStandbyDomains()
		require.NoError(t, err)
		require.Len(t, standbys, 1)
	})
}

func TestDeviceEnumDiagnosticTestSuites(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDevice)
		_, err := getDevice(t, 0, 0).EnumDiagnosticTestSuites()
		require.ErrorIs(t, err, core.RESULT_ERROR_INVALID_ARGUMENT)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		diags, err := getDevice(t, 0, 0).EnumDiagnosticTestSuites()
		require.NoError(t, err)
		require.Len(t, diags, 2)
	})
}

func TestDeviceFabricPortGetMultiPortThroughput(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		// Pass nil (count=0) to avoid the CGo restriction on Go pointer slices.
		_, err := getDevice(t, 0, 0).FabricPortGetMultiPortThroughput(nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		// Pass nil (count=0) to avoid the CGo restriction on Go pointer slices.
		results, err := getDevice(t, 0, 0).FabricPortGetMultiPortThroughput(nil)
		require.NoError(t, err)
		require.Empty(t, results)
	})
}

// ------------------------------------------------------------------
// Engine
// ------------------------------------------------------------------

func TestEngineGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getEngine(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getEngine(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, EngineBaseProperties{
			Type: ENGINE_GROUP_ALL,
		}, props.EngineBaseProperties)
	})
	t.Run("SuccessWithExtProps", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getEngine(t, 1, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, EngineProperties{
			EngineBaseProperties: EngineBaseProperties{
				Type: ENGINE_GROUP_ALL,
			},
			ExtendedProperties: &EngineExtProperties{
				CountOfVirtualFunctionInstance: 4,
			},
		}, props)
	})
}

func TestEngineGetActivity(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getEngine(t, 0, 0, 0).GetActivity()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		stats, err := getEngine(t, 0, 0, 0).GetActivity()
		require.NoError(t, err)
		assert.Equal(t, EngineStats{
			ActiveTime: 12345678,
			Timestamp:  87654321,
		}, stats)
	})
}

func TestEngineGetActivityExt(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getEngine(t, 0, 0, 0).GetActivityExt()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		stats, err := getEngine(t, 0, 0, 0).GetActivityExt()
		require.NoError(t, err)
		require.Empty(t, stats)
	})
}

// ------------------------------------------------------------------
// Frequency
// ------------------------------------------------------------------

func TestFrequencyGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFrequency(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getFrequency(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, FreqProperties{
			Type:                     FREQ_DOMAIN_GPU,
			OnSubdevice:              1,
			SubdeviceId:              1,
			CanControl:               1,
			IsThrottleEventSupported: 1,
			Min:                      300.0,
			Max:                      1600.0,
		}, props)
	})
}

func TestFrequencyGetAvailableClocks(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFrequency(t, 0, 0, 0).GetAvailableClocks()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		clocks, err := getFrequency(t, 0, 0, 0).GetAvailableClocks()
		require.NoError(t, err)
		require.Len(t, clocks, 3)
		assert.Equal(t, 300.0, clocks[0], "clocks[0]")
		assert.Equal(t, 900.0, clocks[1], "clocks[1]")
		assert.Equal(t, 1600.0, clocks[2], "clocks[2]")
	})
}

func TestFrequencyGetRange(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFrequency(t, 0, 0, 0).GetRange()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		rng, err := getFrequency(t, 0, 0, 0).GetRange()
		require.NoError(t, err)
		assert.Equal(t, FreqRange{Min: 300.0, Max: 1600.0}, rng)
	})
}

func TestFrequencySetRange(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getFrequency(t, 0, 0, 0).SetRange(&FreqRange{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getFrequency(t, 0, 0, 0).SetRange(&FreqRange{})
		require.NoError(t, err)
	})
}

func TestFrequencyGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFrequency(t, 0, 0, 0).GetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getFrequency(t, 0, 0, 0).GetState()
		require.NoError(t, err)
		assert.Equal(t, FreqState{
			CurrentVoltage: 0.85,
			Request:        1400.0,
			Tdp:            1600.0,
			Efficient:      300.0,
			Actual:         1200.0,
			ThrottleReasons: FreqThrottleReasonFlags(
				FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT | FREQ_THROTTLE_REASON_FLAG_SW_RANGE,
			),
		}, state)
	})
}

func TestFrequencyGetThrottleTime(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFrequency(t, 0, 0, 0).GetThrottleTime()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		tt, err := getFrequency(t, 0, 0, 0).GetThrottleTime()
		require.NoError(t, err)
		assert.Equal(t, FreqThrottleTime{
			ThrottleTime: 5000,
			Timestamp:    100000,
		}, tt)
	})
}

// ------------------------------------------------------------------
// Memory
// ------------------------------------------------------------------

func TestMemoryGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getMemory(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getMemory(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, MemProperties{
			Type:         MEM_TYPE_HBM,
			OnSubdevice:  1,
			SubdeviceId:  1,
			Location:     MEM_LOC_DEVICE,
			PhysicalSize: 17179869184,
			BusWidth:     128,
			NumChannels:  8,
		}, props)
	})
}

func TestMemoryGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getMemory(t, 0, 0, 0).GetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getMemory(t, 0, 0, 0).GetState()
		require.NoError(t, err)
		assert.Equal(t, MemState{
			Health: MEM_HEALTH_OK,
			Free:   8589934592,
			Size:   17179869184,
		}, state)
	})
}

func TestMemoryGetBandwidth(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getMemory(t, 0, 0, 0).GetBandwidth()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		bw, err := getMemory(t, 0, 0, 0).GetBandwidth()
		require.NoError(t, err)
		assert.Equal(t, MemBandwidth{
			ReadCounter:  1073741824,
			WriteCounter: 536870912,
			MaxBandwidth: 512000000000,
			Timestamp:    123456789,
		}, bw)
	})
}

// ------------------------------------------------------------------
// Power
// ------------------------------------------------------------------

func TestPowerGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getPower(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getPower(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, PowerBaseProperties{
			OnSubdevice:                1,
			SubdeviceId:                1,
			CanControl:                 1,
			IsEnergyThresholdSupported: 1,
			DefaultLimit:               200000,
			MinLimit:                   100000,
			MaxLimit:                   300000,
		}, props.PowerBaseProperties)
	})
	t.Run("SuccessWithExtProps", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getPower(t, 1, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, PowerProperties{
			PowerBaseProperties: PowerBaseProperties{
				OnSubdevice:                1,
				SubdeviceId:                1,
				CanControl:                 1,
				IsEnergyThresholdSupported: 1,
				DefaultLimit:               150000,
				MinLimit:                   90000,
				MaxLimit:                   225000,
			},
			ExtendedProperties: &PowerExtProperties{
				Domain: POWER_DOMAIN_PACKAGE,
				DefaultLimit: &PowerLimitExtDesc{
					Level:               POWER_LEVEL_SUSTAINED,
					Source:              POWER_SOURCE_ANY,
					LimitUnit:           LIMIT_UNIT_UNKNOWN,
					EnabledStateLocked:  1,
					Enabled:             1,
					IntervalValueLocked: 1,
					Interval:            250,
					LimitValueLocked:    1,
					Limit:               150000,
				},
			},
		}, props)
	})
}

func TestPowerGetEnergyCounter(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getPower(t, 0, 0, 0).GetEnergyCounter()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		ec, err := getPower(t, 0, 0, 0).GetEnergyCounter()
		require.NoError(t, err)
		assert.Equal(t, PowerEnergyCounter{
			Energy:    5000000,
			Timestamp: 123456789,
		}, ec)
	})
}

func TestPowerGetEnergyThreshold(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getPower(t, 0, 0, 0).GetEnergyThreshold()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		et, err := getPower(t, 0, 0, 0).GetEnergyThreshold()
		require.NoError(t, err)
		assert.Equal(t, EnergyThreshold{
			Enable:    1,
			Threshold: 10.5,
			ProcessId: 4321,
		}, et)
	})
}

func TestPowerSetEnergyThreshold(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getPower(t, 0, 0, 0).SetEnergyThreshold(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getPower(t, 0, 0, 0).SetEnergyThreshold(0)
		require.NoError(t, err)
	})
}

func TestPowerGetLimitsExt(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getPower(t, 0, 0, 0).GetLimitsExt()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		limits, err := getPower(t, 0, 0, 0).GetLimitsExt()
		require.NoError(t, err)
		assert.EqualExportedValues(t, []PowerLimitExtDesc{
			{
				Level:               POWER_LEVEL_SUSTAINED,
				Source:              POWER_SOURCE_ANY,
				LimitUnit:           LIMIT_UNIT_UNKNOWN,
				EnabledStateLocked:  0,
				Enabled:             1,
				IntervalValueLocked: 0,
				Interval:            1000,
				LimitValueLocked:    0,
				Limit:               200000,
			},
			{
				Level:               POWER_LEVEL_BURST,
				Source:              POWER_SOURCE_ANY,
				LimitUnit:           LIMIT_UNIT_UNKNOWN,
				EnabledStateLocked:  0,
				Enabled:             1,
				IntervalValueLocked: 0,
				Interval:            0,
				LimitValueLocked:    0,
				Limit:               250000,
			},
		}, limits)
	})
}

func TestPowerSetLimitsExt(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getPower(t, 0, 0, 0).SetLimitsExt(nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getPower(t, 0, 0, 0).SetLimitsExt(nil)
		require.NoError(t, err)
	})
}

// ------------------------------------------------------------------
// Scheduler
// ------------------------------------------------------------------

func TestSchedulerGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getScheduler(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getScheduler(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, SchedProperties{
			OnSubdevice:    0,
			SubdeviceId:    0,
			CanControl:     1,
			SupportedModes: 7,
			Engines:        7,
		}, props)
	})
}

func TestSchedulerGetCurrentMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getScheduler(t, 0, 0, 0).GetCurrentMode()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		mode, err := getScheduler(t, 0, 0, 0).GetCurrentMode()
		require.NoError(t, err)
		assert.Equal(t, SCHED_MODE_TIMESLICE, mode, "CurrentMode")
	})
}

func TestSchedulerGetTimeoutModeProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getScheduler(t, 0, 0, 0).GetTimeoutModeProperties(false)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getScheduler(t, 0, 0, 0).GetTimeoutModeProperties(false)
		require.NoError(t, err)
		assert.EqualExportedValues(t, SchedTimeoutProperties{
			WatchdogTimeout: 5000000,
		}, props)
	})
}

func TestSchedulerGetTimesliceModeProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getScheduler(t, 0, 0, 0).GetTimesliceModeProperties(false)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getScheduler(t, 0, 0, 0).GetTimesliceModeProperties(false)
		require.NoError(t, err)
		assert.Equal(t, SchedTimesliceProperties{
			Interval:     2000,
			YieldTimeout: 500,
		}, props)
	})
}

func TestSchedulerSetTimeoutMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getScheduler(t, 0, 0, 0).SetTimeoutMode(&SchedTimeoutProperties{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		needReload, err := getScheduler(t, 0, 0, 0).SetTimeoutMode(&SchedTimeoutProperties{})
		require.NoError(t, err)
		assert.Equal(t, false, needReload, "needReload")
	})
}

func TestSchedulerSetTimesliceMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getScheduler(t, 0, 0, 0).SetTimesliceMode(&SchedTimesliceProperties{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		needReload, err := getScheduler(t, 0, 0, 0).SetTimesliceMode(&SchedTimesliceProperties{})
		require.NoError(t, err)
		assert.Equal(t, false, needReload, "needReload")
	})
}

func TestSchedulerSetExclusiveMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getScheduler(t, 0, 0, 0).SetExclusiveMode()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		needReload, err := getScheduler(t, 0, 0, 0).SetExclusiveMode()
		require.NoError(t, err)
		assert.Equal(t, false, needReload, "needReload")
	})
}

// ------------------------------------------------------------------
// Temperature
// ------------------------------------------------------------------

func TestTemperatureGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getTemperature(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getTemperature(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, TempProperties{
			OnSubdevice:             1,
			SubdeviceId:             1,
			Type:                    TEMP_SENSORS_GLOBAL,
			MaxTemperature:          110.0,
			IsCriticalTempSupported: 1,
			IsThreshold1Supported:   1,
			IsThreshold2Supported:   1,
		}, props)
	})
}

func TestTemperatureGetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getTemperature(t, 0, 0, 0).GetConfig()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		cfg, err := getTemperature(t, 0, 0, 0).GetConfig()
		require.NoError(t, err)
		assert.Equal(t, TempConfig{
			EnableCritical: 1,
			Threshold1: TempThreshold{
				EnableLowToHigh: 1,
				EnableHighToLow: 1,
				Threshold:       90.0,
			},
			Threshold2: TempThreshold{
				EnableLowToHigh: 1,
				EnableHighToLow: 1,
				Threshold:       100.0,
			},
		}, cfg)
	})
}

func TestTemperatureSetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getTemperature(t, 0, 0, 0).SetConfig(&TempConfig{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getTemperature(t, 0, 0, 0).SetConfig(&TempConfig{})
		require.NoError(t, err)
	})
}

func TestTemperatureGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getTemperature(t, 0, 0, 0).GetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		temp, err := getTemperature(t, 0, 0, 0).GetState()
		require.NoError(t, err)
		assert.Equal(t, 48.0, temp, "Value")
	})
}

// ------------------------------------------------------------------
// FabricPort
// ------------------------------------------------------------------

func TestFabricPortGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFabricPort(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getFabricPort(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, FabricPortProperties{
			Model:       stringProperty[core.StringProperty256]("Xe-Link"),
			OnSubdevice: 1,
			SubdeviceId: 1,
			PortId: FabricPortId{
				FabricId:   10,
				AttachId:   20,
				PortNumber: 3,
			},
			MaxRxSpeed: FabricPortSpeed{BitRate: 53125000000, Width: 4},
			MaxTxSpeed: FabricPortSpeed{BitRate: 50000000000, Width: 4},
		}, props)
	})
}

func TestFabricPortGetLinkType(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFabricPort(t, 0, 0, 0).GetLinkType()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		link, err := getFabricPort(t, 0, 0, 0).GetLinkType()
		require.NoError(t, err)
		assert.Equal(t, FabricLinkType{
			Desc: stringProperty[core.StringProperty256]("Xe-Link"),
		}, link)
	})
}

func TestFabricPortGetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFabricPort(t, 0, 0, 0).GetConfig()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		cfg, err := getFabricPort(t, 0, 0, 0).GetConfig()
		require.NoError(t, err)
		assert.EqualExportedValues(t, FabricPortConfig{
			Enabled:   1,
			Beaconing: 1,
		}, cfg)
	})
}

func TestFabricPortSetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getFabricPort(t, 0, 0, 0).SetConfig(&FabricPortConfig{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getFabricPort(t, 0, 0, 0).SetConfig(&FabricPortConfig{})
		require.NoError(t, err)
	})
}

func TestFabricPortGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFabricPort(t, 0, 0, 0).GetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getFabricPort(t, 0, 0, 0).GetState()
		require.NoError(t, err)
		assert.EqualExportedValues(t, FabricPortState{
			Status: FABRIC_PORT_STATUS_DEGRADED,
			QualityIssues: FabricPortQualIssueFlags(
				FABRIC_PORT_QUAL_ISSUE_FLAG_LINK_ERRORS | FABRIC_PORT_QUAL_ISSUE_FLAG_SPEED,
			),
			FailureReasons: FabricPortFailureFlags(
				FABRIC_PORT_FAILURE_FLAG_FAILED | FABRIC_PORT_FAILURE_FLAG_FLAPPING,
			),
			RemotePortId: FabricPortId{
				FabricId:   11,
				AttachId:   21,
				PortNumber: 4,
			},
			RxSpeed: FabricPortSpeed{BitRate: 49000000000, Width: 4},
			TxSpeed: FabricPortSpeed{BitRate: 47000000000, Width: 4},
		}, state)
	})
}

func TestFabricPortGetThroughput(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFabricPort(t, 0, 0, 0).GetThroughput()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		tp, err := getFabricPort(t, 0, 0, 0).GetThroughput()
		require.NoError(t, err)
		assert.Equal(t, FabricPortThroughput{
			Timestamp: 777777,
			RxCounter: 111111,
			TxCounter: 222222,
		}, tp)
	})
}

func TestFabricPortGetFabricErrorCounters(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFabricPort(t, 0, 0, 0).GetFabricErrorCounters()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		counters, err := getFabricPort(t, 0, 0, 0).GetFabricErrorCounters()
		require.NoError(t, err)
		assert.EqualExportedValues(t, FabricPortErrorCounters{
			LinkFailureCount: 5,
			FwCommErrorCount: 6,
			FwErrorCount:     7,
			LinkDegradeCount: 8,
		}, counters)
	})
}

// ------------------------------------------------------------------
// Fan
// ------------------------------------------------------------------

func TestFanGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFan(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getFan(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, FanProperties{
			OnSubdevice:    1,
			SubdeviceId:    1,
			CanControl:     1,
			SupportedModes: 7,
			SupportedUnits: 3,
			MaxRPM:         3000,
			MaxPoints:      16,
		}, props)
	})
}

func TestFanGetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFan(t, 0, 0, 0).GetConfig()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		cfg, err := getFan(t, 0, 0, 0).GetConfig()
		require.NoError(t, err)
		assert.Equal(t, FanConfig{
			Mode: FAN_SPEED_MODE_FIXED,
			SpeedFixed: FanSpeed{
				Speed: 1500,
				Units: FAN_SPEED_UNITS_RPM,
			},
		}, cfg)
	})
}

func TestFanSetDefaultMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getFan(t, 0, 0, 0).SetDefaultMode()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getFan(t, 0, 0, 0).SetDefaultMode()
		require.NoError(t, err)
	})
}

func TestFanSetFixedSpeedMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getFan(t, 0, 0, 0).SetFixedSpeedMode(FanSpeed{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getFan(t, 0, 0, 0).SetFixedSpeedMode(FanSpeed{})
		require.NoError(t, err)
	})
}

func TestFanSetSpeedTableMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getFan(t, 0, 0, 0).SetSpeedTableMode(&FanSpeedTable{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getFan(t, 0, 0, 0).SetSpeedTableMode(&FanSpeedTable{})
		require.NoError(t, err)
	})
}

func TestFanGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFan(t, 0, 0, 0).GetState(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		speed, err := getFan(t, 0, 0, 0).GetState(0) // units=0 → RPM
		require.NoError(t, err)
		assert.Equal(t, int32(1500), speed, "SpeedRPM")
	})
}

// ------------------------------------------------------------------
// Firmware
// ------------------------------------------------------------------

func TestFirmwareGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFirmware(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getFirmware(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, FirmwareProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			CanControl:  1,
			Name:        stringProperty[core.StringProperty64]("GFX"),
			Version:     stringProperty[core.StringProperty64]("1.2.3.4"),
		}, props)
	})
}

func TestFirmwareFlash(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getFirmware(t, 0, 0, 0).Flash([]byte{0x00})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getFirmware(t, 0, 0, 0).Flash([]byte{0x00})
		require.NoError(t, err)
	})
}

func TestFirmwareGetFlashProgress(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFirmware(t, 0, 0, 0).GetFlashProgress()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		progress, err := getFirmware(t, 0, 0, 0).GetFlashProgress()
		require.NoError(t, err)
		assert.Equal(t, uint32(100), progress, "progress")
	})
}

func TestFirmwareGetConsoleLogs(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getFirmware(t, 0, 0, 0).GetConsoleLogs()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		logs, err := getFirmware(t, 0, 0, 0).GetConsoleLogs()
		require.NoError(t, err)
		assert.Equal(t, "test log\x00", logs, "logs")
	})
}

// ------------------------------------------------------------------
// Led
// ------------------------------------------------------------------

func TestLedGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getLed(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getLed(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, LedProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			CanControl:  1,
			HaveRGB:     1,
		}, props)
	})
}

func TestLedGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getLed(t, 0, 0, 0).GetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getLed(t, 0, 0, 0).GetState()
		require.NoError(t, err)
		assert.Equal(t, LedState{
			IsOn: 1,
			Color: LedColor{
				Red:   1.0,
				Green: 0.5,
			},
		}, state)
	})
}

func TestLedSetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getLed(t, 0, 0, 0).SetState(false)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getLed(t, 0, 0, 0).SetState(false)
		require.NoError(t, err)
	})
}

func TestLedSetColor(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getLed(t, 0, 0, 0).SetColor(LedColor{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getLed(t, 0, 0, 0).SetColor(LedColor{})
		require.NoError(t, err)
	})
}

// ------------------------------------------------------------------
// Overclock
// ------------------------------------------------------------------

func TestOverclockGetDomainProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getOcDomain(t, 0, 0, 0).GetDomainProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getOcDomain(t, 0, 0, 0).GetDomainProperties()
		require.NoError(t, err)
		assert.Equal(t, OverclockProperties{
			DomainType:        OVERCLOCK_DOMAIN_CARD,
			AvailableControls: 3,
			VFProgramType:     VF_PROGRAM_TYPE_VF_ARBITRARY,
			NumberOfVFPoints:  16,
		}, props)
	})
}

func TestOverclockGetDomainVFProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getOcDomain(t, 0, 0, 0).GetDomainVFProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getOcDomain(t, 0, 0, 0).GetDomainVFProperties()
		require.NoError(t, err)
		assert.Equal(t, VfProperty{
			MinFreq:  300.0,
			MaxFreq:  1600.0,
			StepFreq: 25.0,
			MinVolt:  700.0,
			MaxVolt:  1200.0,
			StepVolt: 10.0,
		}, props)
	})
}

func TestOverclockGetDomainControlProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getOcDomain(t, 0, 0, 0).GetDomainControlProperties(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getOcDomain(t, 0, 0, 0).GetDomainControlProperties(0)
		require.NoError(t, err)
		assert.Equal(t, ControlProperty{
			MinValue:     100.0,
			MaxValue:     2000.0,
			StepValue:    50.0,
			RefValue:     900.0,
			DefaultValue: 1100.0,
		}, props)
	})
}

func TestOverclockGetControlCurrentValue(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getOcDomain(t, 0, 0, 0).GetControlCurrentValue(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		val, err := getOcDomain(t, 0, 0, 0).GetControlCurrentValue(0)
		require.NoError(t, err)
		assert.Equal(t, 1200.0, val, "value")
	})
}

func TestOverclockGetControlPendingValue(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getOcDomain(t, 0, 0, 0).GetControlPendingValue(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		val, err := getOcDomain(t, 0, 0, 0).GetControlPendingValue(0)
		require.NoError(t, err)
		assert.Equal(t, 1100.0, val, "value")
	})
}

func TestOverclockSetControlUserValue(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getOcDomain(t, 0, 0, 0).SetControlUserValue(0, 0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		action, err := getOcDomain(t, 0, 0, 0).SetControlUserValue(0, 0)
		require.NoError(t, err)
		assert.Equal(t, PENDING_ACTION_PENDING_NONE, action, "action")
	})
}

func TestOverclockGetControlState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, _, err := getOcDomain(t, 0, 0, 0).GetControlState(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, action, err := getOcDomain(t, 0, 0, 0).GetControlState(0)
		require.NoError(t, err)
		assert.Equal(t, CONTROL_STATE_STATE_ACTIVE, state, "state")
		assert.Equal(t, PENDING_ACTION_PENDING_NONE, action, "action")
	})
}

func TestOverclockGetVFPointValues(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getOcDomain(t, 0, 0, 0).GetVFPointValues(0, 0, 0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		val, err := getOcDomain(t, 0, 0, 0).GetVFPointValues(0, 0, 0)
		require.NoError(t, err)
		assert.Equal(t, uint32(42), val, "value")
	})
}

func TestOverclockSetVFPointValues(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getOcDomain(t, 0, 0, 0).SetVFPointValues(0, 0, 0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getOcDomain(t, 0, 0, 0).SetVFPointValues(0, 0, 0)
		require.NoError(t, err)
	})
}

// ------------------------------------------------------------------
// Performance
// ------------------------------------------------------------------

func TestPerformanceGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getPerf(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getPerf(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, PerfProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			Engines:     7,
		}, props)
	})
}

func TestPerformanceGetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getPerf(t, 0, 0, 0).GetConfig()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		factor, err := getPerf(t, 0, 0, 0).GetConfig()
		require.NoError(t, err)
		assert.Equal(t, 1.0, factor, "Factor")
	})
}

func TestPerformanceSetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getPerf(t, 0, 0, 0).SetConfig(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getPerf(t, 0, 0, 0).SetConfig(0)
		require.NoError(t, err)
	})
}

// ------------------------------------------------------------------
// Psu
// ------------------------------------------------------------------

func TestPsuGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getPsu(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getPsu(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, PsuProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			HaveFan:     1,
			AmpLimit:    30,
		}, props)
	})
}

func TestPsuGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getPsu(t, 0, 0, 0).GetState()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getPsu(t, 0, 0, 0).GetState()
		require.NoError(t, err)
		assert.Equal(t, PsuState{
			VoltStatus:  PSU_VOLTAGE_STATUS_NORMAL,
			FanFailed:   0,
			Temperature: 45,
			Current:     10,
		}, state)
	})
}

// ------------------------------------------------------------------
// Ras
// ------------------------------------------------------------------

func TestRasGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getRas(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getRas(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, RasProperties{
			Type:        RAS_ERROR_TYPE_CORRECTABLE,
			OnSubdevice: 1,
			SubdeviceId: 1,
		}, props)
	})
}

func TestRasGetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getRas(t, 0, 0, 0).GetConfig()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		cfg, err := getRas(t, 0, 0, 0).GetConfig()
		require.NoError(t, err)
		assert.EqualExportedValues(t, RasConfig{
			TotalThreshold: 100,
			DetailedThresholds: RasState{Category: [7]uint64{
				10, 11, 12, 13, 14, 15, 16,
			}},
		}, cfg)
	})
}

func TestRasSetConfig(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getRas(t, 0, 0, 0).SetConfig(&RasConfig{})
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getRas(t, 0, 0, 0).SetConfig(&RasConfig{})
		require.NoError(t, err)
	})
}

func TestRasGetState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getRas(t, 0, 0, 0).GetState(false)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		state, err := getRas(t, 0, 0, 0).GetState(false)
		require.NoError(t, err)
		assert.EqualExportedValues(t, RasState{
			Category: [7]uint64{5, 6, 7, 8, 9, 10, 11},
		}, state)
	})
}

// ------------------------------------------------------------------
// Standby
// ------------------------------------------------------------------

func TestStandbyGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getStandby(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getStandby(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, StandbyProperties{
			Type:        STANDBY_TYPE_GLOBAL,
			OnSubdevice: 0,
			SubdeviceId: 0,
		}, props)
	})
}

func TestStandbyGetMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getStandby(t, 0, 0, 0).GetMode()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		mode, err := getStandby(t, 0, 0, 0).GetMode()
		require.NoError(t, err)
		assert.Equal(t, STANDBY_PROMO_MODE_DEFAULT, mode, "Mode")
	})
}

func TestStandbySetMode(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		err := getStandby(t, 0, 0, 0).SetMode(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		err := getStandby(t, 0, 0, 0).SetMode(0)
		require.NoError(t, err)
	})
}

// ------------------------------------------------------------------
// Diagnostics
// ------------------------------------------------------------------

func TestDiagnosticsGetProperties(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getDiagnostic(t, 0, 0, 0).GetProperties()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		props, err := getDiagnostic(t, 0, 0, 0).GetProperties()
		require.NoError(t, err)
		assert.EqualExportedValues(t, DiagProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			Name:        stringProperty[core.StringProperty64]("GPU"),
			HaveTests:   0,
		}, props)
	})
}

func TestDiagnosticsGetTests(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getDiagnostic(t, 0, 0, 0).GetTests()
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		tests, err := getDiagnostic(t, 0, 0, 0).GetTests()
		require.NoError(t, err)
		require.Empty(t, tests)
	})
}

func TestDiagnosticsRunTests(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponent)
		_, err := getDiagnostic(t, 0, 0, 0).RunTests(0, 0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDefault)
		results, err := getDiagnostic(t, 0, 0, 0).RunTests(0, 0)
		require.NoError(t, err)
		require.Len(t, results, 1)
		assert.Equal(t, DIAG_RESULT_NO_ERRORS, results[0], "results[0]")
	})
}
