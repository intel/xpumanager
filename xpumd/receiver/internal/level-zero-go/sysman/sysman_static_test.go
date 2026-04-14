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
	driverConfigDefault       = "testdata/default.yaml"
	driverConfigDriverGetErr  = "testdata/error_driver_get.yaml"
	driverConfigDriverErrs    = "testdata/error_driver.yaml"
	driverConfigDeviceErrs    = "testdata/error_device.yaml"
	driverConfigComponentErrs = "testdata/error_component.yaml"
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

// testBaseCfg holds the minimal config needed by the base test helpers.
type testBaseCfg struct {
	name    string
	cfgFile string
	wantErr error
}

// testCfg extends testBaseCfg with addressing fields for device/component wrappers.
type testCfg struct {
	testBaseCfg
	drvIdx  int
	devIdx  int
	compIdx int
}

func (c *testCfg) getDriver(t *testing.T) *Driver {
	t.Helper()
	return getDriver(t, c.drvIdx)
}

func (c *testCfg) getDevice(t *testing.T) *Device {
	t.Helper()
	return getDevice(t, c.drvIdx, c.devIdx)
}

type testOpt func(*testCfg)

func withConfig(cfg string) testOpt { return func(c *testCfg) { c.cfgFile = cfg } }
func withError(err error) testOpt   { return func(c *testCfg) { c.wantErr = err } }
func withDrvIdx(idx int) testOpt    { return func(c *testCfg) { c.drvIdx = idx } }
func withDevIdx(idx int) testOpt    { return func(c *testCfg) { c.devIdx = idx } }  //nolint: unused // Included for completeness and future use
func withCompIdx(idx int) testOpt   { return func(c *testCfg) { c.compIdx = idx } } //nolint: unused // Included for completeness and future use
func withName(name string) testOpt  { return func(c *testCfg) { c.name = name } }

func (c *testCfg) applyOpts(opts ...testOpt) {
	for _, o := range opts {
		o(c)
	}
}

// testGetter is a base helper for testing getter-like functions (that return a value and an error).
func testGetter[T, R any](t *testing.T, getObj func(*testing.T) T, method func(T) (R, error), check func(*testing.T, R), bcfg *testBaseCfg) {
	t.Helper()
	t.Run(bcfg.name, func(t *testing.T) {
		cfgFile := bcfg.cfgFile
		if cfgFile == "" {
			cfgFile = driverConfigDefault
		}
		loadDriverConfig(t, cfgFile)
		got, err := method(getObj(t))
		if bcfg.wantErr != nil {
			require.ErrorIs(t, err, bcfg.wantErr)
		} else {
			require.NoError(t, err)
		}
		if check != nil {
			check(t, got)
		}
	})
}

// testAction is a base helper for testing action-like functions (that only return an error).
func testAction[T any](t *testing.T, getObj func(*testing.T) T, action func(T) error, bcfg *testBaseCfg) {
	t.Helper()
	t.Run(bcfg.name, func(t *testing.T) {
		cfgFile := bcfg.cfgFile
		if cfgFile == "" {
			cfgFile = driverConfigDefault
		}
		loadDriverConfig(t, cfgFile)
		err := action(getObj(t))
		if bcfg.wantErr != nil {
			require.ErrorIs(t, err, bcfg.wantErr)
		} else {
			require.NoError(t, err)
		}
	})
}

func testDriverGetterError[R any](t *testing.T, method func(*Driver) (R, error), opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Error", wantErr: core.RESULT_ERROR_INSUFFICIENT_PERMISSIONS}}
	cfg.applyOpts(opts...)
	testGetter(t, cfg.getDriver, method, nil, &cfg.testBaseCfg)
}

func testDriverGetterSuccess[R any](t *testing.T, method func(*Driver) (R, error), check func(*testing.T, R), opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Success"}}
	cfg.applyOpts(opts...)
	testGetter(t, cfg.getDriver, method, check, &cfg.testBaseCfg)
}

func testDeviceGetterError[R any](t *testing.T, method func(*Device) (R, error), opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Error", wantErr: core.RESULT_ERROR_INVALID_ARGUMENT}}
	cfg.applyOpts(opts...)
	testGetter(t, cfg.getDevice, method, nil, &cfg.testBaseCfg)
}

func testDeviceGetterSuccess[R any](t *testing.T, method func(*Device) (R, error), check func(*testing.T, R), opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Success"}}
	cfg.applyOpts(opts...)
	testGetter(t, cfg.getDevice, method, check, &cfg.testBaseCfg)
}

func testComponentGetterError[C, R any](t *testing.T, getComp func(*testing.T, int, int, int) *C, method func(*C) (R, error), opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Error", wantErr: core.RESULT_ERROR_NOT_AVAILABLE}}
	cfg.applyOpts(opts...)
	getter := func(t *testing.T) *C { return getComp(t, cfg.drvIdx, cfg.devIdx, cfg.compIdx) }
	testGetter(t, getter, method, nil, &cfg.testBaseCfg)
}

