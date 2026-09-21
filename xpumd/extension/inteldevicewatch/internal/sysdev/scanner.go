//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysdev

import (
	"errors"
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"slices"
	"strconv"
	"strings"

	"golang.org/x/sys/unix"
)

// Scanner scans sysfs and a device directory.
type Scanner struct {
	// SysfsRoot is the sysfs mount point.
	SysfsRoot string
	// DevRoot is the device directory root, device nodes live under DevRoot/<subsystem-dev-dir>.
	DevRoot string
	// Subsystems to scan.
	Subsystems []Subsystem
	// VendorIDs restricts the scan to these PCI vendor IDs. Empty accepts any vendor.
	VendorIDs []string
	// Probe is a function to check whether a device node is usable.
	Probe func(path string) error

	// StatNode is a function to read what a device directory entry is.
	// Overridable for testing purposes.
	StatNode func(path string) (NodeIdentity, error)
}

func (s *Scanner) probe(path string) error {
	if s.Probe == nil {
		return ProbeOpen(path)
	}
	return s.Probe(path)
}

func (s *Scanner) statNode(path string) (NodeIdentity, error) {
	if s.StatNode == nil {
		return lstatDevNode(path)
	}
	return s.StatNode(path)
}

// DevDir returns the directory holding the nodes of a subsystem.
func (s *Scanner) DevDir(sub Subsystem) string {
	return filepath.Join(s.DevRoot, sub.DevSubdir())
}

// classDir returns the sysfs class directory of a subsystem.
func (s *Scanner) classDir(sub Subsystem) string {
	return filepath.Join(s.SysfsRoot, "class", string(sub))
}

// ProbeOpen checks if the file is openable read-write (opens the file and closes it immediately).
// NOTE: the only reliable way found to check whether e.g. the container's devices cgroup denies access.
func ProbeOpen(path string) error {
	fd, err := unix.Open(path, unix.O_RDWR|unix.O_NONBLOCK|unix.O_CLOEXEC, 0)
	if err != nil {
		return err
	}
	return unix.Close(fd)
}

// Scan compares sysfs against the device directory.
// The returned inventory holds whatever could be determined despite errors.
// Errors from scanning each subsystem are joined into a single error.
func (s *Scanner) Scan() (Inventory, error) {
	var errs error
	var inv Inventory
	for _, sub := range s.Subsystems {
		sysDevs, err := s.scanSysfs(sub)
		if err != nil {
			errs = errors.Join(errs, err)
			continue
		}
		nodes, err := s.scanDevDir(sub)
		if err != nil {
			errs = errors.Join(errs, err)
			continue
		}
		inv = append(inv, s.reconcile(sub, sysDevs, nodes)...)
	}

	slices.SortFunc(inv, func(a, b Device) int { return strings.Compare(a.ID(), b.ID()) })
	return inv, errs
}

// scanSysfs lists the devices of a subsystem as sysfs describes them.
func (s *Scanner) scanSysfs(sub Subsystem) ([]Device, error) {
	classDir := s.classDir(sub)
	entries, err := os.ReadDir(classDir)
	if err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			// The class is absent
			return nil, nil
		}
		return nil, fmt.Errorf("reading %s: %w", classDir, err)
	}

	var devs []Device
	for _, entry := range entries {
		name := entry.Name()
		if !sub.isNode(name) {
			continue
		}
		id, err := s.readSysfsIdentity(sub, name)
		if err != nil {
			if errors.Is(err, fs.ErrNotExist) {
				// The device is on its way out, gone between listing the class
				// and reading it, so it is simply not part of this listing.
				continue
			}
			// Anything else is no evidence that the device is gone, and taking it
			// for one would settle as a device set without it.
			// NOTE: we assume there will be no single-device persistent errors, this would be an error
			// resolved in the next scan (or then a persistent configuration/permission error that applies to all devices).
			return nil, fmt.Errorf("reading identity for %s/%s: %w", sub, name, err)
		}
		devs = append(devs, Device{
			Subsystem: string(sub),
			Name:      name,
			Sysfs:     id,
		})
	}
	return devs, nil
}

func (s *Scanner) vendorAccepted(vendorID string) bool {
	if len(s.VendorIDs) == 0 {
		return true
	}
	return slices.Contains(s.VendorIDs, vendorID)
}

