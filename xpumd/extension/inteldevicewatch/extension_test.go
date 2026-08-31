//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: MIT

package inteldevicewatch

import (
	"fmt"
	"os"
	"path/filepath"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	"golang.org/x/sys/unix"

	"github.com/intel/xpumanager/xpumd/extension/inteldevicewatch/internal/sysdev"
)

// breakClassDir makes a sysfs class directory unreadable, which is what a device
// tree the scanner cannot list looks like. It returns a function that puts it
// back.
func breakClassDir(t *testing.T, tree *devTree, class string) func() {
	t.Helper()
	if os.Geteuid() == 0 {
		t.Skip("root ignores directory permissions")
	}
	classDir := filepath.Join(tree.sysfs, "class", class)
	require.NoError(t, os.MkdirAll(classDir, 0o755))
	require.NoError(t, os.Chmod(classDir, 0o000))
	restored := false
	t.Cleanup(func() {
		if !restored {
			require.NoError(t, os.Chmod(classDir, 0o755))
		}
	})
	return func() {
		require.NoError(t, os.Chmod(classDir, 0o755))
		restored = true
	}
}

// TestBaseline covers the set the process starts with, which is the closest thing
// to what zesInit enumerated and what every later scan is compared against.
func TestBaseline(t *testing.T) {
	t.Run("DevicesPresent", func(t *testing.T) {
		f := newWatchFixture(t, nil)
		f.tree.addCard("card0", 0)
		f.tree.addCard("renderD128", 128)
		f.start(t)

		require.NotNil(t, f.watch.baseline)
		assert.Equal(t, []string{"drm/card0", "drm/renderD128"}, f.watch.baseline.Devices.IDs())
		assert.NotEmpty(t, f.watch.baseline.Fingerprint)
		assert.Contains(t, f.messages(), msgWatching)

		// A stable device set produces nothing further, however many scans run.
		before := f.logs.Len()
		f.scan(7)
		assert.Equal(t, before, f.logs.Len())
		assert.Empty(t, f.host.fatalErrors())
	})

	t.Run("NoDevices", func(t *testing.T) {
		// An empty scan is still a baseline, so a device turning up later is a change.
		f := newWatchFixture(t, nil)
		f.start(t)
		require.NotNil(t, f.watch.baseline)
		assert.Empty(t, f.watch.baseline.Devices.IDs())

		f.tree.addCard("card0", 0)
		f.scan(2)
		assert.Contains(t, f.messages(), msgDeviceSetChanged)
	})

	t.Run("UnreadableSysfs", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		fix := breakClassDir(t, f.tree, "drm")

		// The extension is an aid, never a prerequisite for collection.
		f.start(t)
		assert.Contains(t, f.messages(), msgScanFailed)
		// A scan that could not read the whole device tree is no baseline
		assert.Nil(t, f.watch.baseline)

		// The first successful scan becomes the baseline
		fix()
		f.scan(1)
		require.NotNil(t, f.watch.baseline)
		assert.Equal(t, []string{"drm/card0"}, f.watch.baseline.Devices.IDs())
		assert.Contains(t, f.messages(), msgWatching)
		assert.Empty(t, f.host.fatalErrors())
		assert.NotContains(t, f.messages(), msgDeviceSetChanged)

		// From there on it behaves like any other baseline.
		f.tree.addCard("card1", 1)
		f.scan(1)
		assert.Len(t, f.host.fatalErrors(), 1)
	})
}