func testComponentGetterSuccess[C, R any](t *testing.T, getComp func(*testing.T, int, int, int) *C, method func(*C) (R, error), check func(*testing.T, R), opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Success"}}
	cfg.applyOpts(opts...)
	getter := func(t *testing.T) *C { return getComp(t, cfg.drvIdx, cfg.devIdx, cfg.compIdx) }
	testGetter(t, getter, method, check, &cfg.testBaseCfg)
}

func testDeviceActionError(t *testing.T, action func(*Device) error, opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Error", wantErr: core.RESULT_ERROR_INVALID_ARGUMENT}}
	cfg.applyOpts(opts...)
	testAction(t, cfg.getDevice, action, &cfg.testBaseCfg)
}

func testDeviceActionSuccess(t *testing.T, action func(*Device) error, opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Success"}}
	cfg.applyOpts(opts...)
	testAction(t, cfg.getDevice, action, &cfg.testBaseCfg)
}

func testComponentActionError[C any](t *testing.T, getComp func(*testing.T, int, int, int) *C, action func(*C) error, opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Error", wantErr: core.RESULT_ERROR_NOT_AVAILABLE}}
	cfg.applyOpts(opts...)
	getter := func(t *testing.T) *C { return getComp(t, cfg.drvIdx, cfg.devIdx, cfg.compIdx) }
	testAction(t, getter, action, &cfg.testBaseCfg)
}

func testComponentActionSuccess[C any](t *testing.T, getComp func(*testing.T, int, int, int) *C, action func(*C) error, opts ...testOpt) {
	t.Helper()
	cfg := &testCfg{testBaseCfg: testBaseCfg{name: "Success"}}
	cfg.applyOpts(opts...)
	getter := func(t *testing.T) *C { return getComp(t, cfg.drvIdx, cfg.devIdx, cfg.compIdx) }
	testAction(t, getter, action, &cfg.testBaseCfg)
}

// checkValue returns a check function that asserts a value.
func checkValue[T any](want T) func(*testing.T, T) {
	return func(t *testing.T, got T) {
		t.Helper()
		assert.Equal(t, want, got)
	}
}

// checkValueExported returns a check function that asserts exported fields of a value.
func checkValueExported[T any](want T) func(*testing.T, T) {
	return func(t *testing.T, got T) {
		t.Helper()
		assert.EqualExportedValues(t, want, got)
	}
}

// checkLen returns a check function that asserts the length of a slice.
func checkLen[T any](n int) func(*testing.T, []T) {
	return func(t *testing.T, v []T) {
		t.Helper()
		require.Len(t, v, n)
	}
}

// ------------------------------------------------------------------
// Init
// ------------------------------------------------------------------

func TestInit(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDriverGetErr)
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
		loadDriverConfig(t, driverConfigDriverGetErr)
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
	// DriverGet silently swallows GetExtensionProperties errors, so call directly.
	testDriverGetterError(t, (*Driver).GetExtensionProperties, withConfig(driverConfigDriverErrs))
	testDriverGetterSuccess(t, (*Driver).GetExtensionProperties,
		checkValue([]DriverExtensionProperties{
			{Name: stringProperty[core.StringProperty256]("ZES_extension_foo"), Version: 1},
			{Name: stringProperty[core.StringProperty256]("ZES_extension_bar"), Version: 2},
		}),
	)
}

func TestDriverDeviceGet(t *testing.T) {
	testDriverGetterError(t, (*Driver).DeviceGet, withConfig(driverConfigDriverErrs))
	testDriverGetterSuccess(t, (*Driver).DeviceGet, checkLen[*Device](1))
}

func TestDriverEventListen(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigDriverErrs)
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
		loadDriverConfig(t, driverConfigDriverErrs)
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
	testDeviceGetterError(t, (*Device).GetProperties, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).GetProperties,
		checkValueExported(DeviceProperties{
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
					Name: stringProperty[core.StringProperty256]("ACME Data Center GPU"),
				},
				NumSubdevices: 2,
				SerialNumber:  stringProperty[core.StringProperty64]("SN-0001"),
				BoardNumber:   stringProperty[core.StringProperty64]("BOARD-0001"),
				BrandName:     stringProperty[core.StringProperty64]("ACME"),
				ModelName:     stringProperty[core.StringProperty64]("Stub GPU 1234"),
				VendorName:    stringProperty[core.StringProperty64]("ACME Corporation"),
				DriverVersion: stringProperty[core.StringProperty64]("1.2.3"),
			},
		}),
	)
	testDeviceGetterSuccess(t, (*Device).GetProperties,
		checkValueExported(DeviceProperties{
			DeviceExtProperties: DeviceExtProperties{
				Uuid: Uuid{
					Id: uuid.MustParse("abcdef01-2345-6789-abcd-ef0000000000"),
				},
				Type:  DEVICE_TYPE_GPU,
				Flags: 1,
			},
		}),
		withDrvIdx(1), withName("SuccessWithExtProps"),
	)
}

func TestDeviceGetState(t *testing.T) {
	testDeviceGetterError(t, (*Device).GetState, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).GetState,
		checkValue(DeviceState{
			Reset:    0,
			Repaired: REPAIR_STATUS_NOT_PERFORMED,
		}),
	)
}