// scanDevDir lists the entries of a subsystem present in the device directory.
// NOTE: A missing directory is not an error but every device is then simply missing.
func (s *Scanner) scanDevDir(sub Subsystem) (map[string]NodeIdentity, error) {
	dir := s.DevDir(sub)
	entries, err := os.ReadDir(dir)
	if err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			return map[string]NodeIdentity{}, nil
		}
		return nil, fmt.Errorf("reading %s: %w", dir, err)
	}

	nodes := make(map[string]NodeIdentity, len(entries))
	for _, entry := range entries {
		name := entry.Name()
		if !sub.isNode(name) {
			continue
		}
		path := filepath.Join(dir, name)
		node, err := s.statNode(path)
		if err != nil {
			if errors.Is(err, fs.ErrNotExist) {
				// Unlinked between listing the directory and reading the entry.
				continue
			}
			// An entry we cannot read is not an entry that is not there.
			// NOTE: we assume there will be no single-device persistent errors, this would be an error
			// resolved in the next scan (or then a persistent configuration/permission error that applies to all devices).
			return nil, fmt.Errorf("reading %s: %w", path, err)
		}
		nodes[name] = node
	}
	return nodes, nil
}

// lstatDevNode reads what an entry is, and the device number of a character
// device. Symlinks are not followed.
func lstatDevNode(path string) (NodeIdentity, error) {
	var stat unix.Stat_t
	if err := unix.Lstat(path, &stat); err != nil {
		return NodeIdentity{}, &os.PathError{Op: "lstat", Path: path, Err: err}
	}
	if stat.Mode&unix.S_IFMT != unix.S_IFCHR {
		return NodeIdentity{Present: true}, nil
	}
	return NodeIdentity{
		Present: true,
		CharDev: true,
		DevNum:  DevNum{Major: unix.Major(uint64(stat.Rdev)), Minor: unix.Minor(uint64(stat.Rdev))},
	}, nil
}

// reconcile classifies the sysfs devices this scanner manages against the
// observed nodes, and reports nodes with no device behind them as orphans.
func (s *Scanner) reconcile(sub Subsystem, sysDevs []Device, nodes map[string]NodeIdentity) []Device {
	dir := s.DevDir(sub)
	out := make([]Device, 0, len(sysDevs))

	// First pass: classify the node of every device sysfs listed and manage.
	claimed := make(map[string]bool, len(sysDevs))
	for _, dev := range sysDevs {
		claimed[dev.Name] = true
		if !s.vendorAccepted(dev.Sysfs.VendorID) {
			continue
		}
		dev.Node = nodes[dev.Name]
		switch {
		case !dev.Node.Present:
			dev.State = DevNodeStateMissing
		case !dev.Node.CharDev:
			// Something else than character device holds the name.
			dev.State = DevNodeStateBlocked
			dev.Err = errors.New("node is not a character device")
		case dev.NodeMismatch():
			// The node refers to a different device than sysfs says it should,
			// e.g. the minor was renumbered by a driver rebind.
			dev.State = DevNodeStateMismatch
			dev.Err = fmt.Errorf("node is %s, sysfs says %s", dev.Node, dev.Sysfs.DevNum)
		default:
			dev.State, dev.Err = s.classifyProbe(filepath.Join(dir, dev.Name))
			if dev.State == DevNodeStateMissing {
				// The node is gone, so there is no node left to describe.
				dev.Node = NodeIdentity{}
			}
		}
		out = append(out, dev)
	}

	// Second pass: the device nodes no device claimed. The scan is the source of truth, so
	// a node no listed device claims has nothing behind it and is reported an orphan.
	// NOTE: the comparison is racy, being snapshots of two directories read one
	// after the other. A device (dis)appearing mid-scan is corrected by the next scan.
	// NOTE: orphans are not vendor filtered, the vendor is unknown once sysfs no longer lists the device.
	for name, node := range nodes {
		// Don't report non-character devices as orphans device nodes (we refuse the touch those).
		if claimed[name] || !node.CharDev {
			continue
		}
		out = append(out, Device{
			Subsystem: string(sub),
			Name:      name,
			Node:      node,
			State:     DevNodeStateOrphan,
		})
	}
	return out
}

// classifyProbe probes a device node and maps the error to a DevNodeState.
// NOTE: DevNodeStateMissing is a possible result, the node having been unlinked after it was scanned.
func (s *Scanner) classifyProbe(path string) (DevNodeState, error) {
	err := s.probe(path)
	switch {
	case err == nil:
		return DevNodeStateOK, nil
	case errors.Is(err, unix.EPERM), errors.Is(err, unix.EACCES):
		return DevNodeStateDenied, err
	case errors.Is(err, unix.ENODEV), errors.Is(err, unix.ENXIO):
		return DevNodeStateStale, err
	case errors.Is(err, unix.ENOENT):
		// The device node was unlinked after scan (but sysfs snapshot still lists it)
		return DevNodeStateMissing, nil
	default:
		return DevNodeStateError, err
	}
}

