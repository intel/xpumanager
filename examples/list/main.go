package main

import (
	"fmt"
	"log"
	"strings"

	"github.com/intel/level-zero-go/levelzero"
	"go.yaml.in/yaml/v4"
)

func main() {
	if ret := levelzero.ZesInit(0); ret != nil {
		log.Fatalf("Failed to initialize System Resource Management (sysman): %v", ret)
	}

	drivers, err := levelzero.ZesDriverGet()
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

		devices, err := driver.ZesDeviceGet()
		if err != nil {
			log.Fatalf("Failed to get devices for driver %d: %v", i, err)
		}
		fmt.Printf("Driver %d has %d devices\n", i, len(devices))

		for j, device := range devices {
			fmt.Println("--------------------------------------------------")
			fmt.Println("# Device", j, "of Driver", i)

			printDevice(device)

			printPCI(device)

			printECC(device)

			printProcs(device)

			printOverock(device)

			printDiags(device)

			printEngineGroups(device)

			printFabricPorts(device)

			printFans(device)

			printFirmwares(device)

			printFrequencyDomains(device)

			printLeds(device)

			printMemory(device)

			printPerf(device)

			printPower(device)

			printPsu(device)

			printRas(device)

			printSched(device)
		}
	}
}

func printDevice(device *levelzero.ZeDevice) {
	props, err := device.GetProperties()
	if err != nil {
		log.Printf("ERROR: Failed to get device properties: %v", err)
		return
	}
	fmt.Printf("## Device Properties:\n")
	dump(props, 2)

	state, err := device.GetState()
	if err != nil {
		log.Printf("ERROR: Failed to get device state: %v", err)
		return
	}
	fmt.Printf("## Device State:\n")
	dump(state, 2)

	subDeviceProps, err := device.GetSubDevicePropertiesExp()
	if err != nil {
		log.Printf("ERROR: Failed to get sub-device properties: %v", err)
		return
	}
	fmt.Printf("## Sub-Device Properties:\n")
	dump(subDeviceProps, 2)
}

func printPCI(device *levelzero.ZeDevice) {
	props, err := device.PciGetProperties()
	if err != nil {
		log.Printf("ERROR: Failed to get PCI properties: %v", err)
		return
	}
	fmt.Printf("## PCI:\n")
	fmt.Printf("  Properties:\n")
	dump(props, 4)

	pciState, err := device.PciGetState()
	if err != nil {
		log.Printf("ERROR: Failed to get PCI state: %v", err)
		return
	}
	fmt.Printf("  State:\n")
	dump(pciState, 4)

	pciBars, err := device.PciGetBars()
	if err != nil {
		log.Printf("ERROR: Failed to get PCI BARs: %v", err)
		return
	}
	fmt.Printf("  BARs:\n")
	dump(pciBars, 4)

	pciStats, err := device.PciGetStats()
	if err != nil {
		log.Printf("ERROR: Failed to get PCI stats: %v", err)
		return
	}
	fmt.Printf("  Stats:\n")
	dump(pciStats, 4)
}

func printECC(device *levelzero.ZeDevice) {
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

func printProcs(device *levelzero.ZeDevice) {
	procs, err := device.ProcessesGetState()
	if err != nil {
		log.Printf("ERROR: Failed to get processes state: %v", err)
		return
	}
	fmt.Printf("## Processes State:\n")
	dump(procs, 2)
}

func printOverock(device *levelzero.ZeDevice) {
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
		dump(state, 2)
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
			dump(props, 4)
		}

		vfProps, err := domain.GetDomainVFProperties()
		if err != nil {
			log.Printf("ERROR: Failed to get VF properties for overclocking domain %d: %v", i, err)
		} else {
			fmt.Printf("  VF Properties:\n")
			dump(vfProps, 4)
		}
	}
}

func printDiags(device *levelzero.ZeDevice) {
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
			dump(props, 4)
		}

		tests, err := diag.GetTests()
		if err != nil {
			log.Printf("ERROR: Failed to get tests for diagnostic test suite %d: %v", i, err)
		} else {
			fmt.Printf("  Tests:\n")
			dump(tests, 4)
		}
	}
}

func printEngineGroups(device *levelzero.ZeDevice) {
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
			dump(props, 4)
		}

		stats, err := engine.GetActivity()
		if err != nil {
			log.Printf("ERROR: Failed to get activity for engine group %d: %v", i, err)
		} else {
			fmt.Printf("  Activity:\n")
			dump(stats, 4)
		}

		activityExt, err := engine.GetActivityExt()
		if err != nil {
			log.Printf("ERROR: Failed to get extended activity for engine group %d: %v", i, err)
		} else {
			fmt.Printf("  Extended Activity:\n")
			dump(activityExt, 4)
		}
	}
}

