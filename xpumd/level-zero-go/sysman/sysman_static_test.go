// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Intel Corporation

package sysman

import (
	"testing"
	"time"
	"unsafe"

	"github.com/google/uuid"
	"github.com/intel/level-zero-go/core"
	th "github.com/intel/level-zero-go/internal/testhelper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

// Driver config paths (relative to the sysman/ package directory, i.e. go test CWD).
const (
	driverConfigDefault       = th.ConfigDefault
	driverConfigDriverGetErr  = "testdata/error_driver_get.yaml"
	driverConfigDriverErrs    = "testdata/error_driver.yaml"
	driverConfigDeviceErrs    = "testdata/error_device.yaml"
	driverConfigComponentErrs = "testdata/error_component.yaml"
)

// getDriver returns drivers[idx] from the currently-loaded stub state.
func getDriver(t *testing.T, idx int) *Driver {
	t.Helper()
	return th.GetIndexed(t, "driver", idx, DriverGet)
}

// getDevice returns drivers[drvIdx].devices[idx] from the currently-loaded stub state.
func getDevice(t *testing.T, drvIdx, idx int) *Device {
	t.Helper()
	return th.GetIndexed(t, "device", idx, getDriver(t, drvIdx).DeviceGet)
}

// getComponent is a generic helper that enumerates components on a device and returns
// the component at the given index.
func getComponent[C any](t *testing.T, drvIdx, devIdx, idx int, enum func(*Device) ([]*C, error)) *C {
	t.Helper()
	dev := getDevice(t, drvIdx, devIdx)
	return th.GetIndexed(t, "component", idx, func() ([]*C, error) { return enum(dev) })
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

// The object getters below return the object under test, as addressed by the
// indices of the test case config.
func driverGetter(cfg *th.Config) func(*testing.T) *Driver {
	return func(t *testing.T) *Driver {
		t.Helper()
		return getDriver(t, cfg.DrvIdx)
	}
}

func deviceGetter(cfg *th.Config) func(*testing.T) *Device {
	return func(t *testing.T) *Device {
		t.Helper()
		return getDevice(t, cfg.DrvIdx, cfg.DevIdx)
	}
}

func componentGetter[C any](cfg *th.Config, getComp func(*testing.T, int, int, int) *C) func(*testing.T) *C {
	return func(t *testing.T) *C {
		t.Helper()
		return getComp(t, cfg.DrvIdx, cfg.DevIdx, cfg.CompIdx)
	}
}

func testDriverGetterError[R any](t *testing.T, method func(*Driver) (R, error), opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Error", core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS, opts...)
	th.Getter(t, cfg, driverGetter(cfg), method, nil)
}

func testDriverGetterSuccess[R any](t *testing.T, method func(*Driver) (R, error), check func(*testing.T, R), opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Success", nil, opts...)
	th.Getter(t, cfg, driverGetter(cfg), method, check)
}

func testDeviceGetterError[R any](t *testing.T, method func(*Device) (R, error), opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Error", core.RESULT_ERROR_INVALID_ARGUMENT, opts...)
	th.Getter(t, cfg, deviceGetter(cfg), method, nil)
}

func testDeviceGetterSuccess[R any](t *testing.T, method func(*Device) (R, error), check func(*testing.T, R), opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Success", nil, opts...)
	th.Getter(t, cfg, deviceGetter(cfg), method, check)
}

func testComponentGetterError[C, R any](t *testing.T, getComp func(*testing.T, int, int, int) *C, method func(*C) (R, error), opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Error", core.RESULT_ERROR_NOT_AVAILABLE, opts...)
	th.Getter(t, cfg, componentGetter(cfg, getComp), method, nil)
}

func testComponentGetterSuccess[C, R any](t *testing.T, getComp func(*testing.T, int, int, int) *C, method func(*C) (R, error), check func(*testing.T, R), opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Success", nil, opts...)
	th.Getter(t, cfg, componentGetter(cfg, getComp), method, check)
}

func testDeviceActionError(t *testing.T, action func(*Device) error, opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Error", core.RESULT_ERROR_INVALID_ARGUMENT, opts...)
	th.Action(t, cfg, deviceGetter(cfg), action)
}

func testDeviceActionSuccess(t *testing.T, action func(*Device) error, opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Success", nil, opts...)
	th.Action(t, cfg, deviceGetter(cfg), action)
}

func testComponentActionError[C any](t *testing.T, getComp func(*testing.T, int, int, int) *C, action func(*C) error, opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Error", core.RESULT_ERROR_NOT_AVAILABLE, opts...)
	th.Action(t, cfg, componentGetter(cfg, getComp), action)
}

func testComponentActionSuccess[C any](t *testing.T, getComp func(*testing.T, int, int, int) *C, action func(*C) error, opts ...th.Opt) {
	t.Helper()
	cfg := th.NewConfig("Success", nil, opts...)
	th.Action(t, cfg, componentGetter(cfg, getComp), action)
}

// ------------------------------------------------------------------
// Handle getters
// ------------------------------------------------------------------

// component is implemented by all wrapper types of the device components (Engine, Memory, etc).
type component interface {
	Handle() unsafe.Pointer
	Device() *Device
}

func testHandleGetters[C component](t *testing.T, name string, getComp func(*testing.T, int, int, int) C) {
	t.Helper()
	t.Run(name, func(t *testing.T) {
		comp := getComp(t, 0, 0, 0)
		assert.NotNil(t, comp.Handle())
		require.NotNil(t, comp.Device())
		assert.Equal(t, getDevice(t, 0, 0).getHandle(), comp.Device().getHandle())
	})
}

func TestHandleGetters(t *testing.T) {
	th.LoadConfig(t, driverConfigDefault)
	t.Run("Driver", func(t *testing.T) {
		assert.NotNil(t, getDriver(t, 0).Handle())
	})
	t.Run("Device", func(t *testing.T) {
		assert.NotNil(t, getDevice(t, 0, 0).Handle())
	})
	testHandleGetters(t, "Diagnostics", getDiagnostic)
	testHandleGetters(t, "Engine", getEngine)
	testHandleGetters(t, "FabricPort", getFabricPort)
	testHandleGetters(t, "Fan", getFan)
	testHandleGetters(t, "Firmware", getFirmware)
	testHandleGetters(t, "Frequency", getFrequency)
	testHandleGetters(t, "Led", getLed)
	testHandleGetters(t, "Memory", getMemory)
	testHandleGetters(t, "Overclock", getOcDomain)
	testHandleGetters(t, "Performance", getPerf)
	testHandleGetters(t, "Power", getPower)
	testHandleGetters(t, "Psu", getPsu)
	testHandleGetters(t, "Ras", getRas)
	testHandleGetters(t, "Scheduler", getScheduler)
	testHandleGetters(t, "Standby", getStandby)
	testHandleGetters(t, "Temperature", getTemperature)
}

func testHasExtension(t *testing.T, name string, drvIdx int, extensions []string) {
	t.Helper()
	t.Run(name, func(t *testing.T) {
		drv := getDriver(t, drvIdx)
		dev := getDevice(t, drvIdx, 0)

		// Devices see the extensions of their driver
		for _, ext := range extensions {
			assert.True(t, drv.HasExtension(ext), "driver extension %q", ext)
			assert.True(t, dev.HasExtension(ext), "device extension %q", ext)
		}
		assert.False(t, drv.HasExtension("ZES_extension_nonexistent"))
		assert.False(t, dev.HasExtension("ZES_extension_nonexistent"))
	})
}

func TestHasExtension(t *testing.T) {
	th.LoadConfig(t, driverConfigDefault)
	testHasExtension(t, "Driver0", 0, []string{
		"ZES_extension_ras_state",
		"ZES_extension_bar",
	})
	testHasExtension(t, "Driver1", 1, []string{
		DEVICE_ECC_DEFAULT_PROPERTIES_EXT_NAME,
		DEVICE_EXT_STATE_NAME,
		ENGINE_ACTIVITY_EXT_NAME,
		OEM_SERIAL_ID_EXT_NAME,
		PCI_LINK_SPEED_DOWNGRADE_EXT_NAME,
		POWER_LIMITS_EXT_NAME,
	})
}

// ------------------------------------------------------------------
// Init
// ------------------------------------------------------------------

func TestInit(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDriverGetErr)
		err := Init(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_UNINITIALIZED)
	})

	t.Run("Success", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDefault)
		err := Init(0)
		require.NoError(t, err)
	})
}