func TestDeviceReset(t *testing.T) {
	testDeviceActionError(t, func(d *Device) error { return d.Reset(false) }, withConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, func(d *Device) error { return d.Reset(false) })
}

func TestDeviceResetExt(t *testing.T) {
	testDeviceActionError(t, func(d *Device) error { return d.ResetExt(nil) }, withConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, func(d *Device) error { return d.ResetExt(nil) })
}

func TestDeviceProcessesGetState(t *testing.T) {
	testDeviceGetterError(t, (*Device).ProcessesGetState, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).ProcessesGetState,
		checkValue([]ProcessState{
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
	testDeviceActionError(t, func(d *Device) error { return d.EventRegister(0) }, withConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, func(d *Device) error { return d.EventRegister(0) })
}

// ------------------------------------------------------------------
// Device.Pci
// ------------------------------------------------------------------

func TestDevicePciGetProperties(t *testing.T) {
	testDeviceGetterError(t, (*Device).PciGetProperties, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).PciGetProperties,
		checkValue(PciProperties{
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
		checkValueExported(PciProperties{
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
		withDrvIdx(1), withName("SuccessWithExt"),
	)
}

func TestDevicePciGetState(t *testing.T) {
	testDeviceGetterError(t, (*Device).PciGetState, withConfig(driverConfigDeviceErrs))
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
		checkValueExported(PciState{
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
		withDrvIdx(1), withName("SuccessWithExt"))
}

func TestDevicePciLinkSpeedUpdateExt(t *testing.T) {
	testDeviceGetterError(t,
		func(d *Device) (DeviceAction, error) { return d.PciLinkSpeedUpdateExt(true) },
		withConfig(driverConfigDeviceErrs))

	testDeviceGetterSuccess(t,
		func(d *Device) (DeviceAction, error) { return d.PciLinkSpeedUpdateExt(true) },
		checkValue(DEVICE_ACTION_WARM_CARD_RESET),
		withDrvIdx(1))
}

func TestDevicePciGetBars(t *testing.T) {
	testDeviceGetterError(t, (*Device).PciGetBars, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).PciGetBars,
		checkValue([]PciBarProperties{
			{Type: PCI_BAR_TYPE_MMIO, Index: 0, Base: 0x80000000, Size: 16777216},
			{Type: PCI_BAR_TYPE_MEM, Index: 2, Base: 0x100000000, Size: 268435456},
		}),
	)
}

func TestDevicePciGetStats(t *testing.T) {
	testDeviceGetterError(t, (*Device).PciGetStats, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).PciGetStats,
		checkValue(PciStats{
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
	testDeviceActionError(t, (*Device).SetOverclockWaiver, withConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, (*Device).SetOverclockWaiver)
}

func TestDeviceGetOverclockDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).GetOverclockDomains, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).GetOverclockDomains,
		checkValue(OverclockDomains(OVERCLOCK_DOMAIN_CARD|OVERCLOCK_DOMAIN_PACKAGE)),
	)
}

func TestDeviceGetOverclockControls(t *testing.T) {
	testDeviceGetterError(t,
		func(d *Device) (OverclockControls, error) { return d.GetOverclockControls(0) },
		withConfig(driverConfigDeviceErrs))

	testDeviceGetterSuccess(t,
		func(d *Device) (OverclockControls, error) { return d.GetOverclockControls(0) },
		checkValue(OverclockControls(OVERCLOCK_CONTROL_VF|OVERCLOCK_CONTROL_FREQ_OFFSET|OVERCLOCK_CONTROL_VMAX_OFFSET)),
	)
}

func TestDeviceResetOverclockSettings(t *testing.T) {
	testDeviceActionError(t, func(d *Device) error { return d.ResetOverclockSettings(false) }, withConfig(driverConfigDeviceErrs))
	testDeviceActionSuccess(t, func(d *Device) error { return d.ResetOverclockSettings(false) })
}

func TestDeviceReadOverclockState(t *testing.T) {
	testDeviceGetterError(t, (*Device).ReadOverclockState, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).ReadOverclockState,
		checkValue(OverclockState{
			Mode:          OVERCLOCK_MODE_MODE_ON,
			WaiverSetting: true,
			State:         false,
			PendingAction: PENDING_ACTION_PENDING_NONE,
			PendingReset:  false,
		}),
	)
}

func TestDeviceEccAvailable(t *testing.T) {
	testDeviceGetterError(t, (*Device).EccAvailable, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EccAvailable,
		func(t *testing.T, available bool) { assert.True(t, available, "EccAvailable") },
	)
}

func TestDeviceEccConfigurable(t *testing.T) {
	testDeviceGetterError(t, (*Device).EccConfigurable, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EccConfigurable,
		func(t *testing.T, configurable bool) { assert.True(t, configurable, "EccConfigurable") },
	)
}

func TestDeviceGetEccState(t *testing.T) {
	testDeviceGetterError(t, (*Device).GetEccState, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).GetEccState,
		checkValue(EccProperties{
			DeviceEccProperties: DeviceEccProperties{
				CurrentState:  DEVICE_ECC_STATE_ENABLED,
				PendingState:  DEVICE_ECC_STATE_ENABLED,
				PendingAction: DEVICE_ACTION_NONE,
			},
		}),
	)
	testDeviceGetterSuccess(t, (*Device).GetEccState,
		checkValueExported(EccProperties{
			DeviceEccProperties: DeviceEccProperties{},
			ExtendedProperties: &DeviceEccDefaultPropertiesExt{
				DefaultState: DEVICE_ECC_STATE_ENABLED,
			},
		}),
		withDrvIdx(1), withName("SuccessWithExtProps"),
	)
}

func TestDeviceSetEccState(t *testing.T) {
	testDeviceGetterError(t,
		func(d *Device) (DeviceEccProperties, error) { return d.SetEccState(DeviceEccDesc{}) },
		withConfig(driverConfigDeviceErrs),
	)
	testDeviceGetterSuccess(t,
		func(d *Device) (DeviceEccProperties, error) { return d.SetEccState(DeviceEccDesc{}) },
		checkValue(DeviceEccProperties{
			CurrentState: DEVICE_ECC_STATE_ENABLED,
			PendingState: DEVICE_ECC_STATE_ENABLED,
		}),
	)
}

// ------------------------------------------------------------------
// Device enum methods
// ------------------------------------------------------------------

func TestDeviceEnumEngineGroups(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumEngineGroups, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumEngineGroups, checkLen[*Engine](3))
}

func TestDeviceEnumFrequencyDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumFrequencyDomains, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumFrequencyDomains, checkLen[*Frequency](2))
}

func TestDeviceEnumMemoryModules(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumMemoryModules, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumMemoryModules, checkLen[*Memory](2))
}

func TestDeviceEnumPowerDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumPowerDomains, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumPowerDomains, checkLen[*Power](2))
}

func TestDeviceEnumSchedulers(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumSchedulers, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumSchedulers, checkLen[*Scheduler](2))
}

func TestDeviceEnumTemperatureSensors(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumTemperatureSensors, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumTemperatureSensors, checkLen[*Temperature](3))
}

func TestDeviceEnumFabricPorts(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumFabricPorts, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumFabricPorts, checkLen[*FabricPort](2))
}

func TestDeviceEnumFans(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumFans, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumFans, checkLen[*Fan](2))
}

func TestDeviceEnumFirmwares(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumFirmwares, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumFirmwares, checkLen[*Firmware](2))
}

func TestDeviceEnumLeds(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumLeds, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumLeds, checkLen[*Led](2))
}

func TestDeviceEnumOverclockDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumOverclockDomains, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumOverclockDomains, checkLen[*Overclock](1))
}

func TestDeviceEnumPerformanceFactorDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumPerformanceFactorDomains, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumPerformanceFactorDomains, checkLen[*Performance](2))
}

func TestDeviceEnumPsus(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumPsus, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumPsus, checkLen[*Psu](2))
}

func TestDeviceEnumRasErrorSets(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumRasErrorSets, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumRasErrorSets, checkLen[*Ras](2))
}

func TestDeviceEnumStandbyDomains(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumStandbyDomains, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumStandbyDomains, checkLen[*Standby](1))
}

func TestDeviceEnumDiagnosticTestSuites(t *testing.T) {
	testDeviceGetterError(t, (*Device).EnumDiagnosticTestSuites, withConfig(driverConfigDeviceErrs))
	testDeviceGetterSuccess(t, (*Device).EnumDiagnosticTestSuites, checkLen[*Diagnostics](2))
}

func TestDeviceFabricPortGetMultiPortThroughput(t *testing.T) {
	testDeviceGetterError(t,
		func(d *Device) ([]FabricPortThroughput, error) { return d.FabricPortGetMultiPortThroughput(nil) },
		withConfig(driverConfigComponentErrs), withError(core.RESULT_ERROR_NOT_AVAILABLE),
	)
	testDeviceGetterSuccess(t,
		// Pass nil (count=0) to avoid the CGo restriction on Go pointer slices.
		func(d *Device) ([]FabricPortThroughput, error) { return d.FabricPortGetMultiPortThroughput(nil) },
		func(t *testing.T, results []FabricPortThroughput) { require.Empty(t, results) },
	)
}

// ------------------------------------------------------------------
// Engine
// ------------------------------------------------------------------

func TestEngineGetProperties(t *testing.T) {
	testComponentGetterError(t, getEngine, (*Engine).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getEngine, (*Engine).GetProperties,
		func(t *testing.T, props EngineProperties) {
			assert.Equal(t, EngineBaseProperties{
				Type: ENGINE_GROUP_ALL,
			}, props.EngineBaseProperties)
		},
	)
	testComponentGetterSuccess(t, getEngine, (*Engine).GetProperties,
		checkValueExported(EngineProperties{
			EngineBaseProperties: EngineBaseProperties{
				Type: ENGINE_GROUP_ALL,
			},
			ExtendedProperties: &EngineExtProperties{
				CountOfVirtualFunctionInstance: 4,
			},
		}),
		withDrvIdx(1), withName("SuccessWithExtProps"),
	)
}

