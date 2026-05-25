//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package intelcrashlog

import (
	"fmt"
	"path/filepath"
	"time"
)

// Config defines configuration for the Intel Crashlog receiver.
type Config struct {
	// Directory is the path to watch for new crashlog files.
	// Must not be empty.
	Directory string `mapstructure:"directory"`

	// Glob is a filename glob pattern matched against the base name of files.
	// Must not be empty.
	Glob string `mapstructure:"glob"`

	// IgnoreOlderThan causes files whose modification time is older than
	// (startup_time - ignore_older_than) to be ignored.
	// Zero (the default) means only files modified at or after startup time are processed.
	IgnoreOlderThan time.Duration `mapstructure:"ignore_older_than"`

	// AddAttributes is a map of static key/value pairs added to every emitted log record.
	AddAttributes map[string]string `mapstructure:"add_attributes"`
}

// Validate checks the receiver configuration.
func (c *Config) Validate() error {
	if c.Directory == "" {
		return fmt.Errorf("directory must not be empty")
	}
	if c.Glob == "" {
		return fmt.Errorf("glob must not be empty")
	}
	if _, err := filepath.Match(c.Glob, ""); err != nil {
		return fmt.Errorf("glob is not a valid pattern: %w", err)
	}

	if c.IgnoreOlderThan < 0 {
		return fmt.Errorf("ignore_older_than must not be negative")
	}
	return nil
}