// ------------------------------------------------------------------
// Driver
// ------------------------------------------------------------------

func TestDriverGet(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDriverGetErr)
		_, err := DriverGet()
		require.ErrorIs(t, err, core.RESULT_ERROR_UNINITIALIZED)
	})

	t.Run("Success", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDefault)
		drivers, err := DriverGet()
		require.NoError(t, err)
		require.Len(t, drivers, 2)
	})
}

func TestDriverGetExtensionProperties(t *testing.T) {
	// DriverGet silently swallows GetExtensionProperties errors, so call directly.
	testDriverGetterError(t, (*Driver).GetExtensionProperties, th.WithConfig(driverConfigDriverErrs))
	testDriverGetterSuccess(t, (*Driver).GetExtensionProperties,
		th.CheckValue([]DriverExtensionProperties{
			{Name: th.StringProperty[core.StringProperty256]("ZES_extension_ras_state"), Version: 1},
			{Name: th.StringProperty[core.StringProperty256]("ZES_extension_bar"), Version: 2},
		}),
	)
}

func TestDriverDeviceGet(t *testing.T) {
	testDriverGetterError(t, (*Driver).DeviceGet, th.WithConfig(driverConfigDriverErrs))
	testDriverGetterSuccess(t, (*Driver).DeviceGet, th.CheckLen[*Device](1))
}

func TestDriverEventListen(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDriverErrs)
		drv := getDriver(t, 0)
		_, _, err := drv.EventListen(time.Second, nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})

	t.Run("Success", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDefault)
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
		th.LoadConfig(t, driverConfigDriverErrs)
		drv := getDriver(t, 0)
		_, _, err := drv.EventListenEx(time.Second, nil)
		require.ErrorIs(t, err, core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS)
	})

	t.Run("Success", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDefault)
		drv := getDriver(t, 0)
		dev := getDevice(t, 0, 0)
		numEvents, events, err := drv.EventListenEx(0, []*Device{dev})
		require.NoError(t, err)
		assert.Equal(t, uint32(1), numEvents)
		assert.Equal(t, EventTypeFlags(EVENT_TYPE_FLAG_DEVICE_DETACH), events[0])
	})
}

func TestDeviceGetProperties(t *testing.T) {
	testDeviceGetterError(t, (*Device).GetProperties, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).GetProperties,
		th.CheckValueExported(DeviceProperties{
			DeviceBaseProperties: DeviceBaseProperties{
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
					Name: th.StringProperty[core.StringProperty256]("ACME Data Center GPU"),
				},
				NumSubdevices: 2,
				SerialNumber:  th.StringProperty[core.StringProperty64]("SN-0001"),
				BoardNumber:   th.StringProperty[core.StringProperty64]("BOARD-0001"),
				BrandName:     th.StringProperty[core.StringProperty64]("ACME"),
				ModelName:     th.StringProperty[core.StringProperty64]("Stub GPU 1234"),
				VendorName:    th.StringProperty[core.StringProperty64]("ACME Corporation"),
				DriverVersion: th.StringProperty[core.StringProperty64]("1.2.3"),
			},
		}),
	)
	testDeviceGetterSuccess(t, (*Device).GetProperties,
		th.CheckValueExported(DeviceProperties{
			DeviceExtProperties: DeviceExtProperties{
				Uuid: Uuid{
					Id: uuid.MustParse("abcdef01-2345-6789-abcd-ef0000000000"),
				},
				Type:  DEVICE_TYPE_GPU,
				Flags: 1,
			},
			OemSerialId: "OEM-SN-0001",
		}),
		th.WithDrvIdx(1), th.WithName("SuccessWithExtProps"),
	)
}

func TestDeviceGetState(t *testing.T) {
	testDeviceGetterError(t, (*Device).GetState, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).GetState,
		th.CheckValue(DeviceState{
			DeviceBaseState: DeviceBaseState{
				Reset:    0,
				Repaired: REPAIR_STATUS_NOT_PERFORMED,
			},
		}),
	)
	testDeviceGetterSuccess(t, (*Device).GetState,
		th.CheckValueExported(DeviceState{
			DeviceBaseState: DeviceBaseState{
				Reset:    0,
				Repaired: REPAIR_STATUS_NOT_PERFORMED,
			},
			ExtendedState: &DeviceExtState{
				Flags: DeviceStateExtFlags(DEVICE_STATE_EXT_FLAG_WEDGED),
			},
		}),
		th.WithDrvIdx(1), th.WithName("SuccessWithExtProps"),
	)
}

