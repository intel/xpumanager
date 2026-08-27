//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysdev

import (
	"errors"
	"fmt"
	"hash/fnv"
	"io"
	"maps"
	"regexp"
	"slices"
	"strconv"
	"strings"
)

var (
	// ErrUnsupportedSubsystem is returned when a subsystem name is not recognized.
	ErrUnsupportedSubsystem = errors.New("unsupported device subsystem")
	// ErrInvalidVendorID is returned when a PCI vendor ID is not a four-digit hex number.
	ErrInvalidVendorID = errors.New("invalid PCI vendor ID")
)

// DevNodeState describes how a device relates to its device node.
type DevNodeState string

const (
	// DevNodeStateOK means the node exists with the expected device number and can be opened read-write.
	DevNodeStateOK DevNodeState = "ok"

	// DevNodeStateMissing means the device exists in sysfs but has no device node.
	// Fixable by creating the node.
	DevNodeStateMissing DevNodeState = "missing"

	// DevNodeStateMismatch means devfs/sysfs mismatch where the node refers to a
	// different device number than sysfs says it should, e.g. after a driver
	// rebind renumbered the minor.
	// Fixable by removing and recreating the node.
	DevNodeStateMismatch DevNodeState = "mismatch"

	// DevNodeStateStale means the node refers to the device number sysfs reports
	// but nothing answers behind it, i.e. open fails with ENODEV/ENXIO.
	// Not fixable: recreating an identical node cannot help.
	DevNodeStateStale DevNodeState = "stale"

	// DevNodeStateOrphan means devfs/sysfs mismatch where the node name isn't in sysfs at all,
	// I.e. no device for it and nothing to recreate.
	// Fixable by removing the node.
	DevNodeStateOrphan DevNodeState = "orphan"

	// DevNodeStateBlocked means the node's name is taken by an entry that is not a
	// character device, e.g. a regular file, so the device has no node and cannot get one.
	// Not fixable: the entry is not ours to delete (principle of caution).
	DevNodeStateBlocked DevNodeState = "blocked"

	// DevNodeStateDenied means the device node exists and refers to the right device, but opening it read-write fails.
	// This cannot be fixed from inside the container (likely denied because of the devices cgroup).
	DevNodeStateDenied DevNodeState = "denied"

	// DevNodeStateError means the device node exists but the access failed for an unexpected reason.
	// Deliberately distinct from DevNodeStateStale: an unrecognized error is no
	// evidence of what is wrong with the device, or that anything is.
	DevNodeStateError DevNodeState = "error"
)

// Fixable reports whether creating or removing the device node resolves the state.
func (s DevNodeState) Fixable() bool {
	return s == DevNodeStateMissing || s == DevNodeStateMismatch || s == DevNodeStateOrphan
}

// Subsystem identifies a sysfs device class holding device nodes.
type Subsystem string

const (
	// SubsystemDRM is the DRM subsystem: /sys/class/drm, device nodes under /dev/dri.
	SubsystemDRM Subsystem = "drm"
	// SubsystemMEI is the MEI subsystem: /sys/class/mei, device nodes directly under /dev.
	SubsystemMEI Subsystem = "mei"
)

// subsystemInfo contains fixed per-subsystem metadata
var subsystemInfo = map[Subsystem]struct {
	// devSubdir is the directory under <dev> holding this subsystem's nodes.
	devSubdir string
	// nodeRe matches the names of nodes of this subsystem that we manage.
	nodeRe *regexp.Regexp
}{
	SubsystemDRM: {devSubdir: "dri", nodeRe: regexp.MustCompile(`^(card|renderD)[0-9]+$`)},
	SubsystemMEI: {devSubdir: "", nodeRe: regexp.MustCompile(`^mei[0-9]+$`)},
}

// DevSubdir returns the directory under <dev> holding this subsystem's nodes.
func (s Subsystem) DevSubdir() string {
	return subsystemInfo[s].devSubdir
}

// isNode reports whether name is a node of this subsystem that we manage.
func (s Subsystem) isNode(name string) bool {
	info, ok := subsystemInfo[s]
	return ok && info.nodeRe.MatchString(name)
}