// TestChangeDetection covers when a difference from the baseline is acted on.
func TestChangeDetection(t *testing.T) {
	t.Run("SettleRequirement", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) { c.SettleScans = 3 })
		f.tree.addCard("card0", 0)
		f.start(t)

		f.tree.addCard("card1", 1)
		f.scan(2)
		assert.NotContains(t, f.messages(), msgDeviceSetChanged)

		f.scan(1)
		assert.Contains(t, f.messages(), msgDeviceSetChanged)
	})

	t.Run("TransientAppearance", func(t *testing.T) {
		// A device that turns up and is gone again before the change settles costs
		// nothing. The process never enumerated it in the first place.
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 3
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		f.start(t)

		f.tree.addCard("card1", 1)
		f.scan(2)
		f.tree.removeSysfsCardSymlink("card1")
		f.tree.setDevNode("card1", 1, false)
		f.scan(3)

		assert.Empty(t, f.host.fatalErrors())
		assert.NotContains(t, f.messages(), msgDeviceSetChanged)

		// The settle counter is reset to start tracking the next change from scratch
		assert.Equal(t, 0, f.watch.pending.Count)
	})

	t.Run("TransientDisappearance", func(t *testing.T) {
		// A device of the baseline that goes away and comes back costs nothing
		// either. Sysman rescans its devices on the DEVICE_ATTACH that ends the flap.
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 3
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		f.tree.addCard("card1", 1)
		f.start(t)

		// card1 goes away for a brief moment, short of settling
		f.tree.setDevNode("card1", 1, false)
		f.scan(2)
		f.tree.setDevNode("card1", 1, true)
		f.scan(3)

		assert.Empty(t, f.host.fatalErrors())
		assert.NotContains(t, f.messages(), msgDeviceSetChanged)
		assert.Zero(t, f.count(msgDeviceSetStale))
		assert.Equal(t, 0, f.watch.pending.Count)
	})

	t.Run("DriverRebind", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) { c.SettleScans = 1 })
		f.tree.addCard("card0", 0)
		f.tree.setSysfsDriver(0, "i915")
		f.start(t)
		require.NotNil(t, f.watch.baseline)

		f.tree.setSysfsDriver(0, "xe")
		f.scan(1)

		require.Equal(t, 1, f.count(msgDeviceSetChanged))
		fields := f.logs.FilterMessage(msgDeviceSetChanged).All()[0].ContextMap()
		assert.Equal(t, []any{"drm/card0"}, fields["changed"])
		assert.Empty(t, fields["appeared"])
		assert.Empty(t, fields["disappeared"])
	})
}

// TestDeviceSetStaleReport covers a persistent warning as long as we differ from the baseline.
func TestDeviceSetStaleReport(t *testing.T) {
	t.Run("FurtherChangeKeepsItStanding", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) { c.SettleScans = 2 })
		f.tree.addCard("card0", 0)
		f.start(t)

		f.tree.addCard("card1", 1)
		f.scan(2)
		require.Equal(t, 1, f.count(msgDeviceSetStaleRestart))

		// A further change restarts the settle counters but the first
		// condition is still standing (until the new set settles)
		f.tree.addCard("card2", 2)
		f.scan(1)
		assert.Zero(t, f.count(msgDeviceSetRestored))
		assert.Equal(t, 1, f.count(msgDeviceSetStaleRestart))

		// Settled on a different set, so the warning is repeated for that set.
		f.scan(1)
		assert.Equal(t, 2, f.count(msgDeviceSetStaleRestart))

		// Return to the startup set resolves the condition
		f.tree.removeSysfsCardSymlink("card1")
		f.tree.setDevNode("card1", 1, false)
		f.tree.removeSysfsCardSymlink("card2")
		f.tree.setDevNode("card2", 2, false)
		f.scan(1)
		assert.Equal(t, 1, f.count(msgDeviceSetRestored))
	})

	t.Run("DescribesTheSettledSet", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 3
			c.ReportInterval = 10 * time.Minute
		})
		now := time.Date(2026, 8, 25, 12, 0, 0, 0, time.UTC)
		f.watch.reporter.now = func() time.Time { return now }

		f.tree.addCard("card0", 0)
		f.start(t)
		f.tree.addCard("card1", 1)
		f.scan(3)
		require.Equal(t, 1, f.count(msgDeviceSetStaleRestart))

		// A further change, still short of settling, and the report falls due
		f.tree.addCard("card2", 2)
		f.scan(1)
		now = now.Add(11 * time.Minute)
		f.scan(1)

		require.Equal(t, 2, f.count(msgDeviceSetStaleRestart))
		entries := f.logs.FilterMessage(msgDeviceSetStaleRestart).All()
		assert.Equal(t, []any{"drm/card1"}, entries[len(entries)-1].ContextMap()["appeared"])
	})
}

