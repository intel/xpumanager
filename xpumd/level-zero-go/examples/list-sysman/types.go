// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"github.com/intel/level-zero-go/sysman"
)

type BaseInfo struct {
	Errors                 []string
	UnsupportedFeatures    []string
	unsupportedFeaturesMap map[string]bool // private map for tracking duplicates
}

type SystemInfo struct {
	Drivers []DriverInfo
}

type DriverInfo struct {
	BaseInfo
	ExtensionProperties []sysman.DriverExtensionProperties
	Devices             []DeviceInfo
}

type DeviceInfo struct {
	BaseInfo
	Properties         *sysman.DeviceProperties
	State              *sysman.DeviceState
	Processes          []sysman.ProcessState
	PCI                PCIInfo
	Overclock          *OverclockInfo
	Diagnostics        []DiagnosticsInfo
	ECC                *ECCInfo
	EngineGroups       []EngineGroupInfo
	FabricPorts        []FabricPortInfo
	Fans               []FanInfo
	Firmwares          []FirmwareInfo
	FrequencyDomains   []FrequencyDomainInfo
	Leds               []LedInfo
	MemoryModules      []MemoryModuleInfo
	PerformanceDomains []PerformanceDomainInfo
	PowerDomains       []PowerDomainInfo
	Psus               []PsuInfo
	RasErrorSets       []RasInfo
	Schedulers         []SchedulerInfo
	StandbyDomains     []StandbyDomainInfo
	TemperatureSensors []TemperatureSensorInfo
}

type PCIInfo struct {
	Properties *sysman.PciProperties
	State      *sysman.PciState
	Bars       []sysman.PciBarProperties
	Stats      *sysman.PciStats
}

type ECCInfo struct {
	Available    bool
	Configurable bool
	State        *sysman.EccProperties
}

type OverclockInfo struct {
	DomainTypes sysman.OverclockDomains
	State       *sysman.OverclockState
	Controls    []OverclockControlsInfo
	Domains     []OverclockDomainInfo
}

type OverclockControlsInfo struct {
	DomainType   sysman.OverclockDomain
	ControlTypes sysman.OverclockControls
}

type OverclockDomainControlsInfo struct {
	ControlType   sysman.OverclockControl
	Properties    *sysman.ControlProperty
	CurrentValue  *float64
	PendingValue  *float64
	State         *sysman.ControlState
	PendingAction *sysman.PendingAction
}

type OverclockDomainInfo struct {
	Properties   *sysman.OverclockProperties
	VFProperties *sysman.VfProperty
	ControlInfos []OverclockDomainControlsInfo
}

type DiagnosticsInfo struct {
	Properties *sysman.DiagProperties
	Tests      []sysman.DiagTest
}

type EngineGroupInfo struct {
	Properties  *sysman.EngineProperties
	Activity    *sysman.EngineStats
	ActivityExt []sysman.EngineStats
}

type FabricPortInfo struct {
	Properties    *sysman.FabricPortProperties
	LinkType      *sysman.FabricLinkType
	Config        *sysman.FabricPortConfig
	State         *sysman.FabricPortState
	Throughput    *sysman.FabricPortThroughput
	ErrorCounters *sysman.FabricPortErrorCounters
}

type FanInfo struct {
	Properties *sysman.FanProperties
	Config     *sysman.FanConfig
	Speed      int32
}

type FirmwareInfo struct {
	Properties *sysman.FirmwareProperties
}

type FrequencyDomainInfo struct {
	Properties      *sysman.FreqProperties
	AvailableClocks []float64
	Range           *sysman.FreqRange
	State           *sysman.FreqState
	ThrottleTime    *sysman.FreqThrottleTime
}

type LedInfo struct {
	Properties *sysman.LedProperties
	State      *sysman.LedState
}

type MemoryModuleInfo struct {
	Properties *sysman.MemProperties
	State      *sysman.MemState
	Bandwidth  *sysman.MemBandwidth
}

type PerformanceDomainInfo struct {
	Properties *sysman.PerfProperties
	Config     float64
}

type PowerDomainInfo struct {
	Properties      *sysman.PowerProperties
	EnergyCounter   *sysman.PowerEnergyCounter
	EnergyThreshold *sysman.EnergyThreshold
	Limits          []sysman.PowerLimitExtDesc
}

type PsuInfo struct {
	Properties *sysman.PsuProperties
	State      *sysman.PsuState
}

type RasInfo struct {
	Properties *sysman.RasProperties
	Config     *sysman.RasConfig
	State      *sysman.RasState
}

type SchedulerInfo struct {
	Properties              *sysman.SchedProperties
	CurrentMode             sysman.SchedMode
	TimeoutModeProperties   *sysman.SchedTimeoutProperties
	TimesliceModeProperties *sysman.SchedTimesliceProperties
}

type StandbyDomainInfo struct {
	Properties *sysman.StandbyProperties
	Mode       sysman.StandbyType
}

type TemperatureSensorInfo struct {
	Properties  *sysman.TempProperties
	Config      *sysman.TempConfig
	Temperature float64
}