func printFabricPorts(device *levelzero.ZeDevice) {
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
			dump(props, 4)
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
			dump(config, 4)
		}

		state, err := port.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for fabric port %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			dump(state, 4)
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

func printFans(device *levelzero.ZeDevice) {
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
			dump(props, 4)
		}

		config, err := fan.GetConfig()
		if err != nil {
			log.Printf("ERROR: Failed to get config for fan %d: %v", i, err)
		} else {
			fmt.Printf("  Config:\n")
			dump(config, 4)
		}

		speed, err := fan.GetState(levelzero.ZES_FAN_SPEED_UNITS_RPM)
		if err != nil {
			log.Printf("ERROR: Failed to get speed for fan %d: %v", i, err)
		} else {
			fmt.Printf("  Speed: %d\n", speed)
		}
	}
}

func printFirmwares(device *levelzero.ZeDevice) {
	firmwares, err := device.EnumFirmwares()
	if err != nil {
		log.Printf("ERROR: Failed to enumerate firmwares: %v", err)
		return
	}

	fmt.Printf("## Found %d firmwares\n", len(firmwares))
}

func printFrequencyDomains(device *levelzero.ZeDevice) {
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
			dump(props, 4)
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
			dump(state, 4)
		}

		throttleTime, err := domain.GetThrottleTime()
		if err != nil {
			log.Printf("ERROR: Failed to get throttle time for frequency domain %d: %v", i, err)
		} else {
			fmt.Printf("  Throttle Time:\n")
			dump(throttleTime, 4)
		}
	}
}

func printLeds(device *levelzero.ZeDevice) {
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
			dump(props, 4)
		}

		state, err := led.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for LED %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			dump(state, 4)
		}
	}

	fmt.Printf("## Found %d LEDs\n", len(leds))
}

func printMemory(device *levelzero.ZeDevice) {
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
			dump(props, 4)
		}

		stats, err := module.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for memory module %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			dump(stats, 4)
		}

		bandwidth, err := module.GetBandwidth()
		if err != nil {
			log.Printf("ERROR: Failed to get bandwidth for memory module %d: %v", i, err)
		} else {
			fmt.Printf("  Bandwidth:\n")
			dump(bandwidth, 4)
		}
	}
}

func printPerf(device *levelzero.ZeDevice) {
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
			dump(props, 4)
		}

		factor, err := domain.GetConfig()
		if err != nil {
			log.Printf("ERROR: Failed to get config for performance domain %d: %v", i, err)
		} else {
			fmt.Printf("  Config: %+v\n", factor)
		}
	}
}

func printPower(device *levelzero.ZeDevice) {
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
			dump(props, 4)
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
			dump(limits, 4)
		}
	}
}

func printPsu(device *levelzero.ZeDevice) {
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
			dump(props, 4)
		}

		state, err := psu.GetState()
		if err != nil {
			log.Printf("ERROR: Failed to get state for PSU %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			dump(state, 4)
		}
	}
}

func printRas(device *levelzero.ZeDevice) {
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
			dump(props, 4)
		}

		config, err := ras.GetConfig()
		if err != nil {
			log.Printf("ERROR: Failed to get config for RAS error set %d: %v", i, err)
		} else {
			fmt.Printf("  Config:\n")
			dump(config, 4)
		}

		state, err := ras.GetState(false)
		if err != nil {
			log.Printf("ERROR: Failed to get state for RAS error set %d: %v", i, err)
		} else {
			fmt.Printf("  State:\n")
			dump(state, 4)
		}

		stateExp, err := ras.GetStateExp()
		if err != nil {
			log.Printf("ERROR: Failed to get extended state for RAS error set %d: %v", i, err)
		} else {
			fmt.Printf("  Extended State:\n")
			dump(stateExp, 4)
		}
	}
}

func printSched(device *levelzero.ZeDevice) {
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
			dump(props, 4)
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
			dump(timoutModeProps, 4)
		}

		timesliceModeProps, err := sched.GetTimesliceModeProperties(false)
		if err != nil {
			log.Printf("ERROR: Failed to get timeslice mode properties for scheduler %d: %v", i, err)
		} else {
			fmt.Printf("  Timeslice Mode Properties:\n")
			dump(timesliceModeProps, 4)
		}
	}
}

func dump(obj interface{}, indent int) {
	prefix := strings.Repeat(" ", indent)
	data, err := yaml.Marshal(obj)
	if err != nil {
		log.Printf("ERROR: Failed to marshal object: %v", err)
		return
	}
	lines := strings.Split(string(data), "\n")
	for _, line := range lines {
		if line != "" {
			fmt.Printf("%s%s\n", prefix, line)
		}
	}
}
