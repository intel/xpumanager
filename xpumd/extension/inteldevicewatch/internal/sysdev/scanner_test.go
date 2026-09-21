//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysdev

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/sys/unix"
)

func TestScanClassifiesStates(t *testing.T) {
	f := newMockFS(t)
	probes := make(map[string]error)

	// card0/renderD128: ok
	gpu0 := f.pciDevice("0000:00:02.0", "0x8086", "i915")
	f.classDevice(gpu0, "drm", "card0", 226, 0)
	f.classDevice(gpu0, "drm", "renderD128", 226, 128)
	f.node("dri", "card0", 226, 0)
	f.node("dri", "renderD128", 226, 128)

	// card1: in sysfs, no device node
	gpu1 := f.pciDevice("0000:00:03.0", "0x8086", "xe")
	f.classDevice(gpu1, "drm", "card1", 226, 1)

	// card2: sysfs and device node disagree on minor
	gpu2 := f.pciDevice("0000:00:04.0", "0x8086", "xe")
	f.classDevice(gpu2, "drm", "card2", 226, 2)
	f.node("dri", "card2", 226, 99)

	// card3: device node matches sysfs, open is denied by the devices cgroup
	gpu3 := f.pciDevice("0000:00:05.0", "0x8086", "xe")
	f.classDevice(gpu3, "drm", "card3", 226, 3)
	f.node("dri", "card3", 226, 3)
	probes["card3"] = unix.EPERM

	// card4: device node matches sysfs, but nothing answers behind it.
	gpu4 := f.pciDevice("0000:00:06.0", "0x8086", "xe")
	f.classDevice(gpu4, "drm", "card4", 226, 4)
	f.node("dri", "card4", 226, 4)
	probes["card4"] = unix.ENODEV

	// card5: device node matches sysfs, open fails for an unexpected reason.
	gpu5 := f.pciDevice("0000:00:07.0", "0x8086", "xe")
	f.classDevice(gpu5, "drm", "card5", 226, 5)
	f.node("dri", "card5", 226, 5)
	probes["card5"] = unix.EIO

	// card6: device node was unlinked after it was listed, i.e. missing after all.
	gpu6 := f.pciDevice("0000:00:08.0", "0x8086", "xe")
	f.classDevice(gpu6, "drm", "card6", 226, 6)
	f.node("dri", "card6", 226, 6)
	probes["card6"] = unix.ENOENT

	// card7: the node's name is taken by something that is not a device node.
	gpu7 := f.pciDevice("0000:00:09.0", "0x8086", "xe")
	f.classDevice(gpu7, "drm", "card7", 226, 7)
	f.plainFile("dri", "card7")

	// card9: a node with nothing left in sysfs.
	f.node("dri", "card9", 226, 9)

	probe := func(path string) error {
		return probes[filepath.Base(path)]
	}

	inv, err := f.scanner([]Subsystem{SubsystemDRM}, probe).Scan()
	require.NoError(t, err)

	devs := inv.ByID()
	assert.Equal(t, DevNodeStateOK, devs["drm/card0"].State)
	assert.Equal(t, DevNodeStateOK, devs["drm/renderD128"].State)
	assert.Equal(t, DevNodeStateMissing, devs["drm/card1"].State)
	assert.Equal(t, DevNodeStateMismatch, devs["drm/card2"].State)
	assert.Equal(t, DevNodeStateDenied, devs["drm/card3"].State)
	assert.Equal(t, DevNodeStateStale, devs["drm/card4"].State)
	assert.Equal(t, DevNodeStateError, devs["drm/card5"].State)
	assert.Equal(t, DevNodeStateMissing, devs["drm/card6"].State)
	assert.Equal(t, DevNodeStateBlocked, devs["drm/card7"].State)
	assert.Equal(t, DevNodeStateOrphan, devs["drm/card9"].State)
	assert.Len(t, inv, 10)

	// A node that disappeared mid-scan is not described as present.
	assert.False(t, devs["drm/card6"].Node.Present)

	// The entry taking card7's name is reported as the non-node it is.
	assert.Equal(t, "not a character device", devs["drm/card7"].Node.String())
	assert.Error(t, devs["drm/card7"].Err)

	assert.Equal(t, map[DevNodeState]int{
		DevNodeStateOK:       2,
		DevNodeStateMissing:  2,
		DevNodeStateMismatch: 1,
		DevNodeStateStale:    1,
		DevNodeStateDenied:   1,
		DevNodeStateOrphan:   1,
		DevNodeStateBlocked:  1,
		DevNodeStateError:    1,
	}, inv.Counts())

	assert.Equal(t, []string{"drm/card0", "drm/renderD128"}, inv.ByState(DevNodeStateOK).IDs())
}

