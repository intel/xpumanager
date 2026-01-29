package main

import (
	"fmt"
	"log"

	"github.com/intel/level-zero-go/examples/common"
	"github.com/intel/level-zero-go/sysman"
)

func main() {
	if ret := sysman.Init(0); ret != nil {
		log.Fatalf("Failed to initialize System Resource Management (sysman): %v", ret)
	}

	drivers, err := sysman.DriverGet()
	if err != nil {
		log.Fatalf("Failed to get drivers: %v", err)
	}

	fmt.Printf("Found %d drivers\n", len(drivers))

	for i, driver := range drivers {
		fmt.Println("**************************************************")
		fmt.Println("* DRIVER #", i)
		fmt.Println("**************************************************")
		extProps, err := driver.GetExtensionProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get driver extension properties: %v", err)
		} else {
			fmt.Printf("Driver %d extension properties: %+v\n", i, extProps)
		}

		devices, err := driver.DeviceGet()
		if err != nil {
			log.Fatalf("Failed to get devices for driver %d: %v", i, err)
		}
		fmt.Printf("Driver %d has %d devices\n", i, len(devices))

		for j, device := range devices {
			fmt.Println("--------------------------------------------------")
			fmt.Println("# Device", j, "of Driver", i)

			printDevice(device)

			printProcs(device)

			printPCI(device)

			printOverclock(device)

			printDiags(device)

			printECC(device)

			printEngineGroups(device)

			printFabricPorts(device)

			printFans(device)

			printFirmwares(device)

			printFrequency(device)

			printLeds(device)

			printMemory(device)

			printPerf(device)

			printPower(device)

			printPsu(device)

			printRas(device)

			printSched(device)

			printStandby(device)

			printTemperature(device)
		}
	}
}

func printDevice(device *sysman.Device) {
	props, err := device.GetProperties()
	if err != nil {
		log.Printf("ERROR: Failed to get device properties: %v", err)
		return
	}
	fmt.Printf("## Device Properties:\n")
	common.Dump(props, 2)

	state, err := device.GetState()
	if err != nil {
		log.Printf("ERROR: Failed to get device state: %v", err)
		return
	}
	fmt.Printf("## Device State:\n")
	common.Dump(state, 2)
}

func printProcs(device *sysman.Device) {
	procs, err := device.ProcessesGetState()
	if err != nil {
		log.Printf("ERROR: Failed to get processes state: %v", err)
		return
	}
	fmt.Printf("## Processes State:\n")
	common.Dump(procs, 2)
}

func printPCI(device *sysman.Device) {
	props, err := device.PciGetProperties()
	if err != nil {
		log.Printf("ERROR: Failed to get PCI properties: %v", err)
		return
	}
	fmt.Printf("## PCI:\n")
	fmt.Printf("  Properties:\n")
	common.Dump(props, 4)

	pciState, err := device.PciGetState()
	if err != nil {
		log.Printf("ERROR: Failed to get PCI state: %v", err)
		return
	}
	fmt.Printf("  State:\n")
	common.Dump(pciState, 4)

	pciBars, err := device.PciGetBars()
	if err != nil {
		log.Printf("ERROR: Failed to get PCI BARs: %v", err)
		return
	}
	fmt.Printf("  BARs:\n")
	common.Dump(pciBars, 4)

	pciStats, err := device.PciGetStats()
	if err != nil {
		log.Printf("ERROR: Failed to get PCI stats: %v", err)
		return
	}
	fmt.Printf("  Stats:\n")
	common.Dump(pciStats, 4)
}

func printOverclock(device *sysman.Device) {
	domainBitmask, err := device.GetOverclockDomains()
	if err != nil {
		log.Printf("ERROR: Failed to get overclocking domains bitmask: %v", err)
		return
	}
	fmt.Printf("## Overclocking Domains Bitmask: %0x\n", domainBitmask)

	state, err := device.ReadOverclockState()
	if err != nil {
		log.Printf("ERROR: Failed to get overclocking state: %v", err)
	} else {
		fmt.Printf("## Overclocking State:\n")
		common.Dump(state, 2)
	}

	domains, err := device.EnumOverclockDomains()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate overclocking domains: %v", err)
		return
	}

	fmt.Printf("## Found %d overclocking domains\n", len(domains))
	for i, domain := range domains {
		fmt.Printf("### Overclocking Domain %d\n", i)

		props, err := domain.GetDomainProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for overclocking domain %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		vfProps, err := domain.GetDomainVFProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get VF properties for overclocking domain %d: %v", i, err)
		} else {
			fmt.Printf("  VF Properties:\n")
			common.Dump(vfProps, 4)
		}
	}
}

