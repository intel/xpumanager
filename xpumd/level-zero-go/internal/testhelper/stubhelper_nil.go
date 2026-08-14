//go:build lint

// SPDX-License-Identifier: MIT
// Copyright (C) 2026 Intel Corporation

package testhelper

// stubReload nil implementation for linting unit tests.
func stubReload(configFilePath string) error {
	return nil
}