// readSysfsIdentity reads what sysfs says about a device (subsystem class entry).
func (s *Scanner) readSysfsIdentity(sub Subsystem, name string) (SysfsIdentity, error) {
	num, err := s.readSysfsDevNum(sub, name)
	if err != nil {
		return SysfsIdentity{}, err
	}
	classEntry := filepath.Join(s.classDir(sub), name)
	id, err := s.readSysfsPCIIdentity(classEntry)
	if err != nil {
		return SysfsIdentity{}, err
	}
	id.DevNum = num

	// Get the inode of the sysfs directory, which is used as a generation number for the device
	var stat unix.Stat_t
	if err := unix.Stat(classEntry, &stat); err != nil {
		return SysfsIdentity{}, &os.PathError{Op: "stat", Path: classEntry, Err: err}
	}
	id.Generation = stat.Ino

	return id, nil
}

// readSysfsDevNum reads the device number sysfs reports.
func (s *Scanner) readSysfsDevNum(sub Subsystem, name string) (DevNum, error) {
	path := filepath.Join(s.classDir(sub), name, "dev")
	data, err := os.ReadFile(path)
	if err != nil {
		return DevNum{}, err
	}
	major, minor, ok := strings.Cut(strings.TrimSpace(string(data)), ":")
	if !ok {
		return DevNum{}, fmt.Errorf("malformed device number %q in %s", data, path)
	}
	majorNum, err := strconv.ParseUint(major, 10, 32)
	if err != nil {
		return DevNum{}, fmt.Errorf("malformed major number in %s: %w", path, err)
	}
	minorNum, err := strconv.ParseUint(minor, 10, 32)
	if err != nil {
		return DevNum{}, fmt.Errorf("malformed minor number in %s: %w", path, err)
	}
	return DevNum{Major: uint32(majorNum), Minor: uint32(minorNum)}, nil
}

// readSysfsPCIIdentity reads the identity of the nearest PCI ancestor of a
// device (subsystem class entry) in sysfs. A device with no PCI ancestor at all
// gets the zero identity.
func (s *Scanner) readSysfsPCIIdentity(classEntry string) (SysfsIdentity, error) {
	dir, err := s.findSysfsPCIDir(classEntry)
	if err != nil || dir == "" {
		return SysfsIdentity{}, err
	}
	vendorID, err := readHexID(filepath.Join(dir, "vendor"))
	if err != nil {
		return SysfsIdentity{}, err
	}
	driver, err := readLinkBase(filepath.Join(dir, "driver"))
	if err != nil {
		return SysfsIdentity{}, err
	}
	return SysfsIdentity{PCIBDF: filepath.Base(dir), VendorID: vendorID, Driver: driver}, nil
}

// findSysfsPCIDir locates the nearest PCI ancestor of a sysfs class entry, or ""
// if the device has none. A class entry is a symlink into the device tree, so
// resolving it and walking up reaches the PCI device the entry hangs off.
func (s *Scanner) findSysfsPCIDir(classEntry string) (string, error) {
	dir, err := filepath.EvalSymlinks(classEntry)
	if err != nil {
		return "", err
	}
	// Stay inside the sysfs devices tree.
	devicesRoot, err := filepath.EvalSymlinks(filepath.Join(s.SysfsRoot, "devices"))
	if err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			// No device tree to walk up at all, so no ancestor to find.
			return "", nil
		}
		return "", err
	}
	for strings.HasPrefix(dir, devicesRoot+string(filepath.Separator)) {
		if isSysfsPCIDir(dir) {
			return dir, nil
		}
		dir = filepath.Dir(dir)
	}
	return "", nil
}

// pciBDFRe matches a PCI address as used for sysfs directory names.
var pciBDFRe = regexp.MustCompile(`^[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\.[0-9a-f]+$`)

// isSysfsPCIDir reports whether a sysfs directory is a PCI device directory.
func isSysfsPCIDir(dir string) bool {
	return pciBDFRe.MatchString(filepath.Base(dir)) && exists(filepath.Join(dir, "vendor"))
}

// readHexID is a helper for reading a sysfs PCI ID file and normalizes
// "0x8086\n" to "8086", the form used for the pci.vendor_id attribute.
// NOTE: the value is not validated, only normalized.
func readHexID(path string) (string, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return "", err
	}
	return strings.TrimPrefix(strings.ToLower(strings.TrimSpace(string(data))), "0x"), nil
}

// readLinkBase returns the last element of a symlink's target.
func readLinkBase(path string) (string, error) {
	target, err := os.Readlink(path)
	if err != nil {
		if errors.Is(err, fs.ErrNotExist) {
			return "", nil
		}
		return "", err
	}
	return filepath.Base(target), nil
}

// exists reports whether a file exists.
func exists(path string) bool {
	_, err := os.Lstat(path)
	return err == nil
}
