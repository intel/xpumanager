// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"errors"
	"log"
	"math/bits"

	"github.com/intel/level-zero-go/core"
	"github.com/intel/level-zero-go/sysman"
)

// logError logs an error if it's not an UNSUPPORTED_FEATURE error
func logError(context string, err error) {
	if err != nil && !errors.Is(err, core.RESULT_ERROR_UNSUPPORTED_FEATURE) {
		log.Printf("ERROR: %s: %v", context, err)
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
		logError("Driver.GetExtensionProperties", err)
	} else {
		driverInfo.ExtensionProperties = extProps
	}

	devices, err := driver.DeviceGet()
	if err != nil {
		logError("Driver.DeviceGet", err)
	} else {
		for _, device := range devices {
			driverInfo.Devices = append(driverInfo.Devices, collectDeviceInfo(device))
		}
	}

	return driverInfo
}

func collectDeviceInfo(device *sysman.Device) *DeviceInfo {
	deviceInfo := &DeviceInfo{}

	if props, err := device.GetProperties(); err != nil {
		logError("Device.GetProperties", err)
	} else {
		deviceInfo.Properties = &props
	}

	if state, err := device.GetState(); err != nil {
		logError("Device.GetState", err)
	} else {
		deviceInfo.State = &state
	}

	if procs, err := device.ProcessesGetState(); err != nil {
		logError("Device.ProcessesGetState", err)
	} else {
		deviceInfo.Processes = procs
	}

	deviceInfo.PCI = collectPCIInfo(device)
	deviceInfo.Overclock = collectOverclockInfo(device)
	deviceInfo.Diagnostics = collectDiagnosticsInfo(device)
	deviceInfo.ECC = collectECCInfo(device)
	deviceInfo.EngineGroups = collectEngineInfo(device)
	deviceInfo.FabricPorts = collectFabricPortInfo(device)
	deviceInfo.Fans = collectFanInfo(device)
	deviceInfo.Firmwares = collectFirmwareInfo(device)
	deviceInfo.FrequencyDomains = collectFrequencyInfo(device)
	deviceInfo.Leds = collectLedInfo(device)
	deviceInfo.MemoryModules = collectMemoryInfo(device)
	deviceInfo.PerformanceDomains = collectPerformanceInfo(device)
	deviceInfo.PowerDomains = collectPowerInfo(device)
	deviceInfo.Psus = collectPsuInfo(device)
	deviceInfo.RasErrorSets = collectRasInfo(device)
	deviceInfo.Schedulers = collectSchedulerInfo(device)
	deviceInfo.StandbyDomains = collectStandbyInfo(device)
	deviceInfo.TemperatureSensors = collectTemperatureInfo(device)

	return deviceInfo
}

func collectPCIInfo(device *sysman.Device) *PCIInfo {
	pciInfo := &PCIInfo{}

	if props, err := device.PciGetProperties(); err != nil {
		logError("PCI.GetProperties", err)
	} else {
		pciInfo.Properties = &props
	}

	if state, err := device.PciGetState(); err != nil {
		logError("PCI.GetState", err)
	} else {
		pciInfo.State = state
	}

	if bars, err := device.PciGetBars(); err != nil {
		logError("PCI.GetBars", err)
	} else {
		pciInfo.Bars = bars
	}

	if stats, err := device.PciGetStats(); err != nil {
		logError("PCI.GetStats", err)
	} else {
		pciInfo.Stats = stats
	}

	return pciInfo
}

func collectOverclockInfo(device *sysman.Device) *OverclockInfo {
	ocInfo := &OverclockInfo{}

	domainFlags, err := device.GetOverclockDomains()
	if err != nil {
		logError("Overclock.GetDomains", err)
		return nil
	}
	ocInfo.DomainsBitmask = domainFlags

	if state, err := device.ReadOverclockState(); err != nil {
		logError("Overclock.ReadState", err)
	} else {
		ocInfo.State = &state
	}

	domains, err := device.EnumOverclockDomains()
	if err != nil {
		logError("Overclock.EnumDomains", err)
	} else if len(domains) > 0 {
		ocInfo.Domains = make([]OverclockDomainInfo, len(domains))
		for i, domain := range domains {
			if props, err := domain.GetDomainProperties(); err != nil {
				logError("OverclockDomain.GetProperties", err)
			} else {
				ocInfo.Domains[i].Properties = &props
			}
			if vfProps, err := domain.GetDomainVFProperties(); err != nil {
				logError("OverclockDomain.GetVFProperties", err)
			} else {
				ocInfo.Domains[i].VFProperties = &vfProps
			}
		}
	}

	return ocInfo
}

