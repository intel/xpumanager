//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"os"
	"testing"
	"time"
)

// pollUntil polls cond every interval until it returns nil, or fails with last error from cond if timeout is reached.
// NOTE: cond may itself call t.Fatalf for a failure that polling cannot fix.
func pollUntil(t *testing.T, what string, timeout, interval time.Duration, cond func() error) {
	t.Helper()

	deadline := time.Now().Add(timeout)
	var lastErr error
	for {
		if lastErr = cond(); lastErr == nil {
			return
		}
		if time.Now().After(deadline) {
			t.Fatalf("timed out after %v waiting for %s: %v", timeout, what, lastErr)
		}
		time.Sleep(interval)
	}
}

// fileExists reports whether path exists.
func fileExists(path string) bool {
	_, err := os.Stat(path)
	return !os.IsNotExist(err)
}