// TestDeviceConditions covers the device node states that are reported but not exited over.
func TestDeviceConditions(t *testing.T) {
	t.Run("Denied", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		f.start(t)

		// A restart would find the device just as inaccessible
		f.tree.addCard("card1", 1)
		f.tree.setProbeResult("card1", unix.EPERM)
		f.scan(5)

		assert.Contains(t, f.messages(), msgDevicesDenied)
		assert.Contains(t, f.messages(), msgDeviceSetChanged)
		assert.Equal(t, 1, f.count(msgChangeNotRestartable))
		assert.Empty(t, f.host.fatalErrors())
	})

	t.Run("StaleNode", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) { c.SettleScans = 1 })
		f.tree.addCard("card0", 0)
		f.start(t)

		// A node left behind for a device that is no longer in sysfs.
		f.tree.addCard("card1", 1)
		f.tree.removeSysfsCardSymlink("card1")
		f.scan(1)
		assert.Contains(t, f.messages(), msgNodesStale)
	})

	t.Run("BlockedNode", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) { c.SettleScans = 1 })
		f.tree.addCard("card0", 0)
		f.start(t)

		// Something that is not a device node holds the name of a device that is in sysfs.
		f.tree.addSysfsCard("card1", 1)
		f.tree.addDevPlainFile("card1")
		f.scan(1)

		assert.Contains(t, f.messages(), msgNodesBlocked)
		assert.NotContains(t, f.messages(), msgNodesMissing)
		assertDeviceGauge(t, f, sysdev.DevNodeStateBlocked, 1)
	})
}

// TestRestartable covers which settled differences are worth exiting over.
func TestRestartable(t *testing.T) {
	inventory := func(state sysdev.DevNodeState) sysdev.Inventory {
		return sysdev.Inventory{{Subsystem: "drm", Name: "card1", State: state}}
	}
	appeared := sysdev.InventoryDiff{Added: []string{"drm/card1"}}

	// A usable device, and one whose node a restart can bring in
	assert.True(t, restartable(appeared, inventory(sysdev.DevNodeStateOK)))
	assert.True(t, restartable(appeared, inventory(sysdev.DevNodeStateMissing)))

	// A device a restart would find exactly as it is
	assert.False(t, restartable(appeared, inventory(sysdev.DevNodeStateDenied)))
	assert.False(t, restartable(appeared, inventory(sysdev.DevNodeStateBlocked)))

	// A device gone from the inventory needs re-enumeration whatever the rest is
	assert.True(t, restartable(sysdev.InventoryDiff{Removed: []string{"drm/card0"}},
		inventory(sysdev.DevNodeStateDenied)))
}

