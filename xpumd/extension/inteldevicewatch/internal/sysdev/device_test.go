//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package sysdev

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestFingerprint(t *testing.T) {
	card0 := dev("card0", 226, 0, DevNodeStateOK)
	card1 := dev("card1", 226, 1, DevNodeStateOK)
	// The same device refused by the devices cgroup, and with its node gone
	card1Denied := dev("card1", 226, 1, DevNodeStateDenied)
	card1Missing := dev("card1", 226, 1, DevNodeStateMissing)

	// What only a probe tells apart is not part of the digest
	assert.Equal(t,
		Inventory{card0, card1}.Fingerprint(),
		Inventory{card0, card1Denied}.Fingerprint())

	// The node disappearing is a change, even though sysfs is unchanged
	assert.NotEqual(t,
		Inventory{card0, card1}.Fingerprint(),
		Inventory{card0, card1Missing}.Fingerprint())

	// A device without a node is still distinct from one that is not there
	assert.NotEqual(t,
		Inventory{card0}.Fingerprint(),
		Inventory{card0, card1Missing}.Fingerprint())
}

// TestFixable covers which states creating or removing the device node resolves.
func TestFixable(t *testing.T) {
	inv := Inventory{
		dev("card0", 226, 0, DevNodeStateOK),
		dev("card1", 226, 1, DevNodeStateMissing),
		dev("card2", 226, 2, DevNodeStateMismatch),
		dev("card3", 226, 3, DevNodeStateOrphan),
		dev("card4", 226, 4, DevNodeStateStale),
		dev("card5", 226, 5, DevNodeStateBlocked),
		dev("card6", 226, 6, DevNodeStateDenied),
		dev("card7", 226, 7, DevNodeStateError),
	}
	assert.Equal(t, []string{"drm/card1", "drm/card2", "drm/card3"}, inv.Fixable().IDs())
}

func TestInventoryDiff(t *testing.T) {
	card := func(name string, minor, slot uint32, driver string) Device {
		d := dev(name, 226, minor, DevNodeStateOK)
		d.Sysfs.PCIBDF = fmt.Sprintf("0000:00:%02x.0", slot)
		d.Sysfs.Driver = driver
		return d
	}
	from := Inventory{
		card("card0", 0, 0, "i915"),
		card("card1", 1, 1, "i915"),
	}

	// Ordering does not matter for the fingerprint
	assert.Equal(t, from.Fingerprint(), Inventory{from[1], from[0]}.Fingerprint())

	t.Run("add-and-remove", func(t *testing.T) {
		diff := from.Diff(Inventory{
			card("card1", 1, 1, "i915"),
			card("card2", 2, 2, "xe"),
		})
		assert.Equal(t, []string{"drm/card2"}, diff.Added)
		assert.Equal(t, []string{"drm/card0"}, diff.Removed)
		assert.Empty(t, diff.Changed)
	})

	t.Run("rebind-and-renumber", func(t *testing.T) {
		to := Inventory{
			card("card0", 0, 0, "xe"),   // driver changed
			card("card1", 5, 1, "i915"), // minor changed
		}
		assert.NotEqual(t, from.Fingerprint(), to.Fingerprint())
		diff := from.Diff(to)
		assert.Equal(t, []string{"drm/card0", "drm/card1"}, diff.Changed)
		assert.Empty(t, diff.Added)
		assert.Empty(t, diff.Removed)
	})

	t.Run("swap-in-place", func(t *testing.T) {
		diff := from.Diff(Inventory{
			card("card0", 3, 9, "xe"), // minor, slot, and driver changed
			card("card1", 1, 1, "i915"),
		})
		assert.Equal(t, []string{"drm/card0"}, diff.Changed)
		assert.Empty(t, diff.Added)
		assert.Empty(t, diff.Removed)
	})

	t.Run("recreated-in-place", func(t *testing.T) {
		// A device destroyed and re-created identical. Nothing but the generation number changes.
		recreated := card("card0", 0, 0, "i915")
		recreated.Sysfs.Generation = 2
		to := Inventory{recreated, from[1]}

		assert.NotEqual(t, from.Fingerprint(), to.Fingerprint())
		diff := from.Diff(to)
		assert.Equal(t, []string{"drm/card0"}, diff.Changed)
		assert.Empty(t, diff.Added)
		assert.Empty(t, diff.Removed)
	})

	t.Run("diff-with-self", func(t *testing.T) {
		diff := from.Diff(from)
		assert.Empty(t, diff.Added)
		assert.Empty(t, diff.Removed)
		assert.Empty(t, diff.Changed)
	})
}

func TestIsNode(t *testing.T) {
	for _, name := range []string{"card0", "card12", "renderD128"} {
		assert.True(t, SubsystemDRM.isNode(name), name)
	}
	for _, name := range []string{"card", "card0-DP-1", "controlD64", "renderD", "version", "by-path"} {
		assert.False(t, SubsystemDRM.isNode(name), name)
	}
	assert.True(t, SubsystemMEI.isNode("mei0"))
	assert.False(t, SubsystemMEI.isNode("mei"))
}

func TestParseVendorIDs(t *testing.T) {
	ids, err := ParseVendorIDs([]string{" 0x8086 ", "10DE"})
	require.NoError(t, err)
	assert.Equal(t, []string{"8086", "10de"}, ids)

	// A nil list stays nil, which is what accepts every vendor.
	ids, err = ParseVendorIDs(nil)
	require.NoError(t, err)
	assert.Nil(t, ids)

	// Anything that is not four hex digits can never match what sysfs reports, so
	// it is refused rather than left to silently filter every device out.
	for _, id := range []string{"", "0x", "808", "80866", "808g", "8086:0bd5"} {
		_, err := ParseVendorIDs([]string{"8086", id})
		assert.Error(t, err, "%q", id)
	}
}
