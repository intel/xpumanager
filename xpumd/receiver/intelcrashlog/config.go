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

	// Globs are filename glob patterns matched against the base name of files.
	// A file is processed if it matches any of them. Must not be empty.
	Globs []string `mapstructure:"globs"`

	// IgnoreOlderThan causes files whose modification time is older than
	// (startup_time - ignore_older_than) to be ignored.
	// Zero (the default) means only files modified at or after startup time are processed.
	IgnoreOlderThan time.Duration `mapstructure:"ignore_older_than"`

	// MaxFileSize is the maximum size of a crashlog file in bytes. Larger files are skipped.
	// Must be positive.
	MaxFileSize int64 `mapstructure:"max_file_size"`

	// AddAttributes is a map of static key/value pairs added to every emitted log record.
	AddAttributes map[string]string `mapstructure:"add_attributes"`
}

// Validate checks the receiver configuration.
func (c *Config) Validate() error {
	if c.Directory == "" {
		return fmt.Errorf("directory must not be empty")
	}
	if len(c.Globs) == 0 {
		return fmt.Errorf("globs must not be empty")
	}
	for _, glob := range c.Globs {
		if glob == "" {
			return fmt.Errorf("globs must not contain empty patterns")
		}
		if _, err := filepath.Match(glob, ""); err != nil {
			return fmt.Errorf("glob %q is not a valid pattern: %w", glob, err)
		}
	}

	if c.IgnoreOlderThan < 0 {
		return fmt.Errorf("ignore_older_than must not be negative")
	}
	if c.MaxFileSize <= 0 {
		return fmt.Errorf("max_file_size must be positive")
	}
	return nil
}