func TestDeviceReset(t *testing.T) {
	testDeviceActionError(t, func(d *Device) error { return d.Reset(false) }, th.WithConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, func(d *Device) error { return d.Reset(false) })
}

func TestDeviceResetExt(t *testing.T) {
	testDeviceActionError(t, func(d *Device) error { return d.ResetExt(nil) }, th.WithConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, func(d *Device) error { return d.ResetExt(nil) })
}

func TestDeviceProcessesGetState(t *testing.T) {
	testDeviceGetterError(t, (*Device).ProcessesGetState, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).ProcessesGetState,
		th.CheckValue([]ProcessState{
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
		}),
	)
}

func TestDeviceEventRegister(t *testing.T) {
	events := EventTypeFlags(EVENT_TYPE_FLAG_DEVICE_DETACH | EVENT_TYPE_FLAG_DEVICE_ATTACH | EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED)
	eventRegister := func(d *Device) (EventTypeFlags, error) { return d.EventRegister(events) }

	testDeviceGetterError(t, eventRegister, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, eventRegister, th.CheckValue(events))
}

// ------------------------------------------------------------------
// Device.Pci
// ------------------------------------------------------------------

func TestDevicePciGetProperties(t *testing.T) {
	testDeviceGetterError(t, (*Device).PciGetProperties, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).PciGetProperties,
		th.CheckValue(PciProperties{
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
		}),
	)
	testDeviceGetterSuccess(t, (*Device).PciGetProperties,
		th.CheckValueExported(PciProperties{
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
		}),
		th.WithDrvIdx(1), th.WithName("SuccessWithExt"),
	)
}

func TestDevicePciGetState(t *testing.T) {
	testDeviceGetterError(t, (*Device).PciGetState, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).PciGetState,
		func(t *testing.T, state PciState) {
			assert.Equal(t, PciState{
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

	testDeviceGetterSuccess(t, (*Device).PciGetState,
		th.CheckValueExported(PciState{
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
		}),
		th.WithDrvIdx(1), th.WithName("SuccessWithExt"))
}

func TestDevicePciLinkSpeedUpdateExt(t *testing.T) {
	testDeviceGetterError(t,
		func(d *Device) (DeviceAction, error) { return d.PciLinkSpeedUpdateExt(true) },
		th.WithConfig(driverConfigDeviceErrs))

	testDeviceGetterSuccess(t,
		func(d *Device) (DeviceAction, error) { return d.PciLinkSpeedUpdateExt(true) },
		th.CheckValue(DEVICE_ACTION_WARM_CARD_RESET),
		th.WithDrvIdx(1))
}

func TestDevicePciGetBars(t *testing.T) {
	testDeviceGetterError(t, (*Device).PciGetBars, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).PciGetBars,
		th.CheckValue([]PciBarProperties{
			{Type: PCI_BAR_TYPE_MMIO, Index: 0, Base: 0x80000000, Size: 16777216},
			{Type: PCI_BAR_TYPE_MEM, Index: 2, Base: 0x100000000, Size: 268435456},
		}),
	)
}

func TestDevicePciGetStats(t *testing.T) {
	testDeviceGetterError(t, (*Device).PciGetStats, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).PciGetStats,
		th.CheckValue(PciStats{
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
		}),
	)
}

// ------------------------------------------------------------------
// Device overclock helpers (no component handle needed)
// ------------------------------------------------------------------

func TestDeviceSetOverclockWaiver(t *testing.T) {
	testDeviceActionError(t, (*Device).SetOverclockWaiver, th.WithConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, (*Device).SetOverclockWaiver)
}

func TestDeviceGetOverclockDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).GetOverclockDomains, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).GetOverclockDomains,
		th.CheckValue(OverclockDomains(OVERCLOCK_DOMAIN_CARD|OVERCLOCK_DOMAIN_PACKAGE)),
	)
}

func TestDeviceGetOverclockControls(t *testing.T) {
	getOverclockControls := func(d *Device) (OverclockControls, error) { return d.GetOverclockControls(0) }

	testDeviceGetterError(t, getOverclockControls, th.WithConfig(driverConfigDeviceErrs))

	testDeviceGetterSuccess(t, getOverclockControls,
		th.CheckValue(OverclockControls(OVERCLOCK_CONTROL_VF|OVERCLOCK_CONTROL_FREQ_OFFSET|OVERCLOCK_CONTROL_VMAX_OFFSET)),
	)
}

func TestDeviceResetOverclockSettings(t *testing.T) {
	testDeviceActionError(t, func(d *Device) error { return d.ResetOverclockSettings(false) }, th.WithConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, func(d *Device) error { return d.ResetOverclockSettings(false) })
}

func TestDeviceReadOverclockState(t *testing.T) {
	testDeviceGetterError(t, (*Device).ReadOverclockState, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).ReadOverclockState,
		th.CheckValue(OverclockState{
			Mode:          OVERCLOCK_MODE_MODE_ON,
			WaiverSetting: true,
			State:         false,
			PendingAction: PENDING_ACTION_PENDING_NONE,
			PendingReset:  false,
		}),
	)
}

func TestDeviceEccAvailable(t *testing.T) {
	testDeviceGetterError(t, (*Device).EccAvailable, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EccAvailable,
		func(t *testing.T, available bool) { assert.True(t, available, "EccAvailable") },
	)
}

func TestDeviceEccConfigurable(t *testing.T) {
	testDeviceGetterError(t, (*Device).EccConfigurable, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EccConfigurable,
		func(t *testing.T, configurable bool) { assert.True(t, configurable, "EccConfigurable") },
	)
}