// ParseSubsystems parses a list of subsystem names, e.g. from configuration.
func ParseSubsystems(names []string) ([]Subsystem, error) {
	subs := make([]Subsystem, 0, len(names))
	for _, name := range names {
		sub := Subsystem(name)
		if _, ok := subsystemInfo[sub]; !ok {
			return nil, fmt.Errorf("%w %q (supported: %s)", ErrUnsupportedSubsystem, name, slices.Sorted(maps.Keys(subsystemInfo)))
		}
		if slices.Contains(subs, sub) {
			continue
		}
		subs = append(subs, sub)
	}
	return subs, nil
}

// DevNum is a device number.
type DevNum struct {
	Major uint32
	Minor uint32
}

// String renders the device number the way sysfs writes it, "major:minor".
func (n DevNum) String() string {
	return strconv.FormatUint(uint64(n.Major), 10) + ":" + strconv.FormatUint(uint64(n.Minor), 10)
}

// SysfsIdentity is what sysfs says a device is.
type SysfsIdentity struct {
	// DevNum is the device number the node for this device should have.
	DevNum DevNum

	// PCIBDF, VendorID and Driver describe the nearest PCI ancestor.
	// Empty when there is none or when sysfs is unreadable.
	// NOTE: VendorID is not merely descriptive, it is what the vendor filter
	// matches on, so a device whose vendor cannot be read counts as none of ours.
	PCIBDF   string
	VendorID string
	Driver   string
}

// NodeIdentity is what the device directory actually holds for a device.
type NodeIdentity struct {
	// Present reports whether the device directory holds an entry of this name.
	Present bool
	// CharDev reports whether that entry is a character device node.
	CharDev bool
	// DevNum is the device number the device node refers to. Only a character device has one.
	DevNum DevNum
}

// String renders the node's device number, "none" when there is no node, or what
// the entry is instead when it is not a device node.
func (n NodeIdentity) String() string {
	switch {
	case !n.Present:
		return "none"
	case !n.CharDev:
		return "not a character device"
	}
	return n.DevNum.String()
}

// Device is one device node, as described by sysfs and as observed in the device directory.
type Device struct {
	// Subsystem is the sysfs class, e.g. "drm".
	Subsystem string
	// Name is the name of the device node, e.g. "card1" or "renderD128".
	Name string

	// Sysfs is the device as sysfs describes it.
	Sysfs SysfsIdentity
	// Node is the device node as the device directory holds it.
	Node NodeIdentity

	// State holds the result of comparing the two.
	State DevNodeState
	// Err is the underlying error when State is DevNodeStateDenied,
	// DevNodeStateMismatch, DevNodeStateStale, DevNodeStateBlocked or DevNodeStateError.
	Err error
}

// NodeMismatch reports whether a node exists but refers to a different device
// number than sysfs says it should.
func (d Device) NodeMismatch() bool {
	return d.Node.Present && d.Node.DevNum != d.Sysfs.DevNum
}

// DevNum returns the device number of the device. The one sysfs reports, or the
// one the device node itself refers to when sysfs has no entry (i.e. "orphan" node).
func (d Device) DevNum() DevNum {
	if d.Sysfs.DevNum == (DevNum{}) {
		return d.Node.DevNum
	}
	return d.Sysfs.DevNum
}

// ID returns a stable identifier for the device node.
func (d Device) ID() string { return d.Subsystem + "/" + d.Name }

// identity uniquely identifies a device across scans, its device node included.
// What only a probe tells apart is left out: an unopenable node is the same node.
type identity struct {
	id     string
	devNum DevNum
	pciBDF string
	driver string
	node   NodeIdentity
}

func (d Device) identity() identity {
	return identity{
		id:     d.ID(),
		devNum: d.DevNum(),
		pciBDF: d.Sysfs.PCIBDF,
		driver: d.Sysfs.Driver,
		node:   d.Node,
	}
}

// writeTo is a helper for inventory hashing.
func (i identity) writeTo(w io.Writer) {
	// Ignored deliberately: the writer is a hash and cannot fail
	_, _ = fmt.Fprintln(w, i.id, i.devNum, i.pciBDF, i.driver, i.node)
}

