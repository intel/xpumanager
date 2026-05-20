// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"errors"
	"fmt"
	"math/bits"

	"github.com/intel/level-zero-go/core"
	"github.com/intel/level-zero-go/sysman"
)

// recordError records an error into the appropriate list
func (b *BaseInfo) recordError(context string, err error) {
	if err == nil {
		return
	}
	if err == core.RESULT_ERROR_UNSUPPORTED_FEATURE {
		if b.unsupportedFeaturesMap == nil {
			b.unsupportedFeaturesMap = make(map[string]bool)
		}
		if !b.unsupportedFeaturesMap[context] {
			b.unsupportedFeaturesMap[context] = true
			b.UnsupportedFeatures = append(b.UnsupportedFeatures, context)
		}
	} else {
		b.Errors = append(b.Errors, fmt.Sprintf("%s: %v", context, err))
	}
}

func collectSystemInfo() (*SystemInfo, error) {
	drivers, err := sysman.DriverGet()
	if err != nil {
		return nil, err
	}

	sysInfo := &SystemInfo{}

	for _, driver := range drivers {
		if driverInfo := collectDriverInfo(driver); driverInfo != nil {
			sysInfo.Drivers = append(sysInfo.Drivers, *driverInfo)
		}
	}

	return sysInfo, nil
}

func collectDriverInfo(driver *sysman.Driver) *DriverInfo {
	driverInfo := &DriverInfo{}

	extProps, err := driver.GetExtensionProperties()
	if err != nil {
		driverInfo.recordError("GetExtensionProperties", err)
	} else {
		driverInfo.ExtensionProperties = extProps
	}

	devices, err := driver.DeviceGet()
	if err != nil {
		driverInfo.recordError("Driver.DeviceGet", err)
		return driverInfo
	}

	for _, device := range devices {
		deviceInfo := &DeviceInfo{}
		deviceInfo.collect(device)
		driverInfo.Devices = append(driverInfo.Devices, *deviceInfo)
	}

	return driverInfo
}

func (d *DeviceInfo) collect(device *sysman.Device) {
	if props, err := device.GetProperties(); err != nil {
		d.recordError("Device.GetProperties", err)
	} else {
		d.Properties = &props
	}

	if state, err := device.GetState(); err != nil {
		d.recordError("Device.GetState", err)
	} else {
		d.State = &state
	}

	if procs, err := device.ProcessesGetState(); err != nil {
		d.recordError("Device.ProcessesGetState", err)
	} else {
		d.Processes = procs
	}

	d.collectPCIInfo(device)
	d.collectOverclockInfo(device)
	d.collectDiagnosticsInfo(device)
	d.collectECCInfo(device)
	d.collectEngineInfo(device)
	d.collectFabricPortInfo(device)
	d.collectFanInfo(device)
	d.collectFirmwareInfo(device)
	d.collectFrequencyInfo(device)
	d.collectLedInfo(device)
	d.collectMemoryInfo(device)
	d.collectPerformanceInfo(device)
	d.collectPowerInfo(device)
	d.collectPsuInfo(device)
	d.collectRasInfo(device)
	d.collectSchedulerInfo(device)
	d.collectStandbyInfo(device)
	d.collectTemperatureInfo(device)
}

func (d *DeviceInfo) collectPCIInfo(device *sysman.Device) {
	if props, err := device.PciGetProperties(); err != nil {
		d.recordError("PCI.GetProperties", err)
	} else {
		d.PCI.Properties = &props
	}

	if state, err := device.PciGetState(); err != nil {
		d.recordError("PCI.GetState", err)
	} else {
		d.PCI.State = &state
	}

	if bars, err := device.PciGetBars(); err != nil {
		d.recordError("PCI.GetBars", err)
	} else {
		d.PCI.Bars = bars
	}

	if stats, err := device.PciGetStats(); err != nil {
		d.recordError("PCI.GetStats", err)
	} else {
		d.PCI.Stats = &stats
	}
}

