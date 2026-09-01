//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package k8s

import (
	"testing"
)

// sysfsHelper builds and mutates a fake sysfs/device-node tree inside a
// helper container
type sysfsHelper struct {
	tc        testConfig
	container string
}

// newSysfsHelper returns a new helper. Mutates the tree through the named
// helper container (expected to be busybox with CAP_MKNOD).
func newSysfsHelper(tc testConfig, container string) sysfsHelper {
	return sysfsHelper{tc: tc, container: container}
}

// devSpec describes one fake device.
type devSpec struct {
	bdf      string
	name     string
	vendorID string
	deviceID string
	major    uint32
	minor    uint32
	// mode is the permissions of the device node, "0666" when empty.
	mode string
}

// nodeMode is the permissions to create the device node with.
func (d devSpec) nodeMode() string {
	if d.mode == "" {
		return "0666"
	}
	return d.mode
}

// addDRMDevice creates a fake DRM device under the (fake) sysfs root.
func (f sysfsHelper) addDRMDevice(t *testing.T, sysfsRoot string, dev devSpec) {
	t.Helper()

	pciDir := sysfsRoot + "/devices/pci0000:00/" + dev.bdf
	devDir := pciDir + "/drm/" + dev.name
	classDir := sysfsRoot + "/class/drm"
	classLink := classDir + "/" + dev.name

	var b scriptBuilder
	b.addf("mkdir -p %q", devDir)
	b.addf("echo %q > %q", "0x"+dev.vendorID, pciDir+"/vendor")
	b.addf("echo %q > %q", "0x"+dev.deviceID, pciDir+"/device")
	b.addf("echo %d:%d > %q", dev.major, dev.minor, devDir+"/dev")
	b.addf("mkdir -p %q", classDir)
	b.addf("ln -sfn %q %q", devDir, classLink)

	f.tc.k8sClient.execShell(t, f.tc.podName, f.container, b.String())
}

// removeDRMDevice removes a fake DRM device added by addDRMDevice.
func (f sysfsHelper) removeDRMDevice(t *testing.T, sysfsRoot string, dev devSpec) {
	t.Helper()

	pciDir := sysfsRoot + "/devices/pci0000:00/" + dev.bdf
	classLink := sysfsRoot + "/class/drm/" + dev.name

	var b scriptBuilder
	b.addf("rm -f %q", classLink)
	b.addf("rm -rf %q", pciDir)

	f.tc.k8sClient.execShell(t, f.tc.podName, f.container, b.String())
}

// mknodDevice creates a character device node.
func (f sysfsHelper) mknodDevice(t *testing.T, devDir string, dev devSpec) {
	t.Helper()

	path := devDir + "/" + dev.name

	var b scriptBuilder
	b.addf("mkdir -p %q", devDir)
	b.addf("mknod -m %s %q c %d %d", dev.nodeMode(), path, dev.major, dev.minor)

	f.tc.k8sClient.execShell(t, f.tc.podName, f.container, b.String())
}

// removeNode removes a device node (or any file) at the given path.
func (f sysfsHelper) removeNode(t *testing.T, devDir, name string) {
	t.Helper()

	var b scriptBuilder
	b.addf("rm -f %q", devDir+"/"+name)

	f.tc.k8sClient.execShell(t, f.tc.podName, f.container, b.String())
}