func printDiags(device *sysman.Device) {
	diags, err := device.EnumDiagnosticTestSuites()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate diagnostic test suites: %v", err)
		return
	}

	fmt.Printf("## Found %d diagnostic test suites\n", len(diags))
	for i, diag := range diags {
		fmt.Printf("### Diagnostic Test Suite %d\n", i)

		props, err := diag.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for diagnostic test suite %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		tests, err := diag.GetTests()
		if err != nil {
			log.Printf("ERROR: Failed to get tests for diagnostic test suite %d: %v", i, err)
		} else {
			fmt.Printf("  Tests:\n")
			common.Dump(tests, 4)
		}
	}
}

func printECC(device *sysman.Device) {
	eccAvailable, err := device.EccAvailable()
	if err != nil {
		log.Printf("ERROR: Failed to check ECC availability: %v", err)
		return
	}
	fmt.Printf("## ECC:\n  Available: %v\n", eccAvailable)
	if !eccAvailable {
		return
	}

	eccConfigurable, err := device.EccConfigurable()
	if err != nil {
		log.Printf("ERROR: Failed to check ECC configurability: %v", err)
	} else {
		fmt.Printf("  Configurable: %v\n", eccConfigurable)
	}

	eccState, err := device.GetEccState()
	if err != nil {
		log.Printf("ERROR: Failed to get ECC state: %v", err)
	} else {
		fmt.Printf("  State: %+v\n", eccState)
	}
}

func printEngineGroups(device *sysman.Device) {
	engines, err := device.EnumEngineGroups()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate engine groups: %v", err)
		return
	}

	fmt.Printf("## Found %d engine groups\n", len(engines))
	for i, engine := range engines {
		fmt.Printf("### Engine Group %d\n", i)

		props, err := engine.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for engine group %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		stats, err := engine.GetActivity()
		if err != nil {
			log.Printf("ERROR: Failed to get activity for engine group %d: %v", i, err)
		} else {
			fmt.Printf("  Activity:\n")
			common.Dump(stats, 4)
		}

		activityExt, err := engine.GetActivityExt()
		if err != nil {
			log.Printf("ERROR: Failed to get extended activity for engine group %d: %v", i, err)
		} else {
			fmt.Printf("  Extended Activity:\n")
			common.Dump(activityExt, 4)
		}
	}
}

func printFabricPorts(device *sysman.Device) {
	fabricPorts, err := device.EnumFabricPorts()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate fabric ports: %v", err)
		return
	}

	fmt.Printf("## Found %d fabric ports\n", len(fabricPorts))
	for i, port := range fabricPorts {
		fmt.Printf("### Fabric Port %d\n", i)

		props, err := port.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for fabric port %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		linkType, err := port.GetLinkType()
		if err != nil {
			log.Printf("ERROR: Failed to get link type for fabric port %d: %v", i, err)
		} else {
			fmt.Printf("  Link Type: %+v\n", linkType)
		}

		config, err := port.GetConfig()
		if err != nil {
			log.Printf("ERROR: Failed to get config for fabric port %d: %v", i, err)
		} else {
			fmt.Printf("  Config:\n")
			common.Dump(config, 4)
		}

		state, err := port.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for fabric port %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			common.Dump(state, 4)
		}

		throughput, err := port.GetThroughput()
		if err != nil {
			log.Printf("ERROR: Failed to get throughput for fabric port %d: %v", i, err)
		} else {
			fmt.Printf("  Throughput: %+v\n", throughput)
		}

		errCounters, err := port.GetFabricErrorCounters()
		if err != nil {
			log.Printf("ERROR: Failed to get error counters for fabric port %d: %v", i, err)
		} else {
			fmt.Printf("  Error Counters: %+v\n", errCounters)
		}
	}
}