func TestScanReadsPCIIdentity(t *testing.T) {
	f := newMockFS(t)
	gpu0 := f.pciDevice("0000:4d:00.0", "0x8086", "i915")
	f.classDevice(gpu0, "drm", "card0", 226, 0)
	f.node("dri", "card0", 226, 0)

	gpu1 := f.pciDevice("0000:4d:00.1", "0x8086", "i915")
	f.classDevice(gpu1, "drm", "card1", 226, 1)
	f.node("dri", "card1", 226, 1)

	inv, err := f.scanner([]Subsystem{SubsystemDRM}, nil).Scan()
	require.NoError(t, err)

	devs := inv.ByID()
	card0 := devs["drm/card0"]
	assert.Equal(t, "0000:4d:00.0", card0.Sysfs.PCIBDF)
	assert.Equal(t, "8086", card0.Sysfs.VendorID)
	assert.Equal(t, "i915", card0.Sysfs.Driver)

	// card1 resolves to its own PCI ancestor, not to the first one found.
	assert.Equal(t, "226:1", devs["drm/card1"].DevNum().String())
	assert.Equal(t, "drm/card1 (226:1 0000:4d:00.1 i915)", devs["drm/card1"].String())
}

func TestScanTracksGeneration(t *testing.T) {
	f := newMockFS(t)
	gpu := f.pciDevice("0000:00:02.0", "0x8086", "i915")
	f.classDevice(gpu, "drm", "card0", 226, 0)
	f.node("dri", "card0", 226, 0)

	scanner := f.scanner([]Subsystem{SubsystemDRM}, nil)
	before, err := scanner.Scan()
	require.NoError(t, err)
	assert.NotZero(t, before.ByID()["drm/card0"].Sysfs.Generation)

	f.recreate("drm", "card0")
	after, err := scanner.Scan()
	require.NoError(t, err)
	assert.NotEqual(t, before.Fingerprint(), after.Fingerprint())
	assert.Equal(t, []string{"drm/card0"}, before.Diff(after).Changed)
}

func TestScanFiltersByVendor(t *testing.T) {
	f := newMockFS(t)
	intel := f.pciDevice("0000:00:02.0", "0x8086", "i915")
	f.classDevice(intel, "drm", "card0", 226, 0)
	f.node("dri", "card0", 226, 0)

	other := f.pciDevice("0000:65:00.0", "0x10de", "nvidia")
	f.classDevice(other, "drm", "card1", 226, 1)
	f.node("dri", "card1", 226, 1)

	inv, err := f.scanner([]Subsystem{SubsystemDRM}, nil).Scan()
	require.NoError(t, err)
	// card1 is from another vendor's, it's not "orphan" but simply ignored
	assert.Equal(t, []string{"drm/card0"}, inv.ByState(DevNodeStateOK).IDs())
	assert.Equal(t, []string{"drm/card0"}, inv.IDs())
	assert.Empty(t, inv.ByState(DevNodeStateOrphan))

	// An empty vendor list accepts everything, including devices with no PCI ancestor at all
	f.classEntryRaw("drm", "card2", 226, 2)
	f.node("dri", "card2", 226, 2)
	scanner := f.scanner([]Subsystem{SubsystemDRM}, nil)
	scanner.VendorIDs = nil
	inv, err = scanner.Scan()
	require.NoError(t, err)
	assert.Equal(t, []string{"drm/card0", "drm/card1", "drm/card2"}, inv.ByState(DevNodeStateOK).IDs())
}

func TestScanIgnoresNonDeviceEntries(t *testing.T) {
	f := newMockFS(t)
	gpu := f.pciDevice("0000:00:02.0", "0x8086", "i915")
	f.classDevice(gpu, "drm", "card0", 226, 0)
	f.node("dri", "card0", 226, 0)

	// Don't accept non-device entries
	require.NoError(t, os.MkdirAll(filepath.Join(f.sysfs, "class", "drm", "card0-DP-1"), 0o755))

	// An entry no device claims and that is not a device node either is none of
	// our business, not an orphan node to remove.
	f.plainFile("dri", "card9")

	inv, err := f.scanner([]Subsystem{SubsystemDRM}, nil).Scan()
	require.NoError(t, err)
	assert.Equal(t, []string{"drm/card0"}, inv.ByState(DevNodeStateOK).IDs())
	assert.Len(t, inv, 1)
}