func TestDeviceGetEccState(t *testing.T) {
	testDeviceGetterError(t, (*Device).GetEccState, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).GetEccState,
		th.CheckValue(EccProperties{
			DeviceEccProperties: DeviceEccProperties{
				CurrentState:  DEVICE_ECC_STATE_ENABLED,
				PendingState:  DEVICE_ECC_STATE_ENABLED,
				PendingAction: DEVICE_ACTION_NONE,
			},
		}),
	)
	testDeviceGetterSuccess(t, (*Device).GetEccState,
		th.CheckValueExported(EccProperties{
			DeviceEccProperties: DeviceEccProperties{},
			ExtendedProperties: &DeviceEccDefaultPropertiesExt{
				DefaultState: DEVICE_ECC_STATE_ENABLED,
			},
		}),
		th.WithDrvIdx(1), th.WithName("SuccessWithExtProps"),
	)
}

func TestDeviceSetEccState(t *testing.T) {
	setEccState := func(d *Device) (DeviceEccProperties, error) { return d.SetEccState(DeviceEccDesc{}) }

	testDeviceGetterError(t, setEccState,
		th.WithConfig(driverConfigDeviceErrs),
	)
	testDeviceGetterSuccess(t, setEccState,
		th.CheckValue(DeviceEccProperties{
			CurrentState: DEVICE_ECC_STATE_ENABLED,
			PendingState: DEVICE_ECC_STATE_ENABLED,
		}),
	)
}

// ------------------------------------------------------------------
// Device enum methods
// ------------------------------------------------------------------

func TestDeviceEnumEngineGroups(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumEngineGroups, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumEngineGroups, th.CheckLen[*Engine](3))
}

func TestDeviceEnumFrequencyDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumFrequencyDomains, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumFrequencyDomains, th.CheckLen[*Frequency](2))
}

func TestDeviceEnumMemoryModules(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumMemoryModules, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumMemoryModules, th.CheckLen[*Memory](2))
}

func TestDeviceEnumPowerDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumPowerDomains, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumPowerDomains, th.CheckLen[*Power](2))
}

func TestDeviceEnumSchedulers(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumSchedulers, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumSchedulers, th.CheckLen[*Scheduler](2))
}

func TestDeviceEnumTemperatureSensors(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumTemperatureSensors, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumTemperatureSensors, th.CheckLen[*Temperature](3))
}

func TestDeviceEnumFabricPorts(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumFabricPorts, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumFabricPorts, th.CheckLen[*FabricPort](2))
}

func TestDeviceEnumFans(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumFans, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumFans, th.CheckLen[*Fan](2))
}

func TestDeviceEnumFirmwares(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumFirmwares, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumFirmwares, th.CheckLen[*Firmware](2))
}

func TestDeviceEnumLeds(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumLeds, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumLeds, th.CheckLen[*Led](2))
}

func TestDeviceEnumOverclockDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumOverclockDomains, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumOverclockDomains, th.CheckLen[*Overclock](1))
}

func TestDeviceEnumPerformanceFactorDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumPerformanceFactorDomains, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumPerformanceFactorDomains, th.CheckLen[*Performance](2))
}

func TestDeviceEnumPsus(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumPsus, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumPsus, th.CheckLen[*Psu](2))
}

func TestDeviceEnumRasErrorSets(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumRasErrorSets, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumRasErrorSets, th.CheckLen[*Ras](2))
}

func TestDeviceEnumStandbyDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumStandbyDomains, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumStandbyDomains, th.CheckLen[*Standby](1))
}

func TestDeviceEnumDiagnosticTestSuites(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumDiagnosticTestSuites, th.WithConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumDiagnosticTestSuites, th.CheckLen[*Diagnostics](2))
}

func TestDeviceFabricPortGetMultiPortThroughput(t *testing.T) {
	fabricPortGetMultiPortThroughput := func(d *Device) ([]FabricPortThroughput, error) {
		// Pass nil (count=0) to avoid the CGo restriction on Go pointer slices.
		return d.FabricPortGetMultiPortThroughput(nil)
	}

	testDeviceGetterError(t,
		fabricPortGetMultiPortThroughput,
		th.WithConfig(driverConfigComponentErrs), th.WithError(core.RESULT_ERROR_NOT_AVAILABLE),
	)
	testDeviceGetterSuccess(t,
		fabricPortGetMultiPortThroughput,
		func(t *testing.T, results []FabricPortThroughput) { require.Empty(t, results) },
	)
}

// ------------------------------------------------------------------
// Engine
// ------------------------------------------------------------------

func TestEngineGetProperties(t *testing.T) {
	testComponentGetterError(t, getEngine, (*Engine).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getEngine, (*Engine).GetProperties,
		func(t *testing.T, props EngineProperties) {
			assert.Equal(t, EngineBaseProperties{
				Type: ENGINE_GROUP_ALL,
			}, props.EngineBaseProperties)
		},
	)
	testComponentGetterSuccess(t, getEngine, (*Engine).GetProperties,
		th.CheckValueExported(EngineProperties{
			EngineBaseProperties: EngineBaseProperties{
				Type: ENGINE_GROUP_ALL,
			},
			ExtendedProperties: &EngineExtProperties{
				CountOfVirtualFunctionInstance: 4,
			},
		}),
		th.WithDrvIdx(1), th.WithName("SuccessWithExtProps"),
	)
}

func TestEngineGetActivity(t *testing.T) {
	testComponentGetterError(t, getEngine, (*Engine).GetActivity, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getEngine, (*Engine).GetActivity,
		th.CheckValue(EngineStats{
			ActiveTime: 12345678,
			Timestamp:  87654321,
		}),
	)
}

func TestEngineGetActivityExt(t *testing.T) {
	testComponentGetterError(t, getEngine, (*Engine).GetActivityExt, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getEngine, (*Engine).GetActivityExt,
		func(t *testing.T, stats []EngineStats) {
			require.Empty(t, stats)
		},
	)
}

// ------------------------------------------------------------------
// Frequency
// ------------------------------------------------------------------

func TestFrequencyGetProperties(t *testing.T) {
	testComponentGetterError(t, getFrequency, (*Frequency).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetProperties,
		th.CheckValue(FreqProperties{
			Type:                     FREQ_DOMAIN_GPU,
			OnSubdevice:              1,
			SubdeviceId:              1,
			CanControl:               1,
			IsThrottleEventSupported: 1,
			Min:                      300.0,
			Max:                      1600.0,
		}),
	)
}