func (d *DeviceInfo) collectOverclockInfo(device *sysman.Device) {
	ocInfo := &OverclockInfo{}

	domainTypes, err := device.GetOverclockDomains()
	if err != nil {
		d.recordError("OverclockDomains", err)
		return
	}
	ocInfo.DomainTypes = domainTypes

	for _, flag := range domainTypes.Bits() {
		domainType := sysman.OverclockDomain(flag)
		if controlTypes, err := device.GetOverclockControls(domainType); err != nil {
			d.recordError("Device.GetOverclockControls", err)
		} else {
			ocInfo.Controls = append(ocInfo.Controls, OverclockControlsInfo{
				DomainType:   domainType,
				ControlTypes: controlTypes,
			})
		}
	}

	if state, err := device.ReadOverclockState(); err != nil {
		d.recordError("Overclock.ReadState", err)
	} else {
		ocInfo.State = &state
	}

	domains, err := device.EnumOverclockDomains()
	if err != nil {
		d.recordError("Overclock.EnumDomains", err)
	} else {
		ocInfo.Domains = make([]OverclockDomainInfo, len(domains))
		for i, domain := range domains {
			var domainProps *sysman.OverclockProperties
			if props, err := domain.GetDomainProperties(); err != nil {
				d.recordError("OverclockDomain.GetProperties", err)
			} else {
				ocInfo.Domains[i].Properties = &props
				domainProps = &props
			}
			if vfProps, err := domain.GetDomainVFProperties(); err != nil {
				d.recordError("OverclockDomain.GetVFProperties", err)
			} else {
				ocInfo.Domains[i].VFProperties = &vfProps
			}
			if domainProps != nil {
				for _, flag := range domainProps.AvailableControls.Bits() {
					ctrl := sysman.OverclockControl(flag)
					info := OverclockDomainControlsInfo{ControlType: ctrl}
					if cp, err := domain.GetDomainControlProperties(ctrl); err != nil {
						d.recordError("OverclockDomain.GetDomainControlProperties", err)
					} else {
						info.Properties = &cp
					}
					if val, err := domain.GetControlCurrentValue(ctrl); err != nil {
						d.recordError("OverclockDomain.GetControlCurrentValue", err)
					} else {
						info.CurrentValue = &val
					}
					if val, err := domain.GetControlPendingValue(ctrl); err != nil {
						d.recordError("OverclockDomain.GetControlPendingValue", err)
					} else {
						info.PendingValue = &val
					}
					if state, pendingAction, err := domain.GetControlState(ctrl); err != nil {
						d.recordError("OverclockDomain.GetControlState", err)
					} else {
						info.State = &state
						info.PendingAction = &pendingAction
					}
					ocInfo.Domains[i].ControlInfos = append(ocInfo.Domains[i].ControlInfos, info)
				}
			}
		}
	}

	d.Overclock = ocInfo
}