// TestExitRestart covers the exit action and the rate limit guarding it.
func TestExitRestart(t *testing.T) {
	t.Run("ExitsDescribingTheChange", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		f.tree.addCard("card1", 1)
		f.start(t)

		// One device replaced by another, so the error has to name both directions.
		f.tree.addCard("card2", 2)
		f.tree.removeSysfsCardSymlink("card1")
		f.tree.setDevNode("card1", 1, false)
		f.scan(1)

		errs := f.host.fatalErrors()
		require.Len(t, errs, 1)
		assert.Equal(t, fmt.Sprintf(errDeviceSetChanged, "drm/card2", "drm/card1", ""), errs[0].Error())
		assert.Contains(t, f.messages(), msgShuttingDown)
	})

	t.Run("ExitsOnNodeGone", func(t *testing.T) {
		// The node of a device sysfs still lists is what a restart re-creates.
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		f.tree.addCard("card1", 1)
		f.start(t)

		f.tree.setDevNode("card1", 1, false)
		f.scan(1)

		errs := f.host.fatalErrors()
		require.Len(t, errs, 1)
		assert.Equal(t, fmt.Sprintf(errDeviceSetChanged, "", "", "drm/card1"), errs[0].Error())
	})

	t.Run("RestartThatBringsNothing", func(t *testing.T) {
		// A device directory fixed for the container's lifetime (device plugin or
		// DRA): the GPU that appeared has no node there, and a restart brings none.
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		f.start(t)

		f.tree.addSysfsCard("card1", 1)
		f.scan(1)
		require.Len(t, f.host.fatalErrors(), 1)

		// The restarted process adopts the very same devices, and has to say so
		next := f.restart(t)
		next.start(t)
		assert.Contains(t, next.messages(), msgExitIneffective)
		assert.NotContains(t, next.messages(), msgDeviceSetChanged)

		next.scan(3)
		assert.Empty(t, next.host.fatalErrors())
	})

	t.Run("RestartThatReEnumerates", func(t *testing.T) {
		// A restart that did what it was asked for finds the same set too: the nodes
		// were there all along, only sysman had not enumerated them.
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		f.start(t)

		f.tree.addCard("card1", 1)
		f.scan(1)
		require.Len(t, f.host.fatalErrors(), 1)

		next := f.restart(t)
		next.start(t)
		assert.NotContains(t, next.messages(), msgExitIneffective)
	})

	t.Run("MaxRestartsZero", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
			c.MaxRestarts = 0
		})
		f.tree.addCard("card0", 0)
		f.start(t)

		f.tree.addCard("card1", 1)
		f.scan(1)
		assert.Empty(t, f.host.fatalErrors())
		assert.True(t, f.watch.exitDisabled)
		assert.Contains(t, f.messages(), msgRestartsLimitZeroReached)
		// Reported all the same, just not acted on.
		assert.Contains(t, f.messages(), msgDeviceSetChanged)
	})

	t.Run("RestartRateLimit", func(t *testing.T) {
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
			c.MaxRestarts = 2
		})
		f.tree.addCard("card0", 0)
		f.start(t)

		// Each new device is a fresh settled change, so each one asks to exit
		steps := []struct {
			card      string
			minor     uint32
			wantFatal int
		}{
			{"card1", 1, 1},
			{"card2", 2, 2},
			{"card3", 3, 2},
			{"card4", 4, 2},
		}
		for _, s := range steps {
			f.tree.addCard(s.card, s.minor)
			f.scan(1)
			require.Len(t, f.host.fatalErrors(), s.wantFatal)
		}

		assert.Contains(t, f.messages(), msgRestartLimitReached)
		assert.True(t, f.watch.exitDisabled)
	})

	t.Run("UnrecordableExit", func(t *testing.T) {
		if os.Geteuid() == 0 {
			t.Skip("root ignores directory permissions")
		}
		dir := t.TempDir()
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
			c.StateFile = filepath.Join(dir, "restarts.json")
		})
		f.tree.addCard("card0", 0)
		f.start(t)

		require.NoError(t, os.Chmod(dir, 0o500))
		t.Cleanup(func() { require.NoError(t, os.Chmod(dir, 0o700)) })

		f.tree.addCard("card1", 1)
		f.scan(1)

		assert.Empty(t, f.host.fatalErrors())
		assert.Contains(t, f.messages(), msgStateSaveFailed)
		// Reported all the same, just not acted on.
		assert.Contains(t, f.messages(), msgDeviceSetChanged)
		assertSuppressedExits(t, f, reasonStateError, 1)
		assertSuppressedExits(t, f, reasonMaxRestarts, 0)
		assertCounter(t, f, metricExitsRequested, 0)
		// A write that fails once is no reason to stop trying.
		assert.False(t, f.watch.exitDisabled)

		// Once the state file is writable again, the same device set is retried.
		require.NoError(t, os.Chmod(dir, 0o700))
		f.scan(1)
		assert.Len(t, f.host.fatalErrors(), 1)
		assertCounter(t, f, metricExitsRequested, 1)
	})
}

