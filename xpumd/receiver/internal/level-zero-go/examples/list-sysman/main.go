// Copyright (C) 2026-2026 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

package main

import (
	"encoding/json"
	"flag"
	"log"
	"os"

	"github.com/intel/level-zero-go/examples/common"
	"github.com/intel/level-zero-go/sysman"
	yaml "go.yaml.in/yaml/v4"
)

func main() {
	format := flag.String("format", "yaml", "Output format: json, yaml, json-raw, or yaml-raw")
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
	case "json-raw":
		encoder := json.NewEncoder(os.Stdout)
		encoder.SetIndent("", "  ")
		if err := encoder.Encode(sysInfo); err != nil {
			log.Fatalf("Failed to encode raw JSON: %v", err)
		}
	case "yaml-raw":
		encoder := yaml.NewEncoder(os.Stdout)
		if err := encoder.Encode(sysInfo); err != nil {
			log.Fatalf("Failed to encode raw YAML: %v", err)
		}
		if err := encoder.Close(); err != nil {
			log.Fatalf("Failed to close YAML encoder: %v", err)
		}
	default:
		log.Fatalf("Unknown format: %s (use json, yaml, json-raw, or yaml-raw)", *format)
	}
}
