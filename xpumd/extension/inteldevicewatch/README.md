# Intel Device Watch Extension

## Overview

The Intel Device Watch extension detects that the set of GPUs the collector can
use no longer matches the one it started with, and reports it.

Its intention is to handle the following problems:

1. The Level-Zero Sysman API enumerates devices once, at `zesInit`. Devices that
appear or disappear after initialization are not detected (re-init does not re-enumerate).
2. Inside a container, `/dev` is a copy of the host's, populated when the
container is created, and it does not track devices added to (or removed from)
the host afterwards. Sysfs is different as the container engines share (a subset
of) the host's sysfs content with the container, so devices that appear on the
node are visible there.

This extension periodically compares the sysfs device inventory against the
device files (the device nodes under `/dev`) and classifies each device:

| State | Meaning | Fixed by |
|-------|---------|----------|
| `ok` | device file present and openable read-write | — |
| `missing` | in sysfs, no device file | container restart |
| `mismatch` | device file present, wrong device number | container restart |
| `stale` | device file matches sysfs, but `open()` fails with `ENODEV`/`ENXIO` | nobody |
| `orphan` | device file present, device absent from sysfs | container restart |
| `blocked` | the device file's name is taken by an entry that is not a character device | nobody (remove the entry manually) |
| `denied` | device file present, read-write `open()` fails with `EPERM`/`EACCES` | recreate the pod (the `devices` cgroup is fixed at admission) |
| `error` | device file could not be probed | — |

The container runtime creates the device files when it starts the container, which
is why a restart is what fixes the states that are fixable at all.

A difference from the baseline is acted on only once `settle_scans` consecutive
scans agree on it. However, `change_action: exit` only exits if the change is
determined "fixable". E.g. device files that are present but cannot be opened
are ignored in this regard.

A device that disappears and comes back before the change settles costs nothing:
the process either never enumerated it, or Sysman rescans its devices on the
`DEVICE_ATTACH` event that ends the flap.

> **NOTE:** To work around problems in the Level-Zero Sysman driver, the
> extension currently tracks also the "generation" of the device. I.e. device
> that is destroyed and recreated identical (e.g. driver unbind/bind) is treated
> as a change, and in `change_action: exit` mode costs a restart.

## Limitations

- The baseline is a sysfs scan at `Start` of this extension, not guaranteed to
  match what Sysman actually enumerated at `zesInit`. A device that flaps right
  at startup can go unmonitored for the lifetime of the process.
- With no devices at startup the baseline is empty, so the first device to appear
  costs a restart. Postponing `zesInit` until there is a device to enumerate
  would avoid that (TODO).
- A configured `state_file` that cannot be written suppresses the exit: an exit
  that goes uncounted cannot be rate limited.
- `denied` devices are never fixed automatically. The `devices` cgroup is fixed
  at pod admission, so only recreating the pod may help.
- Whether a restart makes device accessible within a container cannot be known in advance,
  so an inaccessible device file costs one container restart before that is known.
- Once `max_restarts` limit fires, `change_action: exit` falls back to
  logging only for the remaining process lifetime, not just until
  `restart_window` passes.
- Orphaned devices are not vendor filtered, because vendor is unknown once
  sysfs no longer lists the device. So a device file left behind by a removed
  device of another vendor is reported as an `orphan` and  may trigger an exit.

## Configuration

### Example Configuration

```yaml
extensions:
  intel_device_watch:
    scan_interval: 30s
    settle_scans: 2
    change_action: exit
    report_interval: 10m
    max_restarts: 3
    restart_window: 10m
    state_file: /var/lib/xpumd/device-watch/restarts.json

service:
  extensions: [intel_device_watch]
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `scan_interval` | duration | `30s` | How often sysfs is compared against the device files. |
| `settle_scans` | int | `2` | Consecutive scans that must agree before a change is acted on. Must be at least 1 |
| `change_action` | enum | `log` | `log` only reports the change. `exit` additionally shuts the collector down gracefully (exit code 0) |
| `report_interval` | duration | `10m` | How often a persisting condition is re-logged. `0` logs each condition only when it appears or changes |
| `max_restarts` | int | `3` | Number of restarts allowed within `restart_window` before falling back to logging only, `0` effectively disables exiting |
| `restart_window` | duration | `10m` | Period over which `max_restarts` is counted |
| `state_file` | string | *empty* | Where the restart count is persisted, empty disables restart rate limiting |
| `subsystems` | list | `[drm, mei]` | sysfs device classes to watch: `drm`, `mei` |
| `vendor_ids` | list | `["8086"]` | PCI vendor IDs to watch, without the `0x` prefix, empty accepts every vendor |
| `sysfs_root` | string | `/sys` | sysfs mount point. Mainly for testing |
| `dev_root` | string | `/dev` | Root of the device file tree. Mainly for testing |

## Internal Telemetry

See [documentation.md](documentation.md) for the emitted metrics.