// TestScanError covers a scan that could not read the whole device tree.
func TestScanError(t *testing.T) {
	t.Run("IsNoChange", func(t *testing.T) {
		// Run in exit mode to test that a scan error does not trigger the action
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.ChangeAction = ActionExit
		})
		f.tree.addCard("card0", 0)
		f.start(t)
		fix := breakClassDir(t, f.tree, "drm")

		f.scan(3)
		assert.Contains(t, f.messages(), msgScanFailed)
		// It must neither settle nor exit
		assert.Empty(t, f.host.fatalErrors())
		assert.Empty(t, f.watch.pending.Fingerprint)
		assert.Equal(t, 0, f.watch.pending.Count)
		assert.NotContains(t, f.messages(), msgDeviceSetChanged)

		// The next scan that succeeds and triggers an exit
		fix()
		f.tree.addCard("card1", 1)
		f.scan(1)
		assert.Len(t, f.host.fatalErrors(), 1)
	})

	t.Run("HoldsConditions", func(t *testing.T) {
		// A failed scan is not evidence that what it could not read has gone away.
		f := newWatchFixture(t, func(c *Config) { c.SettleScans = 1 })
		f.tree.addCard("card0", 0)
		f.tree.addCard("card1", 1)
		f.tree.setProbeResult("card1", unix.EPERM)
		f.start(t)
		require.Contains(t, f.messages(), msgDevicesDenied)
		assertDeviceGauge(t, f, sysdev.DevNodeStateDenied, 1)

		breakClassDir(t, f.tree, "drm")
		f.scan(1)
		require.Contains(t, f.messages(), msgScanFailed)
		assert.Zero(t, f.count(msgDevicesAllowed))
		// The gauge keeps its last value, which is stale but true
		assertDeviceGauge(t, f, sysdev.DevNodeStateDenied, 1)
		assertCounter(t, f, metricScanErrors, 1)
	})

	t.Run("StillReportsWhatItRead", func(t *testing.T) {
		// One unreadable subsystem must not silence the ones that did read
		f := newWatchFixture(t, func(c *Config) {
			c.SettleScans = 1
			c.Subsystems = []string{"drm", "mei"}
		})
		f.tree.addCard("card0", 0)
		f.start(t)
		// An absent class directory is not an error, so the baseline is a clean scan.
		require.NotNil(t, f.watch.baseline)

		// MEI is there but unreadable, e.g. not mapped into the container, while DRM keeps working.
		breakClassDir(t, f.tree, "mei")
		f.tree.addCard("card1", 1)
		f.tree.setProbeResult("card1", unix.EPERM)
		f.scan(1)

		require.Contains(t, f.messages(), msgScanFailed)
		assert.Contains(t, f.messages(), msgDevicesDenied)
	})
}

func TestTelemetry(t *testing.T) {
	f := newWatchFixture(t, func(c *Config) {
		c.SettleScans = 1
		c.ChangeAction = ActionExit
		c.MaxRestarts = 1
	})
	f.tree.addCard("card0", 0)
	f.tree.addSysfsCard("card1", 1) // missing node
	f.tree.addCard("card2", 2)
	f.tree.setProbeResult("card2", unix.EPERM) // denied
	f.start(t)

	assertDeviceGauge(t, f, sysdev.DevNodeStateOK, 1)
	assertDeviceGauge(t, f, sysdev.DevNodeStateMissing, 1)
	assertDeviceGauge(t, f, sysdev.DevNodeStateDenied, 1)
	// States that do not occur are reported as zero rather than left out
	assertDeviceGauge(t, f, sysdev.DevNodeStateMismatch, 0)
	assertDeviceGauge(t, f, sysdev.DevNodeStateStale, 0)
	assertDeviceGauge(t, f, sysdev.DevNodeStateOrphan, 0)
	assertDeviceGauge(t, f, sysdev.DevNodeStateBlocked, 0)
	assertDeviceGauge(t, f, sysdev.DevNodeStateError, 0)

	assertCounter(t, f, metricScans, 1)
	assertCounter(t, f, metricScanErrors, 0)

	f.tree.addCard("card3", 3)
	f.scan(1)
	assertCounter(t, f, metricScans, 2)
	assertCounter(t, f, metricDeviceChanges, 1)
	assertCounter(t, f, metricExitsRequested, 1)

	f.tree.addCard("card4", 4)
	f.scan(1)
	assertCounter(t, f, metricExitsSuppressed, 1)
	assertSuppressedExits(t, f, reasonMaxRestarts, 1)
}
