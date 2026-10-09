// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"github.com/intel/level-zero-go/sysman"
)

type BaseInfo struct {
	Errors                 []string        `yaml:"Errors"`
	UnsupportedFeatures    []string        `yaml:"UnsupportedFeatures"`
	unsupportedFeaturesMap map[string]bool // private map for tracking duplicates
}

type SystemInfo struct {
	Drivers []DriverInfo `yaml:"Drivers"`
}

// BaseDriverInfo holds the driver fields that are always available,
// regardless of whether the experimental (Exp) Sysman API extensions are
// used. See types_stable.go/types_exp.go for the DriverInfo variants that
// embed this.
type BaseDriverInfo struct {
	BaseInfo            `yaml:",inline"`
	ExtensionProperties []sysman.DriverExtensionProperties `yaml:"ExtensionProperties"`
	Devices             []DeviceInfo                       `yaml:"Devices"`
}

type DeviceInfo struct {
	BaseInfo           `yaml:",inline"`
	Properties         *sysman.DeviceProperties      `yaml:"Properties"`
	State              *sysman.DeviceState           `yaml:"State"`
	HealthStatus       *sysman.DeviceHealthStatusExt `yaml:"HealthStatus"`
	Processes          []sysman.ProcessState         `yaml:"Processes"`
	PCI                PCIInfo                       `yaml:"PCI"`
	Overclock          *OverclockInfo                `yaml:"Overclock"`
	Diagnostics        []DiagnosticsInfo             `yaml:"Diagnostics"`
	ECC                *ECCInfo                      `yaml:"ECC"`
	EngineGroups       []EngineGroupInfo             `yaml:"EngineGroups"`
	FabricPorts        []FabricPortInfo              `yaml:"FabricPorts"`
	Fans               []FanInfo                     `yaml:"Fans"`
	Firmwares          []FirmwareInfo                `yaml:"Firmwares"`
	FrequencyDomains   []FrequencyDomainInfo         `yaml:"FrequencyDomains"`
	Leds               []LedInfo                     `yaml:"Leds"`
	MemoryModules      []MemoryModuleInfo            `yaml:"MemoryModules"`
	PerformanceDomains []PerformanceDomainInfo       `yaml:"PerformanceDomains"`
	PowerDomains       []PowerDomainInfo             `yaml:"PowerDomains"`
	Psus               []PsuInfo                     `yaml:"Psus"`
	RasErrorSets       []RasInfo                     `yaml:"RasErrorSets"`
	Schedulers         []SchedulerInfo               `yaml:"Schedulers"`
	StandbyDomains     []StandbyDomainInfo           `yaml:"StandbyDomains"`
	TemperatureSensors []TemperatureSensorInfo       `yaml:"TemperatureSensors"`
}

type PCIInfo struct {
	Properties *sysman.PciProperties     `yaml:"Properties"`
	State      *sysman.PciState          `yaml:"State"`
	Bars       []sysman.PciBarProperties `yaml:"Bars"`
	Stats      *sysman.PciStats          `yaml:"Stats"`
}

type ECCInfo struct {
	Available    bool                  `yaml:"Available"`
	Configurable bool                  `yaml:"Configurable"`
	State        *sysman.EccProperties `yaml:"State"`
}

type OverclockInfo struct {
	DomainsBitmask sysman.OverclockDomains `yaml:"DomainsBitmask"`
	State          *sysman.OverclockState  `yaml:"State"`
	Controls       []OverclockControlsInfo `yaml:"Controls"`
	Domains        []OverclockDomainInfo   `yaml:"Domains"`
}

type OverclockControlsInfo struct {
	DomainType      sysman.OverclockDomain   `yaml:"DomainType"`
	ControlsBitmask sysman.OverclockControls `yaml:"ControlsBitmask"`
}

type OverclockDomainControlsInfo struct {
	ControlType   sysman.OverclockControl `yaml:"ControlType"`
	Properties    *sysman.ControlProperty `yaml:"Properties"`
	CurrentValue  *float64                `yaml:"CurrentValue"`
	PendingValue  *float64                `yaml:"PendingValue"`
	State         *sysman.ControlState    `yaml:"State"`
	PendingAction *sysman.PendingAction   `yaml:"PendingAction"`
}

type OverclockDomainInfo struct {
	Properties   *sysman.OverclockProperties   `yaml:"Properties"`
	VFProperties *sysman.VfProperty            `yaml:"VFProperties"`
	ControlInfos []OverclockDomainControlsInfo `yaml:"ControlInfos"`
}