func collectDiagnosticsInfo(device *sysman.Device) []DiagnosticsInfo {
	diags, err := device.EnumDiagnosticTestSuites()
	if err != nil {
		logError("Diagnostics.EnumTestSuites", err)
		return nil
	}

	result := make([]DiagnosticsInfo, len(diags))
	for i, diag := range diags {
		if props, err := diag.GetProperties(); err != nil {
			logError("Diagnostic.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if tests, err := diag.GetTests(); err != nil {
			logError("Diagnostic.GetTests", err)
		} else {
			result[i].Tests = tests
		}
	}

	return result
}

func collectECCInfo(device *sysman.Device) *ECCInfo {
	eccInfo := &ECCInfo{}

	available, err := device.EccAvailable()
	if err != nil {
		logError("ECC.Available", err)
		return nil
	}
	eccInfo.Available = available

	if available {
		if configurable, err := device.EccConfigurable(); err != nil {
			logError("ECC.Configurable", err)
		} else {
			eccInfo.Configurable = configurable
		}

		if state, err := device.GetEccState(); err != nil {
			logError("ECC.GetState", err)
		} else {
			eccInfo.State = &state
		}
	}

	return eccInfo
}

func collectEngineInfo(device *sysman.Device) []EngineGroupInfo {
	engines, err := device.EnumEngineGroups()
	if err != nil {
		logError("Engine.EnumGroups", err)
		return nil
	}

	result := make([]EngineGroupInfo, len(engines))
	for i, engine := range engines {
		if props, err := engine.GetProperties(); err != nil {
			logError("EngineGroup.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if activity, err := engine.GetActivity(); err != nil {
			logError("EngineGroup.GetActivity", err)
		} else {
			result[i].Activity = &activity
		}
		if activityExt, err := engine.GetActivityExt(); err != nil {
			logError("EngineGroup.GetActivityExt", err)
		} else {
			result[i].ActivityExt = activityExt
		}
	}

	return result
}

func collectFabricPortInfo(device *sysman.Device) []FabricPortInfo {
	ports, err := device.EnumFabricPorts()
	if err != nil {
		logError("Fabric.EnumPorts", err)
		return nil
	}

	result := make([]FabricPortInfo, len(ports))
	for i, port := range ports {
		if props, err := port.GetProperties(); err != nil {
			logError("FabricPort.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if linkType, err := port.GetLinkType(); err != nil {
			logError("FabricPort.GetLinkType", err)
		} else {
			result[i].LinkType = &linkType
		}
		if config, err := port.GetConfig(); err != nil {
			logError("FabricPort.GetConfig", err)
		} else {
			result[i].Config = &config
		}
		if state, err := port.GetState(); err != nil {
			logError("FabricPort.GetState", err)
		} else {
			result[i].State = &state
		}
		if throughput, err := port.GetThroughput(); err != nil {
			logError("FabricPort.GetThroughput", err)
		} else {
			result[i].Throughput = &throughput
		}
		if errCounters, err := port.GetFabricErrorCounters(); err != nil {
			logError("FabricPort.GetErrorCounters", err)
		} else {
			result[i].ErrorCounters = &errCounters
		}
	}

	return result
}

func collectFanInfo(device *sysman.Device) []FanInfo {
	fans, err := device.EnumFans()
	if err != nil {
		logError("Fan.EnumFans", err)
		return nil
	}

	result := make([]FanInfo, len(fans))
	for i, fan := range fans {
		if props, err := fan.GetProperties(); err != nil {
			logError("Fan.GetProperties", err)
		} else {
			result[i].Properties = &props

			// Return speed in the first supported unit
			if unit := bits.TrailingZeros32(props.SupportedUnits); unit < 32 {
				if speed, err := fan.GetState(sysman.FanSpeedUnits(unit)); err != nil {
					logError("Fan.GetState", err)
				} else {
					result[i].Speed = speed
				}
			} else {
				logError("Fan.GetState", errors.New("no supported speed units"))
			}
		}
		if config, err := fan.GetConfig(); err != nil {
			logError("Fan.GetConfig", err)
		} else {
			result[i].Config = &config
		}
	}

	return result
}

func collectFirmwareInfo(device *sysman.Device) []FirmwareInfo {
	firmwares, err := device.EnumFirmwares()
	if err != nil {
		logError("Firmware.EnumFirmwares", err)
		return nil
	}

	result := make([]FirmwareInfo, len(firmwares))
	for i, firmware := range firmwares {
		if props, err := firmware.GetProperties(); err != nil {
			logError("Firmware.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
	}

	return result
}

func collectFrequencyInfo(device *sysman.Device) []FrequencyDomainInfo {
	domains, err := device.EnumFrequencyDomains()
	if err != nil {
		logError("Frequency.EnumDomains", err)
		return nil
	}

	result := make([]FrequencyDomainInfo, len(domains))
	for i, domain := range domains {
		if props, err := domain.GetProperties(); err != nil {
			logError("FrequencyDomain.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if clocks, err := domain.GetAvailableClocks(); err != nil {
			logError("FrequencyDomain.GetAvailableClocks", err)
		} else {
			result[i].AvailableClocks = clocks
		}
		if freqRange, err := domain.GetRange(); err != nil {
			logError("FrequencyDomain.GetRange", err)
		} else {
			result[i].Range = &freqRange
		}
		if state, err := domain.GetState(); err != nil {
			logError("FrequencyDomain.GetState", err)
		} else {
			result[i].State = &state
		}
		if throttleTime, err := domain.GetThrottleTime(); err != nil {
			logError("FrequencyDomain.GetThrottleTime", err)
		} else {
			result[i].ThrottleTime = &throttleTime
		}
	}

	return result
}

func collectLedInfo(device *sysman.Device) []LedInfo {
	leds, err := device.EnumLeds()
	if err != nil {
		logError("Led.EnumLeds", err)
		return nil
	}

	result := make([]LedInfo, len(leds))
	for i, led := range leds {
		if props, err := led.GetProperties(); err != nil {
			logError("Led.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if state, err := led.GetState(); err != nil {
			logError("Led.GetState", err)
		} else {
			result[i].State = &state
		}
	}

	return result
}

func collectMemoryInfo(device *sysman.Device) []MemoryModuleInfo {
	modules, err := device.EnumMemoryModules()
	if err != nil {
		logError("Memory.EnumModules", err)
		return nil
	}

	result := make([]MemoryModuleInfo, len(modules))
	for i, module := range modules {
		if props, err := module.GetProperties(); err != nil {
			logError("MemoryModule.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if state, err := module.GetState(); err != nil {
			logError("MemoryModule.GetState", err)
		} else {
			result[i].State = &state
		}
		if bandwidth, err := module.GetBandwidth(); err != nil {
			logError("MemoryModule.GetBandwidth", err)
		} else {
			result[i].Bandwidth = &bandwidth
		}
	}

	return result
}

func collectPerformanceInfo(device *sysman.Device) []PerformanceDomainInfo {
	domains, err := device.EnumPerformanceFactorDomains()
	if err != nil {
		logError("Performance.EnumDomains", err)
		return nil
	}

	result := make([]PerformanceDomainInfo, len(domains))
	for i, domain := range domains {
		if props, err := domain.GetProperties(); err != nil {
			logError("PerformanceDomain.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if config, err := domain.GetConfig(); err != nil {
			logError("PerformanceDomain.GetConfig", err)
		} else {
			result[i].Config = config
		}
	}

	return result
}

func collectPowerInfo(device *sysman.Device) []PowerDomainInfo {
	domains, err := device.EnumPowerDomains()
	if err != nil {
		logError("Power.EnumDomains", err)
		return nil
	}

	result := make([]PowerDomainInfo, len(domains))
	for i, domain := range domains {
		if props, err := domain.GetProperties(); err != nil {
			logError("PowerDomain.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if energyCounter, err := domain.GetEnergyCounter(); err != nil {
			logError("PowerDomain.GetEnergyCounter", err)
		} else {
			result[i].EnergyCounter = &energyCounter
		}
		if energyThreshold, err := domain.GetEnergyThreshold(); err != nil {
			logError("PowerDomain.GetEnergyThreshold", err)
		} else {
			result[i].EnergyThreshold = &energyThreshold
		}
		if limits, err := domain.GetLimitsExt(); err != nil {
			logError("PowerDomain.GetLimitsExt", err)
		} else {
			result[i].Limits = limits
		}
	}

	return result
}

func collectPsuInfo(device *sysman.Device) []PsuInfo {
	psus, err := device.EnumPsus()
	if err != nil {
		logError("Psu.EnumPsus", err)
		return nil
	}

	result := make([]PsuInfo, len(psus))
	for i, psu := range psus {
		if props, err := psu.GetProperties(); err != nil {
			logError("Psu.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if state, err := psu.GetState(); err != nil {
			logError("Psu.GetState", err)
		} else {
			result[i].State = &state
		}
	}

	return result
}

func collectRasInfo(device *sysman.Device) []RasInfo {
	rasSets, err := device.EnumRasErrorSets()
	if err != nil {
		logError("Ras.EnumErrorSets", err)
		return nil
	}

	result := make([]RasInfo, len(rasSets))
	for i, ras := range rasSets {
		if props, err := ras.GetProperties(); err != nil {
			logError("RasErrorSet.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if config, err := ras.GetConfig(); err != nil {
			logError("RasErrorSet.GetConfig", err)
		} else {
			result[i].Config = &config
		}
		if state, err := ras.GetState(false); err != nil {
			logError("RasErrorSet.GetState", err)
		} else {
			result[i].State = &state
		}
	}

	return result
}

func collectSchedulerInfo(device *sysman.Device) []SchedulerInfo {
	scheds, err := device.EnumSchedulers()
	if err != nil {
		logError("Scheduler.EnumSchedulers", err)
		return nil
	}

	result := make([]SchedulerInfo, len(scheds))
	for i, sched := range scheds {
		if props, err := sched.GetProperties(); err != nil {
			logError("Scheduler.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if mode, err := sched.GetCurrentMode(); err != nil {
			logError("Scheduler.GetCurrentMode", err)
		} else {
			result[i].CurrentMode = mode
		}
		if timeoutProps, err := sched.GetTimeoutModeProperties(false); err != nil {
			logError("Scheduler.GetTimeoutModeProperties", err)
		} else {
			result[i].TimeoutModeProperties = &timeoutProps
		}
		if timesliceProps, err := sched.GetTimesliceModeProperties(false); err != nil {
			logError("Scheduler.GetTimesliceModeProperties", err)
		} else {
			result[i].TimesliceModeProperties = &timesliceProps
		}
	}

	return result
}

func collectStandbyInfo(device *sysman.Device) []StandbyDomainInfo {
	domains, err := device.EnumStandbyDomains()
	if err != nil {
		logError("Standby.EnumDomains", err)
		return nil
	}

	result := make([]StandbyDomainInfo, len(domains))
	for i, domain := range domains {
		if props, err := domain.GetProperties(); err != nil {
			logError("StandbyDomain.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if mode, err := domain.GetMode(); err != nil {
			logError("StandbyDomain.GetMode", err)
		} else {
			result[i].Mode = sysman.StandbyType(mode)
		}
	}

	return result
}

func collectTemperatureInfo(device *sysman.Device) []TemperatureSensorInfo {
	sensors, err := device.EnumTemperatureSensors()
	if err != nil {
		logError("Temperature.EnumSensors", err)
		return nil
	}

	result := make([]TemperatureSensorInfo, len(sensors))
	for i, sensor := range sensors {
		if props, err := sensor.GetProperties(); err != nil {
			logError("TemperatureSensor.GetProperties", err)
		} else {
			result[i].Properties = &props
		}
		if config, err := sensor.GetConfig(); err != nil {
			logError("TemperatureSensor.GetConfig", err)
		} else {
			result[i].Config = &config
		}
		if temp, err := sensor.GetState(); err != nil {
			logError("TemperatureSensor.GetState", err)
		} else {
			result[i].Temperature = temp
		}
	}

	return result
}