func (d *DeviceInfo) collectDiagnosticsInfo(device *sysman.Device) {
	diags, err := device.EnumDiagnosticTestSuites()
	if err != nil {
		d.recordError("DiagnosticsTestSuites", err)
		return
	}

	result := make([]DiagnosticsInfo, len(diags))
	for i, diag := range diags {
		if props, err := diag.GetProperties(); err != nil {
			d.recordError("Diagnostic.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if tests, err := diag.GetTests(); err != nil {
			d.recordError("Diagnostic.GetTests", err)
		} else {
			result[i].Tests = tests
		}
	}

	d.Diagnostics = result
}

func (d *DeviceInfo) collectECCInfo(device *sysman.Device) {
	eccInfo := &ECCInfo{}

	available, err := device.EccAvailable()
	if err != nil {
		d.recordError("ECC", err)
		return
	}
	eccInfo.Available = available

	if available {
		if configurable, err := device.EccConfigurable(); err != nil {
			d.recordError("ECC.Configurable", err)
		} else {
			eccInfo.Configurable = configurable
		}

		if state, err := device.GetEccState(); err != nil {
			d.recordError("ECC.GetState", err)
		} else {
			eccInfo.State = &state
		}
	}

	d.ECC = eccInfo
}

func (d *DeviceInfo) collectEngineInfo(device *sysman.Device) {
	engines, err := device.EnumEngineGroups()
	if err != nil {
		d.recordError("EngineGroups", err)
		return
	}

	result := make([]EngineGroupInfo, len(engines))
	for i, engine := range engines {
		if props, err := engine.GetProperties(); err != nil {
			d.recordError("EngineGroup.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if activity, err := engine.GetActivity(); err != nil {
			d.recordError("EngineGroup.GetActivity", err)
		} else {
			result[i].Activity = &activity
		}
		if activityExt, err := engine.GetActivityExt(); err != nil {
			d.recordError("EngineGroup.GetActivityExt", err)
		} else {
			result[i].ActivityExt = activityExt
		}
	}

	d.EngineGroups = result
}

func (d *DeviceInfo) collectFabricPortInfo(device *sysman.Device) {
	ports, err := device.EnumFabricPorts()
	if err != nil {
		d.recordError("FabricPorts", err)
		return
	}

	result := make([]FabricPortInfo, len(ports))
	for i, port := range ports {
		if props, err := port.GetProperties(); err != nil {
			d.recordError("FabricPort.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if linkType, err := port.GetLinkType(); err != nil {
			d.recordError("FabricPort.GetLinkType", err)
		} else {
			result[i].LinkType = &linkType
		}
		if config, err := port.GetConfig(); err != nil {
			d.recordError("FabricPort.GetConfig", err)
		} else {
			result[i].Config = &config
		}
		if state, err := port.GetState(); err != nil {
			d.recordError("FabricPort.GetState", err)
		} else {
			result[i].State = &state
		}
		if throughput, err := port.GetThroughput(); err != nil {
			d.recordError("FabricPort.GetThroughput", err)
		} else {
			result[i].Throughput = &throughput
		}
		if errCounters, err := port.GetFabricErrorCounters(); err != nil {
			d.recordError("FabricPort.GetErrorCounters", err)
		} else {
			result[i].ErrorCounters = &errCounters
		}
	}

	d.FabricPorts = result
}

func (d *DeviceInfo) collectFanInfo(device *sysman.Device) {
	fans, err := device.EnumFans()
	if err != nil {
		d.recordError("Fans", err)
		return
	}

	result := make([]FanInfo, len(fans))
	for i, fan := range fans {
		if props, err := fan.GetProperties(); err != nil {
			d.recordError("Fan.GetProperties", err)
		} else {
			result[i].Properties = &props

			// Return speed in the first supported unit
			if unit := bits.TrailingZeros32(props.SupportedUnits); unit < 32 {
				if speed, err := fan.GetState(sysman.FanSpeedUnits(unit)); err != nil {
					d.recordError("Fan.GetState", err)
				} else {
					result[i].Speed = speed
				}
			} else {
				d.recordError("Fan.GetState", errors.New("no supported speed units"))
			}
		}
		if config, err := fan.GetConfig(); err != nil {
			d.recordError("Fan.GetConfig", err)
		} else {
			result[i].Config = &config
		}
	}

	d.Fans = result
}

func (d *DeviceInfo) collectFirmwareInfo(device *sysman.Device) {
	firmwares, err := device.EnumFirmwares()
	if err != nil {
		d.recordError("Firmwares", err)
		return
	}

	result := make([]FirmwareInfo, len(firmwares))
	for i, firmware := range firmwares {
		if props, err := firmware.GetProperties(); err != nil {
			d.recordError("Firmware.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
	}

	d.Firmwares = result
}

func (d *DeviceInfo) collectFrequencyInfo(device *sysman.Device) {
	domains, err := device.EnumFrequencyDomains()
	if err != nil {
		d.recordError("FrequencyDomains", err)
		return
	}

	result := make([]FrequencyDomainInfo, len(domains))
	for i, domain := range domains {
		if props, err := domain.GetProperties(); err != nil {
			d.recordError("FrequencyDomain.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if clocks, err := domain.GetAvailableClocks(); err != nil {
			d.recordError("FrequencyDomain.GetAvailableClocks", err)
		} else {
			result[i].AvailableClocks = clocks
		}
		if freqRange, err := domain.GetRange(); err != nil {
			d.recordError("FrequencyDomain.GetRange", err)
		} else {
			result[i].Range = &freqRange
		}
		if state, err := domain.GetState(); err != nil {
			d.recordError("FrequencyDomain.GetState", err)
		} else {
			result[i].State = &state
		}
		if throttleTime, err := domain.GetThrottleTime(); err != nil {
			d.recordError("FrequencyDomain.GetThrottleTime", err)
		} else {
			result[i].ThrottleTime = &throttleTime
		}
	}

	d.FrequencyDomains = result
}

func (d *DeviceInfo) collectLedInfo(device *sysman.Device) {
	leds, err := device.EnumLeds()
	if err != nil {
		d.recordError("Leds", err)
		return
	}

	result := make([]LedInfo, len(leds))
	for i, led := range leds {
		if props, err := led.GetProperties(); err != nil {
			d.recordError("Led.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if state, err := led.GetState(); err != nil {
			d.recordError("Led.GetState", err)
		} else {
			result[i].State = &state
		}
	}

	d.Leds = result
}

func (d *DeviceInfo) collectMemoryInfo(device *sysman.Device) {
	modules, err := device.EnumMemoryModules()
	if err != nil {
		d.recordError("MemoryModules", err)
		return
	}

	result := make([]MemoryModuleInfo, len(modules))
	for i, module := range modules {
		if props, err := module.GetProperties(); err != nil {
			d.recordError("MemoryModule.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if state, err := module.GetState(); err != nil {
			d.recordError("MemoryModule.GetState", err)
		} else {
			result[i].State = &state
		}
		if bandwidth, err := module.GetBandwidth(); err != nil {
			d.recordError("MemoryModule.GetBandwidth", err)
		} else {
			result[i].Bandwidth = &bandwidth
		}
	}

	d.MemoryModules = result
}

func (d *DeviceInfo) collectPerformanceInfo(device *sysman.Device) {
	domains, err := device.EnumPerformanceFactorDomains()
	if err != nil {
		d.recordError("PerformanceDomains", err)
		return
	}

	result := make([]PerformanceDomainInfo, len(domains))
	for i, domain := range domains {
		if props, err := domain.GetProperties(); err != nil {
			d.recordError("PerformanceDomain.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if config, err := domain.GetConfig(); err != nil {
			d.recordError("PerformanceDomain.GetConfig", err)
		} else {
			result[i].Config = config
		}
	}

	d.PerformanceDomains = result
}

func (d *DeviceInfo) collectPowerInfo(device *sysman.Device) {
	domains, err := device.EnumPowerDomains()
	if err != nil {
		d.recordError("PowerDomains", err)
		return
	}

	result := make([]PowerDomainInfo, len(domains))
	for i, domain := range domains {
		if props, err := domain.GetProperties(); err != nil {
			d.recordError("PowerDomain.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if energyCounter, err := domain.GetEnergyCounter(); err != nil {
			d.recordError("PowerDomain.GetEnergyCounter", err)
		} else {
			result[i].EnergyCounter = &energyCounter
		}
		if energyThreshold, err := domain.GetEnergyThreshold(); err != nil {
			d.recordError("PowerDomain.GetEnergyThreshold", err)
		} else {
			result[i].EnergyThreshold = &energyThreshold
		}
		if limits, err := domain.GetLimitsExt(); err != nil {
			d.recordError("PowerDomain.GetLimitsExt", err)
		} else {
			result[i].Limits = limits
		}
	}

	d.PowerDomains = result
}

func (d *DeviceInfo) collectPsuInfo(device *sysman.Device) {
	psus, err := device.EnumPsus()
	if err != nil {
		d.recordError("Psus", err)
		return
	}

	result := make([]PsuInfo, len(psus))
	for i, psu := range psus {
		if props, err := psu.GetProperties(); err != nil {
			d.recordError("Psu.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if state, err := psu.GetState(); err != nil {
			d.recordError("Psu.GetState", err)
		} else {
			result[i].State = &state
		}
	}

	d.Psus = result
}

func (d *DeviceInfo) collectRasInfo(device *sysman.Device) {
	rasSets, err := device.EnumRasErrorSets()
	if err != nil {
		d.recordError("RasErrorSets", err)
		return
	}

	result := make([]RasInfo, len(rasSets))
	for i, ras := range rasSets {
		if props, err := ras.GetProperties(); err != nil {
			d.recordError("RasErrorSet.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if config, err := ras.GetConfig(); err != nil {
			d.recordError("RasErrorSet.GetConfig", err)
		} else {
			result[i].Config = &config
		}
		if state, err := ras.GetState(false); err != nil {
			d.recordError("RasErrorSet.GetState", err)
		} else {
			result[i].State = &state
		}
		if states, err := ras.GetStateExp(); err != nil {
			d.recordError("RasErrorSet.GetStateExp", err)
		} else {
			result[i].StateExp = states
		}
	}

	d.RasErrorSets = result
}

func (d *DeviceInfo) collectSchedulerInfo(device *sysman.Device) {
	scheds, err := device.EnumSchedulers()
	if err != nil {
		d.recordError("Schedulers", err)
		return
	}

	result := make([]SchedulerInfo, len(scheds))
	for i, sched := range scheds {
		if props, err := sched.GetProperties(); err != nil {
			d.recordError("Scheduler.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if mode, err := sched.GetCurrentMode(); err != nil {
			d.recordError("Scheduler.GetCurrentMode", err)
		} else {
			result[i].CurrentMode = mode
		}
		if timeoutProps, err := sched.GetTimeoutModeProperties(false); err != nil {
			d.recordError("Scheduler.GetTimeoutModeProperties", err)
		} else {
			result[i].TimeoutModeProperties = &timeoutProps
		}
		if timesliceProps, err := sched.GetTimesliceModeProperties(false); err != nil {
			d.recordError("Scheduler.GetTimesliceModeProperties", err)
		} else {
			result[i].TimesliceModeProperties = &timesliceProps
		}
	}

	d.Schedulers = result
}

func (d *DeviceInfo) collectStandbyInfo(device *sysman.Device) {
	domains, err := device.EnumStandbyDomains()
	if err != nil {
		d.recordError("StandbyDomains", err)
		return
	}

	result := make([]StandbyDomainInfo, len(domains))
	for i, domain := range domains {
		if props, err := domain.GetProperties(); err != nil {
			d.recordError("StandbyDomain.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if mode, err := domain.GetMode(); err != nil {
			d.recordError("StandbyDomain.GetMode", err)
		} else {
			result[i].Mode = sysman.StandbyType(mode)
		}
	}

	d.StandbyDomains = result
}

func (d *DeviceInfo) collectTemperatureInfo(device *sysman.Device) {
	sensors, err := device.EnumTemperatureSensors()
	if err != nil {
		d.recordError("TemperatureSensors", err)
		return
	}

	result := make([]TemperatureSensorInfo, len(sensors))
	for i, sensor := range sensors {
		if props, err := sensor.GetProperties(); err != nil {
			d.recordError("TemperatureSensor.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if config, err := sensor.GetConfig(); err != nil {
			d.recordError("TemperatureSensor.GetConfig", err)
		} else {
			result[i].Config = &config
		}
		if temp, err := sensor.GetState(); err != nil {
			d.recordError("TemperatureSensor.GetState", err)
		} else {
			result[i].Temperature = temp
		}
	}

	d.TemperatureSensors = result
}