func TestEngineGetActivity(t *testing.T) {
	testComponentGetterError(t, getEngine, (*Engine).GetActivity, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getEngine, (*Engine).GetActivity,
		checkValue(EngineStats{ActiveTime: 12345678, Timestamp: 87654321}),
	)
}

func TestEngineGetActivityExt(t *testing.T) {
	testComponentGetterError(t, getEngine, (*Engine).GetActivityExt, withConfig(driverConfigComponentErrs))
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
	testComponentGetterError(t, getFrequency, (*Frequency).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetProperties,
		checkValue(FreqProperties{
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
	testComponentGetterError(t, getFrequency, (*Frequency).GetAvailableClocks, withConfig(driverConfigComponentErrs))
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
	testComponentGetterError(t, getFrequency, (*Frequency).GetRange, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetRange,
		checkValue(FreqRange{Min: 300.0, Max: 1600.0}),
	)
}

func TestFrequencySetRange(t *testing.T) {
	testComponentActionError(t, getFrequency, func(f *Frequency) error { return f.SetRange(&FreqRange{}) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFrequency, func(f *Frequency) error { return f.SetRange(&FreqRange{}) })
}

func TestFrequencyGetState(t *testing.T) {
	testComponentGetterError(t, getFrequency, (*Frequency).GetState, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetState,
		checkValue(FreqState{
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
	testComponentGetterError(t, getFrequency, (*Frequency).GetThrottleTime, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFrequency, (*Frequency).GetThrottleTime,
		checkValue(FreqThrottleTime{ThrottleTime: 5000, Timestamp: 100000}),
	)
}

// ------------------------------------------------------------------
// Memory
// ------------------------------------------------------------------

func TestMemoryGetProperties(t *testing.T) {
	testComponentGetterError(t, getMemory, (*Memory).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getMemory, (*Memory).GetProperties,
		checkValue(MemProperties{
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
	testComponentGetterError(t, getMemory, (*Memory).GetState, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getMemory, (*Memory).GetState,
		checkValue(MemState{
			Health: MEM_HEALTH_OK,
			Free:   8589934592,
			Size:   17179869184,
		}),
	)
}

func TestMemoryGetBandwidth(t *testing.T) {
	testComponentGetterError(t, getMemory, (*Memory).GetBandwidth, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getMemory, (*Memory).GetBandwidth,
		checkValue(MemBandwidth{
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
	testComponentGetterError(t, getPower, (*Power).GetProperties, withConfig(driverConfigComponentErrs))
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
		checkValueExported(PowerProperties{
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
		withDrvIdx(1), withName("SuccessWithExtProps"),
	)
}

func TestPowerGetEnergyCounter(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetEnergyCounter, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetEnergyCounter,
		checkValue(PowerEnergyCounter{Energy: 5000000, Timestamp: 123456789}),
	)
}

func TestPowerGetEnergyThreshold(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetEnergyThreshold, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetEnergyThreshold,
		checkValue(EnergyThreshold{Enable: 1, Threshold: 10.5, ProcessId: 4321}),
	)
}

func TestPowerSetEnergyThreshold(t *testing.T) {
	testComponentActionError(t, getPower, func(p *Power) error { return p.SetEnergyThreshold(0) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getPower, func(p *Power) error { return p.SetEnergyThreshold(0) })
}

func TestPowerGetLimitsExt(t *testing.T) {
	testComponentGetterError(t, getPower, (*Power).GetLimitsExt, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPower, (*Power).GetLimitsExt,
		checkValue([]PowerLimitExtDesc{
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
	testComponentActionError(t, getPower, func(p *Power) error { return p.SetLimitsExt(nil) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getPower, func(p *Power) error { return p.SetLimitsExt(nil) })
}

// ------------------------------------------------------------------
// Scheduler
// ------------------------------------------------------------------

func TestSchedulerGetProperties(t *testing.T) {
	testComponentGetterError(t, getScheduler, (*Scheduler).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler, (*Scheduler).GetProperties,
		checkValue(SchedProperties{
			OnSubdevice:    0,
			SubdeviceId:    0,
			CanControl:     1,
			SupportedModes: 7,
			Engines:        7,
		}),
	)
}

func TestSchedulerGetCurrentMode(t *testing.T) {
	testComponentGetterError(t, getScheduler, (*Scheduler).GetCurrentMode, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler, (*Scheduler).GetCurrentMode,
		checkValue(SCHED_MODE_TIMESLICE),
	)
}

func TestSchedulerGetTimeoutModeProperties(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (SchedTimeoutProperties, error) { return s.GetTimeoutModeProperties(false) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (SchedTimeoutProperties, error) { return s.GetTimeoutModeProperties(false) },
		checkValue(SchedTimeoutProperties{WatchdogTimeout: 5000000}),
	)
}

func TestSchedulerGetTimesliceModeProperties(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (SchedTimesliceProperties, error) { return s.GetTimesliceModeProperties(false) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (SchedTimesliceProperties, error) { return s.GetTimesliceModeProperties(false) },
		checkValue(SchedTimesliceProperties{Interval: 2000, YieldTimeout: 500}),
	)
}

func TestSchedulerSetTimeoutMode(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (bool, error) { return s.SetTimeoutMode(&SchedTimeoutProperties{}) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (bool, error) { return s.SetTimeoutMode(&SchedTimeoutProperties{}) },
		func(t *testing.T, needReload bool) { assert.False(t, needReload, "needReload") },
	)
}

func TestSchedulerSetTimesliceMode(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (bool, error) { return s.SetTimesliceMode(&SchedTimesliceProperties{}) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (bool, error) { return s.SetTimesliceMode(&SchedTimesliceProperties{}) },
		func(t *testing.T, needReload bool) { assert.False(t, needReload, "needReload") },
	)
}

func TestSchedulerSetExclusiveMode(t *testing.T) {
	testComponentGetterError(t, getScheduler, func(s *Scheduler) (bool, error) { return s.SetExclusiveMode() }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getScheduler,
		func(s *Scheduler) (bool, error) { return s.SetExclusiveMode() },
		func(t *testing.T, needReload bool) { assert.False(t, needReload, "needReload") },
	)
}

// ------------------------------------------------------------------
// Temperature
// ------------------------------------------------------------------

func TestTemperatureGetProperties(t *testing.T) {
	testComponentGetterError(t, getTemperature, (*Temperature).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getTemperature, (*Temperature).GetProperties,
		checkValue(TempProperties{
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
	testComponentGetterError(t, getTemperature, (*Temperature).GetConfig, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getTemperature, (*Temperature).GetConfig,
		checkValue(TempConfig{
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
	testComponentActionError(t, getTemperature, func(temp *Temperature) error { return temp.SetConfig(&TempConfig{}) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getTemperature, func(temp *Temperature) error { return temp.SetConfig(&TempConfig{}) })
}

func TestTemperatureGetState(t *testing.T) {
	testComponentGetterError(t, getTemperature, (*Temperature).GetState, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getTemperature, (*Temperature).GetState,
		checkValue(48.0),
	)
}

// ------------------------------------------------------------------
// FabricPort
// ------------------------------------------------------------------

func TestFabricPortGetProperties(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetProperties,
		checkValue(FabricPortProperties{
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
		}),
	)
}

func TestFabricPortGetLinkType(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetLinkType, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetLinkType,
		checkValue(FabricLinkType{
			Desc: stringProperty[core.StringProperty256]("Xe-Link"),
		}),
	)
}

func TestFabricPortGetConfig(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetConfig, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetConfig,
		checkValue(FabricPortConfig{Enabled: 1, Beaconing: 1}),
	)
}

func TestFabricPortSetConfig(t *testing.T) {
	testComponentActionError(t, getFabricPort, func(fp *FabricPort) error { return fp.SetConfig(&FabricPortConfig{}) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFabricPort, func(fp *FabricPort) error { return fp.SetConfig(&FabricPortConfig{}) })
}

func TestFabricPortGetState(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetState, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetState,
		checkValue(FabricPortState{
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
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetThroughput, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetThroughput,
		checkValue(FabricPortThroughput{Timestamp: 777777, RxCounter: 111111, TxCounter: 222222}),
	)
}

func TestFabricPortGetFabricErrorCounters(t *testing.T) {
	testComponentGetterError(t, getFabricPort, (*FabricPort).GetFabricErrorCounters, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFabricPort, (*FabricPort).GetFabricErrorCounters,
		checkValue(FabricPortErrorCounters{
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
	testComponentGetterError(t, getFan, (*Fan).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFan, (*Fan).GetProperties,
		checkValue(FanProperties{
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
	testComponentGetterError(t, getFan, (*Fan).GetConfig, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFan, (*Fan).GetConfig,
		checkValue(FanConfig{
			Mode: FAN_SPEED_MODE_FIXED,
			SpeedFixed: FanSpeed{
				Speed: 1500,
				Units: FAN_SPEED_UNITS_RPM,
			},
		}),
	)
}

func TestFanSetDefaultMode(t *testing.T) {
	testComponentActionError(t, getFan, (*Fan).SetDefaultMode, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFan, (*Fan).SetDefaultMode)
}

func TestFanSetFixedSpeedMode(t *testing.T) {
	testComponentActionError(t, getFan, func(f *Fan) error { return f.SetFixedSpeedMode(FanSpeed{}) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFan, func(f *Fan) error { return f.SetFixedSpeedMode(FanSpeed{}) })
}

func TestFanSetSpeedTableMode(t *testing.T) {
	testComponentActionError(t, getFan, func(f *Fan) error { return f.SetSpeedTableMode(&FanSpeedTable{}) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFan, func(f *Fan) error { return f.SetSpeedTableMode(&FanSpeedTable{}) })
}

func TestFanGetState(t *testing.T) {
	testComponentGetterError(t, getFan, func(f *Fan) (int32, error) { return f.GetState(0) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFan,
		func(f *Fan) (int32, error) { return f.GetState(0) },
		checkValue(int32(1500)),
	)
}

// ------------------------------------------------------------------
// Firmware
// ------------------------------------------------------------------

func TestFirmwareGetProperties(t *testing.T) {
	testComponentGetterError(t, getFirmware, (*Firmware).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFirmware, (*Firmware).GetProperties,
		checkValue(FirmwareProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			CanControl:  1,
			Name:        stringProperty[core.StringProperty64]("GFX"),
			Version:     stringProperty[core.StringProperty64]("1.2.3.4"),
		}),
	)
}

func TestFirmwareFlash(t *testing.T) {
	testComponentActionError(t, getFirmware, func(fw *Firmware) error { return fw.Flash([]byte{0x00}) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getFirmware, func(fw *Firmware) error { return fw.Flash([]byte{0x00}) })
}

func TestFirmwareGetFlashProgress(t *testing.T) {
	testComponentGetterError(t, getFirmware, (*Firmware).GetFlashProgress, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFirmware, (*Firmware).GetFlashProgress,
		checkValue(uint32(100)),
	)
}

func TestFirmwareGetConsoleLogs(t *testing.T) {
	testComponentGetterError(t, getFirmware, (*Firmware).GetConsoleLogs, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getFirmware, (*Firmware).GetConsoleLogs,
		checkValue("test log\x00"),
	)
}

// ------------------------------------------------------------------
// Led
// ------------------------------------------------------------------

func TestLedGetProperties(t *testing.T) {
	testComponentGetterError(t, getLed, (*Led).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getLed, (*Led).GetProperties,
		checkValue(LedProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			CanControl:  1,
			HaveRGB:     1,
		}),
	)
}

func TestLedGetState(t *testing.T) {
	testComponentGetterError(t, getLed, (*Led).GetState, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getLed, (*Led).GetState,
		checkValue(LedState{
			IsOn: 1,
			Color: LedColor{
				Red:   1.0,
				Green: 0.5,
			},
		}),
	)
}

func TestLedSetState(t *testing.T) {
	testComponentActionError(t, getLed, func(l *Led) error { return l.SetState(false) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getLed, func(l *Led) error { return l.SetState(false) })
}

func TestLedSetColor(t *testing.T) {
	testComponentActionError(t, getLed, func(l *Led) error { return l.SetColor(LedColor{}) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getLed, func(l *Led) error { return l.SetColor(LedColor{}) })
}

// ------------------------------------------------------------------
// Overclock
// ------------------------------------------------------------------

func TestOverclockGetDomainProperties(t *testing.T) {
	testComponentGetterError(t, getOcDomain, (*Overclock).GetDomainProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain, (*Overclock).GetDomainProperties,
		checkValue(OverclockProperties{
			DomainType:        OVERCLOCK_DOMAIN_CARD,
			AvailableControls: 3,
			VFProgramType:     VF_PROGRAM_TYPE_VF_ARBITRARY,
			NumberOfVFPoints:  16,
		}),
	)
}

func TestOverclockGetDomainVFProperties(t *testing.T) {
	testComponentGetterError(t, getOcDomain, (*Overclock).GetDomainVFProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain, (*Overclock).GetDomainVFProperties,
		checkValue(VfProperty{
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
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (ControlProperty, error) { return oc.GetDomainControlProperties(0) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (ControlProperty, error) { return oc.GetDomainControlProperties(0) },
		checkValue(ControlProperty{
			MinValue:     100.0,
			MaxValue:     2000.0,
			StepValue:    50.0,
			RefValue:     900.0,
			DefaultValue: 1100.0,
		}),
	)
}

func TestOverclockGetControlCurrentValue(t *testing.T) {
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (float64, error) { return oc.GetControlCurrentValue(0) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (float64, error) { return oc.GetControlCurrentValue(0) },
		checkValue(1200.0),
	)
}

func TestOverclockGetControlPendingValue(t *testing.T) {
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (float64, error) { return oc.GetControlPendingValue(0) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (float64, error) { return oc.GetControlPendingValue(0) },
		checkValue(1100.0),
	)
}

func TestOverclockSetControlUserValue(t *testing.T) {
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (PendingAction, error) { return oc.SetControlUserValue(0, 0) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (PendingAction, error) { return oc.SetControlUserValue(0, 0) },
		checkValue(PENDING_ACTION_PENDING_NONE),
	)
}

func TestOverclockGetControlState(t *testing.T) {
	t.Run("Error", func(t *testing.T) {
		loadDriverConfig(t, driverConfigComponentErrs)
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
	testComponentGetterError(t, getOcDomain, func(oc *Overclock) (uint32, error) { return oc.GetVFPointValues(0, 0, 0) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getOcDomain,
		func(oc *Overclock) (uint32, error) { return oc.GetVFPointValues(0, 0, 0) },
		checkValue(uint32(42)),
	)
}

func TestOverclockSetVFPointValues(t *testing.T) {
	testComponentActionError(t, getOcDomain, func(oc *Overclock) error { return oc.SetVFPointValues(0, 0, 0) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getOcDomain, func(oc *Overclock) error { return oc.SetVFPointValues(0, 0, 0) })
}

// ------------------------------------------------------------------
// Performance
// ------------------------------------------------------------------

func TestPerformanceGetProperties(t *testing.T) {
	testComponentGetterError(t, getPerf, (*Performance).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPerf, (*Performance).GetProperties,
		checkValue(PerfProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			Engines:     7,
		}),
	)
}

func TestPerformanceGetConfig(t *testing.T) {
	testComponentGetterError(t, getPerf, (*Performance).GetConfig, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPerf, (*Performance).GetConfig,
		checkValue(1.0),
	)
}

func TestPerformanceSetConfig(t *testing.T) {
	testComponentActionError(t, getPerf, func(p *Performance) error { return p.SetConfig(0) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getPerf, func(p *Performance) error { return p.SetConfig(0) })
}

// ------------------------------------------------------------------
// Psu
// ------------------------------------------------------------------

func TestPsuGetProperties(t *testing.T) {
	testComponentGetterError(t, getPsu, (*Psu).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPsu, (*Psu).GetProperties,
		checkValue(PsuProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			HaveFan:     1,
			AmpLimit:    30,
		}),
	)
}

func TestPsuGetState(t *testing.T) {
	testComponentGetterError(t, getPsu, (*Psu).GetState, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getPsu, (*Psu).GetState,
		checkValue(PsuState{
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
	testComponentGetterError(t, getRas, (*Ras).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getRas, (*Ras).GetProperties,
		checkValue(RasProperties{
			Type:        RAS_ERROR_TYPE_CORRECTABLE,
			OnSubdevice: 1,
			SubdeviceId: 1,
		}),
	)
}

func TestRasGetConfig(t *testing.T) {
	testComponentGetterError(t, getRas, (*Ras).GetConfig, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getRas, (*Ras).GetConfig,
		checkValue(RasConfig{
			TotalThreshold: 100,
			DetailedThresholds: RasState{Category: [7]uint64{
				10, 11, 12, 13, 14, 15, 16,
			}},
		}),
	)
}

func TestRasSetConfig(t *testing.T) {
	testComponentActionError(t, getRas, func(r *Ras) error { return r.SetConfig(&RasConfig{}) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getRas, func(r *Ras) error { return r.SetConfig(&RasConfig{}) })
}

func TestRasGetState(t *testing.T) {
	testComponentGetterError(t, getRas, func(r *Ras) (RasState, error) { return r.GetState(false) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getRas,
		func(r *Ras) (RasState, error) { return r.GetState(false) },
		checkValue(RasState{
			Category: [7]uint64{5, 6, 7, 8, 9, 10, 11},
		}),
	)
}

// ------------------------------------------------------------------
// Standby
// ------------------------------------------------------------------

func TestStandbyGetProperties(t *testing.T) {
	testComponentGetterError(t, getStandby, (*Standby).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getStandby, (*Standby).GetProperties,
		checkValue(StandbyProperties{
			Type:        STANDBY_TYPE_GLOBAL,
			OnSubdevice: 0,
			SubdeviceId: 0,
		}),
	)
}

func TestStandbyGetMode(t *testing.T) {
	testComponentGetterError(t, getStandby, (*Standby).GetMode, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getStandby, (*Standby).GetMode,
		checkValue(STANDBY_PROMO_MODE_DEFAULT),
	)
}

func TestStandbySetMode(t *testing.T) {
	testComponentActionError(t, getStandby, func(s *Standby) error { return s.SetMode(0) }, withConfig(driverConfigComponentErrs))
	testComponentActionSuccess(t, getStandby, func(s *Standby) error { return s.SetMode(0) })
}

// ------------------------------------------------------------------
// Diagnostics
// ------------------------------------------------------------------

func TestDiagnosticsGetProperties(t *testing.T) {
	testComponentGetterError(t, getDiagnostic, (*Diagnostics).GetProperties, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getDiagnostic, (*Diagnostics).GetProperties,
		checkValue(DiagProperties{
			OnSubdevice: 0,
			SubdeviceId: 0,
			Name:        stringProperty[core.StringProperty64]("GPU"),
			HaveTests:   0,
		}),
	)
}

func TestDiagnosticsGetTests(t *testing.T) {
	testComponentGetterError(t, getDiagnostic, (*Diagnostics).GetTests, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getDiagnostic, (*Diagnostics).GetTests,
		func(t *testing.T, tests []DiagTest) {
			require.Empty(t, tests)
		},
	)
}

func TestDiagnosticsRunTests(t *testing.T) {
	testComponentGetterError(t, getDiagnostic, func(d *Diagnostics) ([]DiagResult, error) { return d.RunTests(0, 0) }, withConfig(driverConfigComponentErrs))
	testComponentGetterSuccess(t, getDiagnostic,
		func(d *Diagnostics) ([]DiagResult, error) { return d.RunTests(0, 0) },
		func(t *testing.T, results []DiagResult) {
			require.Len(t, results, 1)
			assert.Equal(t, DIAG_RESULT_NO_ERRORS, results[0], "results[0]")
		},
	)
}
