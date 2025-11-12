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