func TestFrequencyGetAvailableClocks(t *testing.T) {
	testComponentGetterError(t, getFrequency, (*Frequency).GetAvailableClocks, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetAvailableClocks,
		func(t *testing.T, clocks []float64) {
			require.Len(t, clocks, 3)
			assert.Equal(t, 300.0, clocks[0], "clocks[0]")
			assert.Equal(t, 900.0, clocks[1], "clocks[1]")
			assert.Equal(t, 1600.0, clocks[2], "clocks[2]")
		},
	)
}

func TestFrequencyGetRange(t *testing.T) {
	testComponentGetterError(t, getFrequency, (*Frequency).GetRange, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetRange,
		th.CheckValue(FreqRange{
			Min: 300.0,
			Max: 1600.0,
		}),
	)
}

func TestFrequencySetRange(t *testing.T) {
	testComponentActionError(t, getFrequency, func(f *Frequency) error { return f.SetRange(&FreqRange{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFrequency, func(f *Frequency) error { return f.SetRange(&FreqRange{}) })
}

func TestFrequencyGetState(t *testing.T) {
	testComponentGetterError(t, getFrequency, (*Frequency).GetState, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetState,
		th.CheckValue(FreqState{
			CurrentVoltage: 0.85,
			Request:        1400.0,
			Tdp:            1600.0,
			Efficient:      300.0,
			Actual:         1200.0,
			ThrottleReasons: FreqThrottleReasonFlags(
				FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT | FREQ_THROTTLE_REASON_FLAG_SW_RANGE,
			),
		}),
	)
}

func TestFrequencyGetThrottleTime(t *testing.T) {
	testComponentGetterError(t, getFrequency, (*Frequency).GetThrottleTime, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetThrottleTime,
		th.CheckValue(FreqThrottleTime{
			ThrottleTime: 5000,
			Timestamp:    100000,
		}),
	)
}

// ------------------------------------------------------------------
// Memory
// ------------------------------------------------------------------

func TestMemoryGetProperties(t *testing.T) {
	testComponentGetterError(t, getMemory, (*Memory).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getMemory, (*Memory).GetProperties,
		th.CheckValue(MemProperties{
			Type:         MEM_TYPE_HBM,
			OnSubdevice:  1,
			SubdeviceId:  1,
			Location:     MEM_LOC_DEVICE,
			PhysicalSize: 17179869184,
			BusWidth:     128,
			NumChannels:  8,
		}),
	)
}

func TestMemoryGetState(t *testing.T) {
	testComponentGetterError(t, getMemory, (*Memory).GetState, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getMemory, (*Memory).GetState,
		th.CheckValue(MemState{
			Health: MEM_HEALTH_OK,
			Free:   8589934592,
			Size:   17179869184,
		}),
	)
}

func TestMemoryGetBandwidth(t *testing.T) {
	testComponentGetterError(t, getMemory, (*Memory).GetBandwidth, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getMemory, (*Memory).GetBandwidth,
		th.CheckValue(MemBandwidth{
			ReadCounter:  1073741824,
			WriteCounter: 536870912,
			MaxBandwidth: 512000000000,
			Timestamp:    123456789,
		}),
	)
}

// ------------------------------------------------------------------
// Power
// ------------------------------------------------------------------

func TestPowerGetProperties(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetProperties,
		func(t *testing.T, props PowerProperties) {
			assert.Equal(t, PowerBaseProperties{
				OnSubdevice:                1,
				SubdeviceId:                1,
				CanControl:                 1,
				IsEnergyThresholdSupported: 1,
				DefaultLimit:               200000,
				MinLimit:                   100000,
				MaxLimit:                   300000,
			}, props.PowerBaseProperties)
		},
	)
	testComponentGetterSuccess(t, getPower, (*Power).GetProperties,
		th.CheckValueExported(PowerProperties{
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
		}),
		th.WithDrvIdx(1), th.WithName("SuccessWithExtProps"),
	)
}

func TestPowerGetEnergyCounter(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetEnergyCounter, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetEnergyCounter,
		th.CheckValue(PowerEnergyCounter{
			Energy:    5000000,
			Timestamp: 123456789,
		}),
	)
}

func TestPowerGetEnergyThreshold(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetEnergyThreshold, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetEnergyThreshold,
		th.CheckValue(EnergyThreshold{
			Enable:    1,
			Threshold: 10.5,
			ProcessId: 4321,
		}),
	)
}

func TestPowerSetEnergyThreshold(t *testing.T) {
	testComponentActionError(t, getPower, func(p *Power) error { return p.SetEnergyThreshold(0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getPower, func(p *Power) error { return p.SetEnergyThreshold(0) })
}

func TestPowerGetLimitsExt(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetLimitsExt, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetLimitsExt,
		th.CheckValue([]PowerLimitExtDesc{
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
		}),
	)
}

func TestPowerSetLimitsExt(t *testing.T) {
	testComponentActionError(t, getPower, func(p *Power) error { return p.SetLimitsExt(nil) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getPower, func(p *Power) error { return p.SetLimitsExt(nil) })
}

func TestPowerGetUsage(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetUsage, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetUsage,
		th.CheckValue(PowerUsage{
			InstantPower: 180000,
			AveragePower: 160000,
		}),
	)
}

func TestPowerGetLimitsExt2(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetLimitsExt2, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetLimitsExt2, th.CheckValue(uint32(200000)))
}