func TestScanMissingTreesAreNotErrors(t *testing.T) {
	f := newMockFS(t)

	// No class or device directory exists (no DRM driver is loaded). Not a failure
	inv, err := f.scanner([]Subsystem{SubsystemDRM}, nil).Scan()
	require.NoError(t, err)
	assert.Empty(t, inv)

	// Class present, device directory absent (all device nodes are missing).
	gpu := f.pciDevice("0000:00:02.0", "0x8086", "i915")
	f.classDevice(gpu, "drm", "card0", 226, 0)
	inv, err = f.scanner([]Subsystem{SubsystemDRM}, nil).Scan()
	require.NoError(t, err)
	require.Len(t, inv, 1)
	assert.Equal(t, DevNodeStateMissing, inv[0].State)
	assert.False(t, inv[0].Node.Present)
}

func TestScanMultipleSubsystems(t *testing.T) {
	f := newMockFS(t)
	gpu := f.pciDevice("0000:4d:00.0", "0x8086", "i915")
	f.classDevice(gpu, "drm", "card0", 226, 0)
	f.node("dri", "card0", 226, 0)

	f.classDevice(gpu, "mei", "mei0", 511, 0)
	f.node("", "mei0", 511, 0)

	inv, err := f.scanner([]Subsystem{SubsystemDRM, SubsystemMEI}, nil).Scan()
	require.NoError(t, err)
	assert.Equal(t, []string{"drm/card0", "mei/mei0"}, inv.ByState(DevNodeStateOK).IDs())

	assert.Equal(t, "0000:4d:00.0", inv.ByID()["mei/mei0"].Sysfs.PCIBDF)
}

func TestScanMalformedDevFile(t *testing.T) {
	f := newMockFS(t)
	gpu := f.pciDevice("0000:00:02.0", "0x8086", "i915")
	f.classDevice(gpu, "drm", "card0", 226, 0)
	f.node("dri", "card0", 226, 0)
	f.classDevice(gpu, "drm", "card1", 226, 1)
	f.writeFile(filepath.Join(f.sysfs, "class", "drm", "card1", "dev"), "not-a-devnum\n")
	f.node("dri", "card1", 226, 1)

	// A device whose sysfs entry cannot be read causes an incomplete scan
	inv, err := f.scanner([]Subsystem{SubsystemDRM}, nil).Scan()
	require.Error(t, err)
	assert.Contains(t, err.Error(), "drm/card1")
	assert.Empty(t, inv)
}

func TestScanIgnoresVanishingDevice(t *testing.T) {
	f := newMockFS(t)
	gpu := f.pciDevice("0000:00:02.0", "0x8086", "i915")
	f.classDevice(gpu, "drm", "card0", 226, 0)
	f.node("dri", "card0", 226, 0)

	// A device on its way out, its "dev" attribute already removed while the class
	// entry is still listed. Not part of this scan.
	f.classDevice(gpu, "drm", "card1", 226, 1)
	require.NoError(t, os.Remove(filepath.Join(f.sysfs, "class", "drm", "card1", "dev")))

	inv, err := f.scanner([]Subsystem{SubsystemDRM}, nil).Scan()
	require.NoError(t, err)
	assert.Equal(t, []string{"drm/card0"}, inv.IDs())
}

func TestScanUnreadableDevNode(t *testing.T) {
	f := newMockFS(t)
	gpu := f.pciDevice("0000:00:02.0", "0x8086", "i915")
	f.classDevice(gpu, "drm", "card0", 226, 0)
	f.node("dri", "card0", 226, 0)

	// An entry we cannot read causes an incomplete scan, not a missing node.
	scanner := f.scanner([]Subsystem{SubsystemDRM}, nil)
	scanner.StatNode = func(string) (NodeIdentity, error) { return NodeIdentity{}, unix.EACCES }

	inv, err := scanner.Scan()
	require.Error(t, err)
	assert.Contains(t, err.Error(), "card0")
	assert.Empty(t, inv)
}
