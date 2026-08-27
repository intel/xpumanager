//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"errors"
	"time"

	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/sysdev"
)

// Error messages shared with the tests.
const (
	errScanInterval   = "scan_interval must be positive"
	errSettleScans    = "settle_scans must be at least 1"
	errReportInterval = "report_interval must not be negative"
	errNoSubsystems   = "subsystems must not be empty"
	errSysfsRoot      = "sysfs_root must not be empty"
	errDevRoot        = "dev_root must not be empty"
)

// Config defines configuration for the Intel device watch extension.
type Config struct {
	// ScanInterval is how often the sysfs is compared against the device nodes.
	ScanInterval time.Duration `mapstructure:"scan_interval"`

	// SettleScans is the number of consecutive scans that must agree before
	// acting on changes.
	SettleScans int `mapstructure:"settle_scans"`

	// ReportInterval is how often a persisting condition is re-logged. Changes
	// are always logged immediately. This bounds the repetition of a condition
	// that stays the same, such as a permanently inaccessible device.  Zero
	// logs each condition only when it appears or changes.
	ReportInterval time.Duration `mapstructure:"report_interval"`

	// Subsystems are the sysfs device classes to watch. Supported: "drm",
	// "mei".
	// NOTE: watching a subsystem whose device nodes are not mapped into the
	// container makes it report as inaccessible forever.
	Subsystems []string `mapstructure:"subsystems"`

	// VendorIDs restricts the scan to certain PCI vendor IDs (without the "0x"
	// prefix). Empty watches every vendor.
	VendorIDs []string `mapstructure:"vendor_ids"`

	// SysfsRoot is the sysfs mount point.
	// Mainly targeted for testing.
	SysfsRoot string `mapstructure:"sysfs_root"`

	// DevRoot is the root of the device node directory tree.
	// Mainly targeted for testing.
	DevRoot string `mapstructure:"dev_root"`
}

// Validate checks the extension configuration.
func (c *Config) Validate() error {
	if c.ScanInterval <= 0 {
		return errors.New(errScanInterval)
	}
	if c.SettleScans < 1 {
		return errors.New(errSettleScans)
	}
	if c.ReportInterval < 0 {
		return errors.New(errReportInterval)
	}
	if len(c.Subsystems) == 0 {
		return errors.New(errNoSubsystems)
	}
	if _, err := sysdev.ParseSubsystems(c.Subsystems); err != nil {
		return err
	}
	if _, err := sysdev.ParseVendorIDs(c.VendorIDs); err != nil {
		return err
	}
	if c.SysfsRoot == "" {
		return errors.New(errSysfsRoot)
	}
	if c.DevRoot == "" {
		return errors.New(errDevRoot)
	}
	return nil
}

// scanner builds the inventory scanner described by the configuration. The
// configuration must have been validated.
func (c *Config) scanner() (*sysdev.Scanner, error) {
	subs, err := sysdev.ParseSubsystems(c.Subsystems)
	if err != nil {
		return nil, err
	}
	vendors, err := sysdev.ParseVendorIDs(c.VendorIDs)
	if err != nil {
		return nil, err
	}
	return &sysdev.Scanner{
		SysfsRoot:  c.SysfsRoot,
		DevRoot:    c.DevRoot,
		Subsystems: subs,
		VendorIDs:  vendors,
		Probe:      sysdev.ProbeOpen,
	}, nil
}