type DiagnosticsInfo struct {
	Properties *sysman.DiagProperties `yaml:"Properties"`
	Tests      []sysman.DiagTest      `yaml:"Tests"`
}

type EngineGroupInfo struct {
	Properties  *sysman.EngineProperties `yaml:"Properties"`
	Activity    *sysman.EngineStats      `yaml:"Activity"`
	ActivityExt []sysman.EngineStats     `yaml:"ActivityExt"`
}

type FabricPortInfo struct {
	Properties    *sysman.FabricPortProperties    `yaml:"Properties"`
	LinkType      *sysman.FabricLinkType          `yaml:"LinkType"`
	Config        *sysman.FabricPortConfig        `yaml:"Config"`
	State         *sysman.FabricPortState         `yaml:"State"`
	Throughput    *sysman.FabricPortThroughput    `yaml:"Throughput"`
	ErrorCounters *sysman.FabricPortErrorCounters `yaml:"ErrorCounters"`
}

type FanInfo struct {
	Properties *sysman.FanProperties `yaml:"Properties"`
	Config     *sysman.FanConfig     `yaml:"Config"`
	Speed      int32                 `yaml:"Speed"`
}

type FirmwareInfo struct {
	Properties *sysman.FirmwareProperties `yaml:"Properties"`
}

type FrequencyDomainInfo struct {
	Properties      *sysman.FreqProperties   `yaml:"Properties"`
	AvailableClocks []float64                `yaml:"AvailableClocks"`
	Range           *sysman.FreqRange        `yaml:"Range"`
	State           *sysman.FreqState        `yaml:"State"`
	ThrottleTime    *sysman.FreqThrottleTime `yaml:"ThrottleTime"`
}

type LedInfo struct {
	Properties *sysman.LedProperties `yaml:"Properties"`
	State      *sysman.LedState      `yaml:"State"`
}

type MemoryModuleInfo struct {
	Properties *sysman.MemProperties `yaml:"Properties"`
	State      *sysman.MemState      `yaml:"State"`
	Bandwidth  *sysman.MemBandwidth  `yaml:"Bandwidth"`
}

type PerformanceDomainInfo struct {
	Properties *sysman.PerfProperties `yaml:"Properties"`
	Config     float64                `yaml:"Config"`
}

type PowerDomainInfo struct {
	Properties      *sysman.PowerProperties    `yaml:"Properties"`
	EnergyCounter   *sysman.PowerEnergyCounter `yaml:"EnergyCounter"`
	EnergyThreshold *sysman.EnergyThreshold    `yaml:"EnergyThreshold"`
	LimitsExt       []sysman.PowerLimitExtDesc `yaml:"LimitsExt"`
	Usage           *sysman.PowerUsage         `yaml:"Usage"`
	LimitExt2       *uint32                    `yaml:"LimitExt2"`
}

type PsuInfo struct {
	Properties *sysman.PsuProperties `yaml:"Properties"`
	State      *sysman.PsuState      `yaml:"State"`
}

// BaseRasInfo holds the RAS error set fields that are always available,
// regardless of whether the experimental (Exp) Sysman API extension is
// used. See types_stable.go/types_exp.go for the RasInfo variants that
// embed this.
type BaseRasInfo struct {
	Properties *sysman.RasProperties `yaml:"Properties"`
	Config     *sysman.RasConfig     `yaml:"Config"`
	State      *sysman.RasState      `yaml:"State"`
}

type SchedulerInfo struct {
	Properties              *sysman.SchedProperties          `yaml:"Properties"`
	CurrentMode             sysman.SchedMode                 `yaml:"CurrentMode"`
	TimeoutModeProperties   *sysman.SchedTimeoutProperties   `yaml:"TimeoutModeProperties"`
	TimesliceModeProperties *sysman.SchedTimesliceProperties `yaml:"TimesliceModeProperties"`
}

type StandbyDomainInfo struct {
	Properties *sysman.StandbyProperties `yaml:"Properties"`
	Mode       sysman.StandbyType        `yaml:"Mode"`
}

type TemperatureSensorInfo struct {
	Properties  *sysman.TempProperties `yaml:"Properties"`
	Config      *sysman.TempConfig     `yaml:"Config"`
	Temperature float64                `yaml:"Temperature"`
}
