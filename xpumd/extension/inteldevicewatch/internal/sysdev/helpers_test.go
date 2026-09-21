//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysdev

import (
	"os"
	"path/filepath"
	"testing"

	"github.com/stretchr/testify/require"
)

// mockFS builds a mocked sysfs and a device directory tree
type mockFS struct {
	t     *testing.T
	root  string
	sysfs string
	dev   string

	// devNodes records the fake device devNodes, keyed by path relative to dev
	devNodes map[string]DevNum
}

func newMockFS(t *testing.T) *mockFS {
	t.Helper()
	root := t.TempDir()
	f := &mockFS{
		t:        t,
		root:     root,
		sysfs:    filepath.Join(root, "sys"),
		dev:      filepath.Join(root, "dev"),
		devNodes: map[string]DevNum{},
	}
	require.NoError(t, os.MkdirAll(filepath.Join(f.sysfs, "class"), 0o755))
	require.NoError(t, os.MkdirAll(f.dev, 0o755))
	return f
}

// pciDevice creates a PCI device directory with the given identity.
func (f *mockFS) pciDevice(bdf, vendor, driver string) string {
	f.t.Helper()
	dir := filepath.Join(f.sysfs, "devices", "pci0000:00", bdf)
	require.NoError(f.t, os.MkdirAll(dir, 0o755))
	f.writeFile(filepath.Join(dir, "vendor"), vendor+"\n")
	if driver != "" {
		driverDir := filepath.Join(f.sysfs, "bus", "pci", "drivers", driver)
		require.NoError(f.t, os.MkdirAll(driverDir, 0o755))
		require.NoError(f.t, os.Symlink(driverDir, filepath.Join(dir, "driver")))
	}
	return dir
}

// classDevice adds a device of a class below a PCI device, and links it into
// <sysfs>/class the way the kernel does.
func (f *mockFS) classDevice(pciDir, class, name string, major, minor uint32) {
	f.t.Helper()
	devDir := filepath.Join(pciDir, class, name)
	require.NoError(f.t, os.MkdirAll(devDir, 0o755))
	f.writeFile(filepath.Join(devDir, "dev"), devNumString(major, minor))
	// Skip the "device" backlink (like ../../../0000:00:02.0) to the PCI directory (nothing reads it)
	classDir := filepath.Join(f.sysfs, "class", class)
	require.NoError(f.t, os.MkdirAll(classDir, 0o755))
	require.NoError(f.t, os.Symlink(devDir, filepath.Join(classDir, name)))
}

// classEntryRaw adds a bare entry under <sysfs>/class with no PCI ancestor.
func (f *mockFS) classEntryRaw(class, name string, major, minor uint32) {
	f.t.Helper()
	dir := filepath.Join(f.sysfs, "class", class, name)
	require.NoError(f.t, os.MkdirAll(dir, 0o755))
	f.writeFile(filepath.Join(dir, "dev"), devNumString(major, minor))
}

// recreate imitates the kernel removing and recreating the device over a driver unbind/bind.
func (f *mockFS) recreate(class, name string) {
	f.t.Helper()
	recreateDir(f.t, filepath.Join(f.sysfs, "class", class, name))
}

// recreateDir replaces the directory path resolves to with an identical new one,
// whose inode is guaranteed to differ from the one it replaces.
func recreateDir(t *testing.T, path string) {
	t.Helper()
	dir, err := filepath.EvalSymlinks(path)
	require.NoError(t, err)
	replacement := dir + ".new"
	require.NoError(t, os.CopyFS(replacement, os.DirFS(dir)))
	require.NoError(t, os.RemoveAll(dir))
	require.NoError(t, os.Rename(replacement, dir))
}

// node registers a fake device node under <dev>/<subdir>/<name>.
func (f *mockFS) node(subdir, name string, major, minor uint32) {
	f.t.Helper()
	f.plainFile(subdir, name)
	f.devNodes[filepath.Join(subdir, name)] = DevNum{Major: major, Minor: minor}
}

// plainFile creates an entry that is not a device node under <dev>/<subdir>/<name>.
func (f *mockFS) plainFile(subdir, name string) {
	f.t.Helper()
	dir := filepath.Join(f.dev, subdir)
	require.NoError(f.t, os.MkdirAll(dir, 0o755))
	f.writeFile(filepath.Join(dir, name), "")
}

func (f *mockFS) writeFile(path, content string) {
	f.t.Helper()
	require.NoError(f.t, os.WriteFile(path, []byte(content), 0o644))
}

// scanner returns a scanner operating on the mockFS.
func (f *mockFS) scanner(subs []Subsystem, probe func(string) error) *Scanner {
	f.t.Helper()
	if probe == nil {
		probe = func(string) error { return nil }
	}
	return &Scanner{
		SysfsRoot:  f.sysfs,
		DevRoot:    f.dev,
		Subsystems: subs,
		VendorIDs:  []string{VendorIDIntel},
		Probe:      probe,
		StatNode:   f.statNode,
	}
}

func (f *mockFS) statNode(path string) (NodeIdentity, error) {
	rel, err := filepath.Rel(f.dev, path)
	if err != nil {
		return NodeIdentity{}, err
	}
	if num, ok := f.devNodes[rel]; ok {
		return NodeIdentity{Present: true, CharDev: true, DevNum: num}, nil
	}
	return lstatDevNode(path)
}

func devNumString(major, minor uint32) string {
	return DevNum{Major: major, Minor: minor}.String() + "\n"
}

// dev builds a device the way a scan reports one.
func dev(name string, major, minor uint32, state DevNodeState) Device {
	num := DevNum{Major: major, Minor: minor}
	d := Device{
		Subsystem: "drm",
		Name:      name,
		State:     state,
	}
	if state != DevNodeStateOrphan {
		d.Sysfs.DevNum = num
	}
	switch state {
	case DevNodeStateOrphan, DevNodeStateStale, DevNodeStateDenied, DevNodeStateOK, DevNodeStateError:
		d.Node = NodeIdentity{Present: true, CharDev: true, DevNum: num}
	case DevNodeStateBlocked:
		d.Node = NodeIdentity{Present: true}
	}
	return d
}
