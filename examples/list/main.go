package main

import (
	"fmt"
	"log"

	"github.com/intel/level-zero-go/levelzero"
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
	}
}
