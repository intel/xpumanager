//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"context"
	"testing"
	"time"
)

const (
	fakeSysfsRoot       = "/test/sysfs"
	fakeDevDRI          = "/test/dev/dri"
	testHelperContainer = "test-helper"
	xpumdContainer      = "xpumd"

	// msgWatching is what the extension logs once it has got a baseline (of devices)
	msgWatching = "watching for device changes"
	// msgRestartLimitReached mirrors the extension's log message
	msgRestartLimitReached = "device set changed but the restart limit is reached"
)

const (
	// /dev/null and /dev/zero never restricted by devices cgroup, so they classify as "ok"
	devNullMajor, devNullMinor = 1, 3
	devZeroMajor, devZeroMinor = 1, 5
	// /dev/kmsg: denied by the default devices cgroup (EPERM), so classified as "denied"
	devKmsgMajor, devKmsgMinor = 1, 11
	// The real DRM device major
	devDrmMajor = 226
)

// TestDeviceWatch verifies the scan/reconcile of the intel_device_watch extension.
func TestDeviceWatch(t *testing.T) {
	tc := newTestConfig(t)
	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	tunnel := tc.forwardPort(t, 8888)
	t.Cleanup(tunnel.stop)
	endpoint := tunnel.endpoint()

	fx := newSysfsHelper(tc, testHelperContainer)

	t.Run("MissingNode", func(t *testing.T) {
		dev := devSpec{
			bdf:      "0000:00:02.0",
			name:     "card0",
			vendorID: "8086",
			deviceID: "56a0",
			major:    devDrmMajor,
			minor:    0,
		}
		fx.addDRMDevice(t, fakeSysfsRoot, dev)
		t.Cleanup(func() { fx.removeDRMDevice(t, fakeSysfsRoot, dev) })

		// No device node created: sysfs knows about the device, /dev/dri does not.
		metricAssertion{
			Name:   "otelcol_device_watch_devices",
			Labels: labels{"state": "missing"},
			Value:  new(1.0),
		}.waitFor(t, endpoint, 30*time.Second)
	})

	t.Run("Denied", func(t *testing.T) {
		dev := devSpec{
			bdf:      "0000:00:03.0",
			name:     "card1",
			vendorID: "8086",
			deviceID: "56a0",
			major:    devKmsgMajor,
			minor:    devKmsgMinor,
		}
		fx.addDRMDevice(t, fakeSysfsRoot, dev)
		t.Cleanup(func() { fx.removeDRMDevice(t, fakeSysfsRoot, dev) })
		fx.mknodDevice(t, fakeDevDRI, dev)
		t.Cleanup(func() { fx.removeNode(t, fakeDevDRI, dev.name) })

		metricAssertion{
			Name:   "otelcol_device_watch_devices",
			Labels: labels{"state": "denied"},
			Value:  new(1.0),
		}.waitFor(t, endpoint, 30*time.Second)
	})
}

// TestDeviceWatchExit verifies the exit/restart behavior triggered by the intel_device_watch extension.
func TestDeviceWatchExit(t *testing.T) {
	tc := newTestConfig(t)
	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	fx := newSysfsHelper(tc, testHelperContainer)

	// A GPU with a usable device node, so the change is one a restart can fix.
	dev := devSpec{
		bdf:      "0000:00:02.0",
		name:     "card0",
		vendorID: "8086",
		deviceID: "56a0",
		major:    devNullMajor,
		minor:    devNullMinor,
	}
	fx.addDRMDevice(t, fakeSysfsRoot, dev)
	fx.mknodDevice(t, fakeDevDRI, dev)

	tc.k8sClient.waitForContainerRestarts(t, tc.podName, xpumdContainer, 1, 30*time.Second)

	pod, err := tc.k8sClient.getPod(context.Background(), tc.podName)
	if err != nil {
		t.Fatalf("failed to get pod %q: %v", tc.podName, err)
	}
	cs := containerStatus(pod, xpumdContainer)
	if cs == nil || cs.LastTerminationState.Terminated == nil {
		t.Fatalf("xpumd container has no termination record after restart")
	}
	if got := cs.LastTerminationState.Terminated.ExitCode; got != 0 {
		t.Errorf("expected a graceful exit (code 0) on device set change, got exit code %d", got)
	}
}

// TestDeviceWatchRestartLimit verifies restart rate limiting behavior of the intel_device_watch extension.
func TestDeviceWatchRestartLimit(t *testing.T) {
	tc := newTestConfig(t)
	t.Cleanup(func() { tc.cleanup(t) })
	tc.setup(t)

	fx := newSysfsHelper(tc, testHelperContainer)

	// First change, first restart (of the quota of 1 allowed by max_restarts)
	card0 := devSpec{
		bdf:      "0000:00:02.0",
		name:     "card0",
		vendorID: "8086",
		deviceID: "56a0",
		major:    devNullMajor,
		minor:    devNullMinor,
	}
	fx.addDRMDevice(t, fakeSysfsRoot, card0)
	fx.mknodDevice(t, fakeDevDRI, card0)
	tc.k8sClient.waitForContainerRestarts(t, tc.podName, xpumdContainer, 1, 30*time.Second)

	// Wait for the new container to have settled a baseline before changing the device set
	tc.k8sClient.waitForContainerLog(t, tc.podName, xpumdContainer, msgWatching, 30*time.Second)

	// Second change, restart should be suppressed.
	card1 := devSpec{
		bdf:      "0000:00:03.0",
		name:     "card1",
		vendorID: "8086",
		deviceID: "56a0",
		major:    devZeroMajor,
		minor:    devZeroMinor,
	}
	fx.addDRMDevice(t, fakeSysfsRoot, card1)
	fx.mknodDevice(t, fakeDevDRI, card1)

	// Confirm that the change was suppressed, then wait a bit to catch an inadvertent restart.
	// RestartCount never decreases, so a single check is enough.
	tc.k8sClient.waitForContainerLog(t, tc.podName, xpumdContainer, msgRestartLimitReached, 30*time.Second)
	time.Sleep(2 * time.Second)
	pod, err := tc.k8sClient.getPod(context.Background(), tc.podName)
	if err != nil {
		t.Fatalf("failed to get pod %q: %v", tc.podName, err)
	}
	cs := containerStatus(pod, xpumdContainer)
	if cs == nil {
		t.Fatalf("xpumd container not found in pod %q", tc.podName)
	}
	if cs.RestartCount != 1 {
		t.Errorf("expected exactly 1 restart (the second change should have been suppressed by max_restarts), got %d", cs.RestartCount)
	}
}