// String implements fmt.Stringer, for log messages.
func (d Device) String() string {
	s := d.ID() + " (" + d.DevNum().String()
	if d.Sysfs.PCIBDF != "" {
		s += " " + d.Sysfs.PCIBDF
	}
	if d.Sysfs.Driver != "" {
		s += " " + d.Sysfs.Driver
	}
	return s + ")"
}

// Inventory is a set of devices, e.g. the result of one scan, sorted by device ID
// for stable output.
type Inventory []Device

// ByState returns a sub-set of devices in the given states.
func (inv Inventory) ByState(states ...DevNodeState) Inventory {
	var out Inventory
	for _, d := range inv {
		if slices.Contains(states, d.State) {
			out = append(out, d)
		}
	}
	return out
}

// ByID indexes the devices by their ID.
func (inv Inventory) ByID() map[string]Device {
	out := make(map[string]Device, len(inv))
	for _, d := range inv {
		out[d.ID()] = d
	}
	return out
}

// Fixable returns the devices whose state creating or removing the device node resolves.
func (inv Inventory) Fixable() Inventory {
	var out Inventory
	for _, d := range inv {
		if d.State.Fixable() {
			out = append(out, d)
		}
	}
	return out
}

// Counts returns the number of devices per state.
func (inv Inventory) Counts() map[DevNodeState]int {
	counts := map[DevNodeState]int{}
	for _, d := range inv {
		counts[d.State]++
	}
	return counts
}

// IDs names the devices.
func (inv Inventory) IDs() []string {
	out := make([]string, 0, len(inv))
	for _, d := range inv {
		out = append(out, d.ID())
	}
	return out
}

// Fingerprint is a short digest of the inventory. Changes whenever a device is
// added, removed, or changed.
func (inv Inventory) Fingerprint() string {
	// Sorted so that the digest describes the set rather than the order of discovery.
	devs := slices.SortedFunc(slices.Values(inv), func(a, b Device) int {
		return strings.Compare(a.ID(), b.ID())
	})

	// FNV rather than a cryptographic hash. Nothing trusts this digest.
	h := fnv.New64a()
	for _, d := range devs {
		d.identity().writeTo(h)
	}
	return fmt.Sprintf("%016x", h.Sum64())
}

// InventoryDiff is how one set of devices differs from another, by device ID.
type InventoryDiff struct {
	Added   []string
	Removed []string
	Changed []string
}

// Diff reports how another set of devices differs from this one.
func (inv Inventory) Diff(other Inventory) InventoryDiff {
	before, after := inv.identities(), other.identities()

	var diff InventoryDiff
	for _, d := range other {
		switch was, existed := before[d.ID()]; {
		case !existed:
			diff.Added = append(diff.Added, d.ID())
		case was != d.identity():
			diff.Changed = append(diff.Changed, d.ID())
		}
	}
	for _, d := range inv {
		if _, kept := after[d.ID()]; !kept {
			diff.Removed = append(diff.Removed, d.ID())
		}
	}
	return diff
}

// identities indexes the set by device ID, for comparing it against another.
func (inv Inventory) identities() map[string]identity {
	out := make(map[string]identity, len(inv))
	for _, d := range inv {
		out[d.ID()] = d.identity()
	}
	return out
}

// VendorIDIntel is the PCI vendor ID of Intel devices, as sysfs spells it once normalized.
const VendorIDIntel = "8086"

var vendorIDRe = regexp.MustCompile(`^[0-9a-f]{4}$`)

// ParseVendorIDs parses a list of PCI vendor IDs, e.g. from configuration, into sysfs format (lower case, no "0x" prefix).
func ParseVendorIDs(ids []string) ([]string, error) {
	if ids == nil {
		return nil, nil
	}
	out := make([]string, 0, len(ids))
	for _, id := range ids {
		normalized := strings.TrimPrefix(strings.ToLower(strings.TrimSpace(id)), "0x")
		if !vendorIDRe.MatchString(normalized) {
			return nil, fmt.Errorf("%w %q: must be four hex digits, optionally 0x-prefixed", ErrInvalidVendorID, id)
		}
		out = append(out, normalized)
	}
	return out, nil
}