func printFans(device *sysman.Device) {
	fans, err := device.EnumFans()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate fans: %v", err)
		return
	}

	fmt.Printf("## Found %d fans\n", len(fans))
	for i, fan := range fans {
		fmt.Printf("### Fan %d\n", i)

		props, err := fan.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for fan %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		config, err := fan.GetConfig()
		if err != nil {
			log.Printf("ERROR: Failed to get config for fan %d: %v", i, err)
		} else {
			fmt.Printf("  Config:\n")
			common.Dump(config, 4)
		}

		speed, err := fan.GetState(sysman.FAN_SPEED_UNITS_RPM)
		if err != nil {
			log.Printf("ERROR: Failed to get speed for fan %d: %v", i, err)
		} else {
			fmt.Printf("  Speed: %d\n", speed)
		}
	}
}

func printFirmwares(device *sysman.Device) {
	firmwares, err := device.EnumFirmwares()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate firmwares: %v", err)
		return
	}

	fmt.Printf("## Found %d firmwares\n", len(firmwares))
}

func printFrequency(device *sysman.Device) {
	freqDomains, err := device.EnumFrequencyDomains()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate frequency domains: %v", err)
		return
	}

	fmt.Printf("## Found %d frequency domains\n", len(freqDomains))
	for i, domain := range freqDomains {
		fmt.Printf("### Frequency Domain %d\n", i)

		props, err := domain.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for frequency domain %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		clocks, err := domain.GetAvailableClocks()
		if err != nil {
			log.Printf("ERROR: Failed to get available clocks for frequency domain %d: %v", i, err)
		} else {
			fmt.Printf("  Available Clocks: %+v\n", clocks)
		}

		domainRange, err := domain.GetRange()
		if err != nil {
			log.Printf("ERROR: Failed to get range for frequency domain %d: %v", i, err)
		} else {
			fmt.Printf("  Range: %+v\n", domainRange)
		}

		state, err := domain.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for frequency domain %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			common.Dump(state, 4)
		}

		throttleTime, err := domain.GetThrottleTime()
		if err != nil {
			log.Printf("ERROR: Failed to get throttle time for frequency domain %d: %v", i, err)
		} else {
			fmt.Printf("  Throttle Time:\n")
			common.Dump(throttleTime, 4)
		}
	}
}

func printLeds(device *sysman.Device) {
	leds, err := device.EnumLeds()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate LEDs: %v", err)
		return
	}

	for i, led := range leds {
		fmt.Printf("### LED %d\n", i)
		props, err := led.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for LED %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		state, err := led.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for LED %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			common.Dump(state, 4)
		}
	}

	fmt.Printf("## Found %d LEDs\n", len(leds))
}

func printMemory(device *sysman.Device) {
	memModules, err := device.EnumMemoryModules()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate memory modules: %v", err)
		return
	}

	fmt.Printf("## Found %d memory modules\n", len(memModules))
	for i, module := range memModules {
		fmt.Printf("### Memory Module %d\n", i)

		props, err := module.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for memory module %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		stats, err := module.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for memory module %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			common.Dump(stats, 4)
		}

		bandwidth, err := module.GetBandwidth()
		if err != nil {
			log.Printf("ERROR: Failed to get bandwidth for memory module %d: %v", i, err)
		} else {
			fmt.Printf("  Bandwidth:\n")
			common.Dump(bandwidth, 4)
		}
	}
}

func printPerf(device *sysman.Device) {
	perfDomains, err := device.EnumPerformanceFactorDomains()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate performance factor domains: %v", err)
		return
	}

	fmt.Printf("## Found %d performance domains\n", len(perfDomains))
	for i, domain := range perfDomains {
		fmt.Printf("### Performance Domain %d\n", i)

		props, err := domain.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for performance domain %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		factor, err := domain.GetConfig()
		if err != nil {
			log.Printf("ERROR: Failed to get config for performance domain %d: %v", i, err)
		} else {
			fmt.Printf("  Config: %+v\n", factor)
		}
	}
}

func printPower(device *sysman.Device) {
	powerDomains, err := device.EnumPowerDomains()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate power domains: %v", err)
		return
	}
	fmt.Printf("## Found %d power domains\n", len(powerDomains))
	for i, domain := range powerDomains {
		fmt.Printf("### Power Domain %d\n", i)

		props, err := domain.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for power domain %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		energyCounter, err := domain.GetEnergyCounter()
		if err != nil {
			log.Printf("ERROR: Failed to get energy counter for power domain %d: %v", i, err)
		} else {
			fmt.Printf("  Energy Counter: %+v\n", energyCounter)
		}

		energyThreshold, err := domain.GetEnergyThreshold()
		if err != nil {
			log.Printf("ERROR: Failed to get energy threshold for power domain %d: %v", i, err)
		} else {
			fmt.Printf("  Energy Threshold: %+v\n", energyThreshold)
		}

		limits, err := domain.GetLimitsExt()
		if err != nil {
			log.Printf("ERROR: Failed to get limits for power domain %d: %v", i, err)
		} else {
			fmt.Printf("  Limits:\n")
			common.Dump(limits, 4)
		}
	}
}