func TestPowerSetLimitsExt2(t *testing.T) {
	testComponentActionError(t, getPower, func(p *Power) error { return p.SetLimitsExt2(0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getPower, func(p *Power) error { return p.SetLimitsExt2(0) })
}

// ------------------------------------------------------------------
// Scheduler
// ------------------------------------------------------------------

func TestSchedulerGetProperties(t *testing.T) {
	testComponentGetterError(t, getScheduler, (*Scheduler).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler, (*Scheduler).GetProperties,
		th.CheckValue(SchedProperties{
			OnSubdevice:    0,
			SubdeviceId:    0,
			CanControl:     1,
			SupportedModes: 7,
			Engines:        7,
		}),
	)
}

func TestSchedulerGetCurrentMode(t *testing.T) {
	testComponentGetterError(t, getScheduler, (*Scheduler).GetCurrentMode, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler, (*Scheduler).GetCurrentMode,
		th.CheckValue(SCHED_MODE_TIMESLICE),
	)
}

func TestSchedulerGetTimeoutModeProperties(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (SchedTimeoutProperties, error) { return s.GetTimeoutModeProperties(false) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (SchedTimeoutProperties, error) { return s.GetTimeoutModeProperties(false) },
		th.CheckValue(SchedTimeoutProperties{
			WatchdogTimeout: 5000000,
		}),
	)
}

func TestSchedulerGetTimesliceModeProperties(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (SchedTimesliceProperties, error) { return s.GetTimesliceModeProperties(false) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (SchedTimesliceProperties, error) { return s.GetTimesliceModeProperties(false) },
		th.CheckValue(SchedTimesliceProperties{
			Interval:     2000,
			YieldTimeout: 500,
		}),
	)
}

func TestSchedulerSetTimeoutMode(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (bool, error) { return s.SetTimeoutMode(&SchedTimeoutProperties{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (bool, error) { return s.SetTimeoutMode(&SchedTimeoutProperties{}) },
		func(t *testing.T, needReload bool) { assert.False(t, needReload, "needReload") },
	)
}

func TestSchedulerSetTimesliceMode(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (bool, error) { return s.SetTimesliceMode(&SchedTimesliceProperties{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (bool, error) { return s.SetTimesliceMode(&SchedTimesliceProperties{}) },
		func(t *testing.T, needReload bool) { assert.False(t, needReload, "needReload") },
	)
}

func TestSchedulerSetExclusiveMode(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (bool, error) { return s.SetExclusiveMode() }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (bool, error) { return s.SetExclusiveMode() },
		func(t *testing.T, needReload bool) { assert.False(t, needReload, "needReload") },
	)
}

// ------------------------------------------------------------------
// Temperature
// ------------------------------------------------------------------

func TestTemperatureGetProperties(t *testing.T) {
	testComponentGetterError(t, getTemperature, (*Temperature).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getTemperature, (*Temperature).GetProperties,
		th.CheckValue(TempProperties{
			OnSubdevice:             1,
			SubdeviceId:             1,
			Type:                    TEMP_SENSORS_GLOBAL,
			MaxTemperature:          110.0,
			IsCriticalTempSupported: 1,
			IsThreshold1Supported:   1,
			IsThreshold2Supported:   1,
		}),
	)
}

func TestTemperatureGetConfig(t *testing.T) {
	testComponentGetterError(t, getTemperature, (*Temperature).GetConfig, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getTemperature, (*Temperature).GetConfig,
		th.CheckValue(TempConfig{
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
		}),
	)
}

func TestTemperatureSetConfig(t *testing.T) {
	testComponentActionError(t, getTemperature, func(temp *Temperature) error { return temp.SetConfig(&TempConfig{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getTemperature, func(temp *Temperature) error { return temp.SetConfig(&TempConfig{}) })
}

func TestTemperatureGetState(t *testing.T) {
	testComponentGetterError(t, getTemperature, (*Temperature).GetState, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getTemperature, (*Temperature).GetState,
		th.CheckValue(48.0),
	)
}

// ------------------------------------------------------------------
// FabricPort
// ------------------------------------------------------------------

func TestFabricPortGetProperties(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetProperties,
		th.CheckValue(FabricPortProperties{
			Model:       th.StringProperty[core.StringProperty256]("Xe-Link"),
			OnSubdevice: 1,
			SubdeviceId: 1,
			PortId: FabricPortId{
				FabricId:   10,
				AttachId:   20,
				PortNumber: 3,
			},
			MaxRxSpeed: FabricPortSpeed{BitRate: 53125000000, Width: 4},
			MaxTxSpeed: FabricPortSpeed{BitRate: 50000000000, Width: 4},
		}),
	)
}

func TestFabricPortGetLinkType(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetLinkType, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetLinkType,
		th.CheckValue(FabricLinkType{
			Desc: th.StringProperty[core.StringProperty256]("Xe-Link"),
		}),
	)
}

func TestFabricPortGetConfig(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetConfig, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetConfig,
		th.CheckValue(FabricPortConfig{
			Enabled:   1,
			Beaconing: 1,
		}),
	)
}

func TestFabricPortSetConfig(t *testing.T) {
	testComponentActionError(t, getFabricPort, func(fp *FabricPort) error { return fp.SetConfig(&FabricPortConfig{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFabricPort, func(fp *FabricPort) error { return fp.SetConfig(&FabricPortConfig{}) })
}

func TestFabricPortGetState(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetState, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetState,
		th.CheckValue(FabricPortState{
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
		}),
	)
}

func TestFabricPortGetThroughput(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetThroughput, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetThroughput,
		th.CheckValue(FabricPortThroughput{
			Timestamp: 777777,
			RxCounter: 111111,
			TxCounter: 222222,
		}),
	)
}

func TestFabricPortGetFabricErrorCounters(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetFabricErrorCounters, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetFabricErrorCounters,
		th.CheckValue(FabricPortErrorCounters{
			LinkFailureCount: 5,
			FwCommErrorCount: 6,
			FwErrorCount:     7,
			LinkDegradeCount: 8,
		}),
	)
}

// ------------------------------------------------------------------
// Fan
// ------------------------------------------------------------------

func TestFanGetProperties(t *testing.T) {
	testComponentGetterError(t, getFan, (*Fan).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFan, (*Fan).GetProperties,
		th.CheckValue(FanProperties{
			OnSubdevice:    1,
			SubdeviceId:    1,
			CanControl:     1,
			SupportedModes: 7,
			SupportedUnits: 3,
			MaxRPM:         3000,
			MaxPoints:      16,
		}),
	)
}

func TestFanGetConfig(t *testing.T) {
	testComponentGetterError(t, getFan, (*Fan).GetConfig, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFan, (*Fan).GetConfig,
		th.CheckValue(FanConfig{
			Mode: FAN_SPEED_MODE_FIXED,
			SpeedFixed: FanSpeed{
				Speed: 1500,
				Units: FAN_SPEED_UNITS_RPM,
			},
		}),
	)
}

func TestFanSetDefaultMode(t *testing.T) {
	testComponentActionError(t, getFan, (*Fan).SetDefaultMode, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFan, (*Fan).SetDefaultMode)
}

func TestFanSetFixedSpeedMode(t *testing.T) {
	testComponentActionError(t, getFan, func(f *Fan) error { return f.SetFixedSpeedMode(FanSpeed{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFan, func(f *Fan) error { return f.SetFixedSpeedMode(FanSpeed{}) })
}

func TestFanSetSpeedTableMode(t *testing.T) {
	testComponentActionError(t, getFan, func(f *Fan) error { return f.SetSpeedTableMode(&FanSpeedTable{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFan, func(f *Fan) error { return f.SetSpeedTableMode(&FanSpeedTable{}) })
}

func TestFanGetState(t *testing.T) {
	testComponentGetterError(t, getFan, func(f *Fan) (int32, error) { return f.GetState(0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFan,
		func(f *Fan) (int32, error) { return f.GetState(0) },
		th.CheckValue(int32(1500)),
	)
}

// ------------------------------------------------------------------
// Firmware
// ------------------------------------------------------------------

func TestFirmwareGetProperties(t *testing.T) {
	testComponentGetterError(t, getFirmware, (*Firmware).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFirmware, (*Firmware).GetProperties,
		th.CheckValue(FirmwareProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			CanControl:  1,
			Name:        th.StringProperty[core.StringProperty64]("GFX"),
			Version:     th.StringProperty[core.StringProperty64]("1.2.3.4"),
		}),
	)
}

func TestFirmwareFlash(t *testing.T) {
	testComponentActionError(t, getFirmware, func(fw *Firmware) error { return fw.Flash([]byte{0x00}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFirmware, func(fw *Firmware) error { return fw.Flash([]byte{0x00}) })
}

func TestFirmwareGetFlashProgress(t *testing.T) {
	testComponentGetterError(t, getFirmware, (*Firmware).GetFlashProgress, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFirmware, (*Firmware).GetFlashProgress,
		th.CheckValue(uint32(100)),
	)
}

func TestFirmwareGetConsoleLogs(t *testing.T) {
	testComponentGetterError(t, getFirmware, (*Firmware).GetConsoleLogs, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFirmware, (*Firmware).GetConsoleLogs,
		th.CheckValue("test log\x00"),
	)
}

// ------------------------------------------------------------------
// Led
// ------------------------------------------------------------------

func TestLedGetProperties(t *testing.T) {
	testComponentGetterError(t, getLed, (*Led).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getLed, (*Led).GetProperties,
		th.CheckValue(LedProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			CanControl:  1,
			HaveRGB:     1,
		}),
	)
}

func TestLedGetState(t *testing.T) {
	testComponentGetterError(t, getLed, (*Led).GetState, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getLed, (*Led).GetState,
		th.CheckValue(LedState{
			IsOn: 1,
			Color: LedColor{
				Red:   1.0,
				Green: 0.5,
			},
		}),
	)
}

func TestLedSetState(t *testing.T) {
	testComponentActionError(t, getLed, func(l *Led) error { return l.SetState(false) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getLed, func(l *Led) error { return l.SetState(false) })
}

func TestLedSetColor(t *testing.T) {
	testComponentActionError(t, getLed, func(l *Led) error { return l.SetColor(LedColor{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getLed, func(l *Led) error { return l.SetColor(LedColor{}) })
}

// ------------------------------------------------------------------
// Overclock
// ------------------------------------------------------------------

func TestOverclockGetDomainProperties(t *testing.T) {
	testComponentGetterError(t, getOcDomain, (*Overclock).GetDomainProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain, (*Overclock).GetDomainProperties,
		th.CheckValue(OverclockProperties{
			DomainType:        OVERCLOCK_DOMAIN_CARD,
			AvailableControls: 3,
			VFProgramType:     VF_PROGRAM_TYPE_VF_ARBITRARY,
			NumberOfVFPoints:  16,
		}),
	)
}

func TestOverclockGetDomainVFProperties(t *testing.T) {
	testComponentGetterError(t, getOcDomain, (*Overclock).GetDomainVFProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain, (*Overclock).GetDomainVFProperties,
		th.CheckValue(VfProperty{
			MinFreq:  300.0,
			MaxFreq:  1600.0,
			StepFreq: 25.0,
			MinVolt:  700.0,
			MaxVolt:  1200.0,
			StepVolt: 10.0,
		}),
	)
}

func TestOverclockGetDomainControlProperties(t *testing.T) {
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (ControlProperty, error) { return oc.GetDomainControlProperties(0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (ControlProperty, error) { return oc.GetDomainControlProperties(0) },
		th.CheckValue(ControlProperty{
			MinValue:     100.0,
			MaxValue:     2000.0,
			StepValue:    50.0,
			RefValue:     900.0,
			DefaultValue: 1100.0,
		}),
	)
}

func TestOverclockGetControlCurrentValue(t *testing.T) {
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (float64, error) { return oc.GetControlCurrentValue(0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (float64, error) { return oc.GetControlCurrentValue(0) },
		th.CheckValue(1200.0),
	)
}

func TestOverclockGetControlPendingValue(t *testing.T) {
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (float64, error) { return oc.GetControlPendingValue(0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (float64, error) { return oc.GetControlPendingValue(0) },
		th.CheckValue(1100.0),
	)
}

func TestOverclockSetControlUserValue(t *testing.T) {
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (PendingAction, error) { return oc.SetControlUserValue(0, 0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (PendingAction, error) { return oc.SetControlUserValue(0, 0) },
		th.CheckValue(PENDING_ACTION_PENDING_NONE),
	)
}

func TestOverclockGetControlState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		th.LoadConfig(t, driverConfigComponentErrs)
		_, _, err := getOcDomain(t, 0, 0, 0).GetControlState(0)
		require.ErrorIs(t, err, core.RESULT_ERROR_NOT_AVAILABLE)
	})
	t.Run("Success", func(t *testing.T) {
		th.LoadConfig(t, driverConfigDefault)
		state, action, err := getOcDomain(t, 0, 0, 0).GetControlState(0)
		require.NoError(t, err)
		assert.Equal(t, CONTROL_STATE_STATE_ACTIVE, state, "state")
		assert.Equal(t, PENDING_ACTION_PENDING_NONE, action, "action")
	})
}

func TestOverclockGetVFPointValues(t *testing.T) {
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (uint32, error) { return oc.GetVFPointValues(0, 0, 0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (uint32, error) { return oc.GetVFPointValues(0, 0, 0) },
		th.CheckValue(uint32(42)),
	)
}

func TestOverclockSetVFPointValues(t *testing.T) {
	testComponentActionError(t, getOcDomain, func(oc *Overclock) error { return oc.SetVFPointValues(0, 0, 0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getOcDomain, func(oc *Overclock) error { return oc.SetVFPointValues(0, 0, 0) })
}

// ------------------------------------------------------------------
// Performance
// ------------------------------------------------------------------

func TestPerformanceGetProperties(t *testing.T) {
	testComponentGetterError(t, getPerf, (*Performance).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPerf, (*Performance).GetProperties,
		th.CheckValue(PerfProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			Engines:     7,
		}),
	)
}

func TestPerformanceGetConfig(t *testing.T) {
	testComponentGetterError(t, getPerf, (*Performance).GetConfig, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPerf, (*Performance).GetConfig,
		th.CheckValue(1.0),
	)
}

func TestPerformanceSetConfig(t *testing.T) {
	testComponentActionError(t, getPerf, func(p *Performance) error { return p.SetConfig(0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getPerf, func(p *Performance) error { return p.SetConfig(0) })
}

// ------------------------------------------------------------------
// Psu
// ------------------------------------------------------------------

func TestPsuGetProperties(t *testing.T) {
	testComponentGetterError(t, getPsu, (*Psu).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPsu, (*Psu).GetProperties,
		th.CheckValue(PsuProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			HaveFan:     1,
			AmpLimit:    30,
		}),
	)
}

func TestPsuGetState(t *testing.T) {
	testComponentGetterError(t, getPsu, (*Psu).GetState, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPsu, (*Psu).GetState,
		th.CheckValue(PsuState{
			VoltStatus:  PSU_VOLTAGE_STATUS_NORMAL,
			FanFailed:   0,
			Temperature: 45,
			Current:     10,
		}),
	)
}

// ------------------------------------------------------------------
// Ras
// ------------------------------------------------------------------

func TestRasGetProperties(t *testing.T) {
	testComponentGetterError(t, getRas, (*Ras).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getRas, (*Ras).GetProperties,
		th.CheckValue(RasProperties{
			Type:        RAS_ERROR_TYPE_CORRECTABLE,
			OnSubdevice: 1,
			SubdeviceId: 1,
		}),
		th.WithCompIdx(0), th.WithName("SuccessCorrectable"),
	)
	testComponentGetterSuccess(t, getRas, (*Ras).GetProperties,
		th.CheckValue(RasProperties{
			Type:        RAS_ERROR_TYPE_UNCORRECTABLE,
			OnSubdevice: 1,
			SubdeviceId: 1,
		}),
		th.WithCompIdx(1), th.WithName("SuccessUncorrectable"),
	)
}

func TestRasGetConfig(t *testing.T) {
	testComponentGetterError(t, getRas, (*Ras).GetConfig, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getRas, (*Ras).GetConfig,
		th.CheckValue(RasConfig{
			TotalThreshold: 100,
			DetailedThresholds: RasState{Category: [7]uint64{
				10, 11, 12, 13, 14, 15, 16,
			}},
		}),
	)
}

func TestRasSetConfig(t *testing.T) {
	testComponentActionError(t, getRas, func(r *Ras) error { return r.SetConfig(&RasConfig{}) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getRas, func(r *Ras) error { return r.SetConfig(&RasConfig{}) })
}

func TestRasGetState(t *testing.T) {
	testComponentGetterError(t, getRas, func(r *Ras) (RasState, error) { return r.GetState(false) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getRas,
		func(r *Ras) (RasState, error) { return r.GetState(false) },
		th.CheckValue(RasState{
			Category: [7]uint64{0, 0, 0, 8, 9, 10, 11},
		}),
		th.WithCompIdx(0), th.WithName("SuccessCorrectable"),
	)
	testComponentGetterSuccess(t, getRas,
		func(r *Ras) (RasState, error) { return r.GetState(false) },
		th.CheckValue(RasState{
			Category: [7]uint64{3, 2, 1, 0, 0, 0, 0},
		}),
		th.WithCompIdx(1), th.WithName("SuccessUncorrectable"),
	)
}

// ------------------------------------------------------------------
// Standby
// ------------------------------------------------------------------

func TestStandbyGetProperties(t *testing.T) {
	testComponentGetterError(t, getStandby, (*Standby).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getStandby, (*Standby).GetProperties,
		th.CheckValue(StandbyProperties{
			Type:        STANDBY_TYPE_GLOBAL,
			OnSubdevice: 0,
			SubdeviceId: 0,
		}),
	)
}

func TestStandbyGetMode(t *testing.T) {
	testComponentGetterError(t, getStandby, (*Standby).GetMode, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getStandby, (*Standby).GetMode,
		th.CheckValue(STANDBY_PROMO_MODE_DEFAULT),
	)
}

func TestStandbySetMode(t *testing.T) {
	testComponentActionError(t, getStandby, func(s *Standby) error { return s.SetMode(0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getStandby, func(s *Standby) error { return s.SetMode(0) })
}

// ------------------------------------------------------------------
// Diagnostics
// ------------------------------------------------------------------

func TestDiagnosticsGetProperties(t *testing.T) {
	testComponentGetterError(t, getDiagnostic, (*Diagnostics).GetProperties, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getDiagnostic, (*Diagnostics).GetProperties,
		th.CheckValue(DiagProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			Name:        th.StringProperty[core.StringProperty64]("GPU"),
			HaveTests:   0,
		}),
	)
}

func TestDiagnosticsGetTests(t *testing.T) {
	testComponentGetterError(t, getDiagnostic, (*Diagnostics).GetTests, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getDiagnostic, (*Diagnostics).GetTests,
		func(t *testing.T, tests []DiagTest) {
			require.Empty(t, tests)
		},
	)
}

func TestDiagnosticsRunTests(t *testing.T) {
	testComponentGetterError(t, getDiagnostic, func(d *Diagnostics) ([]DiagResult, error) { return d.RunTests(0, 0) }, th.WithConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getDiagnostic,
		func(d *Diagnostics) ([]DiagResult, error) { return d.RunTests(0, 0) },
		func(t *testing.T, results []DiagResult) {
			require.Len(t, results, 1)
			assert.Equal(t, DIAG_RESULT_NO_ERRORS, results[0], "results[0]")
		},
	)
}
