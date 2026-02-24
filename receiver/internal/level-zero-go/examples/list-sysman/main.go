// Copyright (C) 2026-2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"flag"
	"log"

	"github.com/intel/level-zero-go/examples/common"
	"github.com/intel/level-zero-go/sysman"
)

func main() {
	format := flag.String("format", "yaml", "Output format: json or yaml")
	flag.Parse()

	if ret := sysman.Init(0); ret != nil {
		log.Fatalf("Failed to initialize System Resource Management (sysman): %v", ret)
	}

	sysInfo, err := collectSystemInfo()
	if err != nil {
		log.Fatalf("Failed to collect system information: %v", err)
	}

	switch *format {
	case "json":
		common.DumpJSON(sysInfo)
	case "yaml":
		common.DumpYAML(sysInfo)
	default:
		log.Fatalf("Unknown format: %s (use json or yaml)", *format)
	}
}