func printPsu(device *sysman.Device) {
	psus, err := device.EnumPsus()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate PSUs: %v", err)
		return
	}

	fmt.Printf("## Found %d PSUs\n", len(psus))
	for i, psu := range psus {
		fmt.Printf("### PSU %d\n", i)

		props, err := psu.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for PSU %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		state, err := psu.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for PSU %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			common.Dump(state, 4)
		}
	}
}

func printRas(device *sysman.Device) {
	rasDomains, err := device.EnumRasErrorSets()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate RAS error sets: %v", err)
		return
	}

	fmt.Printf("## Found %d RAS error sets\n", len(rasDomains))
	for i, ras := range rasDomains {
		fmt.Printf("### RAS Error Set %d\n", i)

		props, err := ras.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for RAS error set %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		config, err := ras.GetConfig()
		if err != nil {
			log.Printf("ERROR: Failed to get config for RAS error set %d: %v", i, err)
		} else {
			fmt.Printf("  Config:\n")
			common.Dump(config, 4)
		}

		state, err := ras.GetState(false)
		if err != nil {
			log.Printf("ERROR: Failed to get state for RAS error set %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			common.Dump(state, 4)
		}
	}
}

func printSched(device *sysman.Device) {
	scheds, err := device.EnumSchedulers()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate schedulers: %v", err)
		return
	}

	fmt.Printf("## Found %d schedulers\n", len(scheds))
	for i, sched := range scheds {
		fmt.Printf("### Scheduler %d\n", i)

		props, err := sched.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for scheduler %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		mode, err := sched.GetCurrentMode()
		if err != nil {
			log.Printf("ERROR: Failed to get current mode for scheduler %d: %v", i, err)
		} else {
			fmt.Printf("  Current Mode: %v\n", mode)
		}

		timoutModeProps, err := sched.GetTimeoutModeProperties(false)
		if err != nil {
			log.Printf("ERROR: Failed to get timeout mode properties for scheduler %d: %v", i, err)
		} else {
			fmt.Printf("  Timeout Mode Properties:\n")
			common.Dump(timoutModeProps, 4)
		}

		timesliceModeProps, err := sched.GetTimesliceModeProperties(false)
		if err != nil {
			log.Printf("ERROR: Failed to get timeslice mode properties for scheduler %d: %v", i, err)
		} else {
			fmt.Printf("  Timeslice Mode Properties:\n")
			common.Dump(timesliceModeProps, 4)
		}
	}
}

func printStandby(device *sysman.Device) {
	standbys, err := device.EnumStandbyDomains()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate standby domains: %v", err)
		return
	}

	fmt.Printf("## Found %d standby domains\n", len(standbys))
	for i, standby := range standbys {
		fmt.Printf("### Standby Domain %d\n", i)

		props, err := standby.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for standby domain %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		mode, err := standby.GetMode()
		if err != nil {
			log.Printf("ERROR: Failed to get mode for standby domain %d: %v", i, err)
		} else {
			fmt.Printf("  Mode: %v\n", mode)
		}
	}
}

func printTemperature(device *sysman.Device) {
	tempSensors, err := device.EnumTemperatureSensors()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate temperature sensors: %v", err)
		return
	}

	fmt.Printf("## Found %d temperature sensors\n", len(tempSensors))
	for i, sensor := range tempSensors {
		fmt.Printf("### Temperature Sensor %d\n", i)

		props, err := sensor.GetProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get properties for temperature sensor %d: %v", i, err)
		} else {
			fmt.Printf("  Properties:\n")
			common.Dump(props, 4)
		}

		config, err := sensor.GetConfig()
		if err != nil {
			log.Printf("ERROR: Failed to get config for temperature sensor %d: %v", i, err)
		} else {
			fmt.Printf("  Config:\n")
			common.Dump(config, 4)
		}

		temp, err := sensor.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for temperature sensor %d: %v", i, err)
		} else {
			fmt.Printf("  Temperature: %+v\n", temp)
		}
	}
}
